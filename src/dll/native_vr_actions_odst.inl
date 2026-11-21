// ODST's kit consumer uses Reload 0x2A, distinct from generic Use 3.
// This is independently matched ODST evidence; H3 action 0x26 is unrelated here.
namespace native_odst {
constexpr auto title=GameTitle::Halo3ODST;
constexpr unsigned readerIndex=3;
constexpr uintptr_t converterRva=0x1A0E30;
constexpr const char* converterPattern=
    "48 8B C4 48 89 58 08 48 89 68 10 48 89 70 18 57 41 54 41 55 41 56 41 57 48 81 EC 90 02 00 00 "
    "49 8B D8 0F 29 70 C8 4C 8B 84 24 E8 02 00 00 45 33 FF 0F 29 78 B8 49 8B F9 8B F2 8B E9";
constexpr uintptr_t controllerProofRva=0x1A0FE7;
constexpr const char* controllerProofPattern=
    "48 8D 54 24 30 8B CD E8 9D 88 01 00 0F B7 84 24 4A 02 00 00 0F 57 D2";
constexpr uintptr_t consumerBegin=0x10FBFC,consumerEnd=0x111F5F,consumerReloadRva=0x111CBF;
constexpr const char* consumerEntryPattern=
    "48 8B C4 F3 0F 11 58 20 F3 0F 11 50 18 89 50 10 89 48 08 55 53 56 57 41 54 41 55 41 56 41 57 "
    "48 8D A8 48 F7 FF FF 48 81 EC 78 09 00 00";
constexpr const char* consumerReloadPattern=
    "BA 2A 00 00 00 48 8B CE E8 54 67 0A 00 48 8B C8 E8 28 D8 FF FF 8B 4F 2C 84 C0 74 06 0F BA E9 0C EB 04 "
    "0F BA F1 0C 89 4F 2C BA 2A 00 00 00 48 8B CE E8 2B 67 0A 00";
// ODST'S PROMPT AND MAP STATE (vr_mapping::OdstAction; docs/ODST-PROMPT-AND-MAP-EVIDENCE-2026-11-15.md). The tutorial
// scripts' unit_confirm_message / unit_confirm_cancel_message set the controller's action-test record's confirm and
// cancel prompt bytes; the game clears them once the answer's test fires. The record's tests word holds bit 3 while the
// controller's PDA, the VISR map, is up. Where all of that is in this build is resolved by unique signatures and
// cross-checked (src/common/odst_prompt_resolve.h; no recorded address); it is read on the game thread, where the
// control builder reads accept for controller 0 -- once a tick -- and published for the input thread. Read only.
struct PromptState {
    std::atomic<bool> proved{};
    const uint32_t* tlsIndex{};
    uintptr_t readReturn{};          // the reader's return address at the builder's read of accept
    uint32_t block{}, confirmFlag{}; // the records' TLS block offset; the confirm byte (cancel follows it)
    uint32_t pdaBit{};               // the PDA's bit in the tests word
    // Bits 0-3: the four prompt bytes (confirm, cancel, and the two after them) set; bit 4: the PDA up.
    std::atomic<uint32_t> bits{}, generation{};
    std::atomic<uint64_t> stampMs{}, faults{};
} prompt;
struct Runtime {
    HMODULE module{};
    uintptr_t base{}, state{};
    std::atomic<uint32_t> generation{};
    // 0 the converter, 1 the reader, 2 the chud's text setter (TitleText), 3 the HUD message text (HudText); 2 and 3
    // are optional: the bridge stands without them
    void* target[4]{}, *original[4]{};
    bool enabled[4]{}, attempted{};
    std::atomic<bool> admitted{}, faulted{};
    std::atomic<uint32_t> callbacks{};
    std::atomic<uint64_t> converted{}, routed{}, readerAt{};
    uint64_t reportAt{};
} runtime;
uint32_t blockedGeneration{}; // worker-only: a fault is not retried every poll
struct ThreadState {
    uint32_t generation{};
    uint64_t publishedAt{}, converterAt{}, cancelEpoch{};
    uint32_t actions{};
    uint32_t blocked{};
    bool direct{}, converted{};
    native_action_button::Record records[vr_mapping::Count]{};
    ExtraRecords extra;
    void Reset(uint32_t next) noexcept {
        generation=next; publishedAt=converterAt=cancelEpoch=0;
        actions=0;direct=converted=false;
        for(unsigned i=0;i<vr_mapping::Count;++i)
            records[i].Reset(title,native_action_button::ConsumerAction(title,static_cast<vr_mapping::Action>(i)));
        extra.Reset(title);
    }
} thread_local local;
bool Current() noexcept {
    return runtime.admitted.load(std::memory_order_acquire) &&
        !runtime.faulted.load(std::memory_order_relaxed) && (g_config.vr_action_mapping||Config_GestureOnlyWeaponInput()) &&
        TitleAdapter_GetActiveTitle()==title &&
        TitleAdapter_GetGeneration(title)==runtime.generation;
}
using Convert = void(__fastcall*)(uintptr_t,int,const void*,void*,void*,void*);
using Read = void*(__fastcall*)(void*,unsigned);

void __fastcall ConverterHook(uintptr_t controller,int elapsedMs,const void* raw,
    void* output,void* axes,void* activity) {
    runtime.callbacks.fetch_add(1,std::memory_order_acq_rel);
    __try {
        reinterpret_cast<Convert>(runtime.original[0])(controller,elapsedMs,raw,output,axes,activity);
        const auto now=GetTickCount64();
        const auto cancelEpoch=publication.cancelEpoch.load(std::memory_order_acquire);
        uint32_t actions=0,blocked=0,extras=0,extrasServed=0;bool direct=false;uint64_t publishedAt=0;
        // ODST preserves the real controller argument and uses it to select
        // native preferences, in both direct and copied gamepad routes.
        if(Current() && controller==0 &&
            ReadPublication(now,actions,direct,&publishedAt,title,runtime.generation.load(std::memory_order_acquire),&blocked,
                &extras,&extrasServed) && elapsedMs>=0 && elapsedMs<=1000 &&
            cancelEpoch==publication.cancelEpoch.load(std::memory_order_acquire)) {
            if(local.generation!=runtime.generation) local.Reset(runtime.generation);
            local.publishedAt=publishedAt;local.actions=actions;local.direct=direct;local.blocked=blocked;
            local.cancelEpoch=cancelEpoch;
            local.converterAt=now;local.converted=true;
            for(unsigned i=0;i<vr_mapping::Count;++i)
                local.records[i].Advance((local.direct||(local.blocked&(1u<<i))) && (local.actions&(1u<<i)),unsigned(elapsedMs));
            local.extra.Advance(local.direct,extras,extrasServed,unsigned(elapsedMs));
            runtime.converted.fetch_add(1,std::memory_order_relaxed);
        } else if(controller==0) local.converted=false;
    } __finally {runtime.callbacks.fetch_sub(1,std::memory_order_release);}
}
__declspec(noinline) bool ReadController(uintptr_t state,unsigned& value) noexcept {
    __try {value=*reinterpret_cast<const volatile unsigned*>(state+0x554);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
__declspec(noinline) bool ReadFrames(const void* record,uint8_t& value) noexcept {
    __try {value=*(reinterpret_cast<const volatile uint8_t*>(record)+2);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
void* ReadOwned(void* state,unsigned action,void* native,uintptr_t caller) noexcept {
        const auto now=GetTickCount64();
        // The abstraction updater also calls the reader to WRITE keyboard
        // counters. It must never receive a private VR record. Admit only the
        // independently matched native player-control builder's read calls.
#if HALOMCCVR_DEV_DIAGNOSTICS
        // SEATEXIT witness: the proven player-control builder reading the Use (3) or Reload consumer record --
        // the game's own view of that button, whatever fed it. Read-only, SEH-guarded, dev builds only.
        if(runtime.admitted.load(std::memory_order_acquire) && caller>=runtime.base+consumerBegin &&
           caller<runtime.base+consumerEnd && reinterpret_cast<uintptr_t>(state)==runtime.state &&
           (action==3u || action==native_action_button::ConsumerAction(title,vr_mapping::Reload)))
            WitnessNativeRecord(title,action,native);
#endif
        if(!Current() || caller<runtime.base+consumerBegin || caller>=runtime.base+consumerEnd ||
            reinterpret_cast<uintptr_t>(state)!=runtime.state)return native;
        unsigned controller=0;
        if(!ReadController(runtime.state,controller)){runtime.faulted=true;return native;}
        if(controller!=0 ||
            local.generation!=runtime.generation || !local.converted ||
            !Fresh(local.converterAt,now)||!Fresh(local.publishedAt,now)||!publication.stamp.load(std::memory_order_acquire)||
            publication.title.load(std::memory_order_acquire)!=title ||
            publication.generation.load(std::memory_order_acquire)!=local.generation ||
            local.cancelEpoch!=publication.cancelEpoch.load(std::memory_order_acquire)) return native;
        runtime.readerAt.store(now,std::memory_order_release);
        if(auto* extra=local.direct?local.extra.Pressed(title,action):nullptr) {
            if(native) {
                uint8_t frames=0;
                if(!ReadFrames(native,frames)){runtime.faulted=true;return native;}
                if(frames)return native;
            }
            runtime.routed.fetch_add(1,std::memory_order_relaxed);
            return extra->bytes.data();
        }
        const int semantic=native_action_button::SemanticAction(title,action);
        if(semantic<0) return native;
        auto& record=local.records[semantic];
        if(local.blocked&(1u<<semantic)) {
            runtime.routed.fetch_add(1,std::memory_order_relaxed);
            return record.bytes.data();
        }
        if(!local.direct) return native;
        const auto virtualState=record.ReadCounters();
        if(!virtualState.frames) return native;
        // Native keyboard/gamepad holds retain their own counters, amount and
        // consumption record. Virtual input never edits or clears that record.
        if(native) {
            uint8_t frames=0;
            if(!ReadFrames(native,frames)){runtime.faulted=true;return native;}
            if(frames)return native;
        }
        runtime.routed.fetch_add(1,std::memory_order_relaxed);
        return record.bytes.data();
}
// The game thread, at the builder's read of accept for controller 0's row: the action-test record's prompt bytes and PDA
// bit (PromptState). ODST runs only in MCC, never lifted, so the engine TLS array is gs:[0x58].
__declspec(noinline) void PublishPromptState(const void* state) noexcept {
    if(reinterpret_cast<uintptr_t>(state)!=runtime.state || !prompt.tlsIndex) return;
    uint32_t bits=0;
    __try {
        auto** slots=reinterpret_cast<void**>(__readgsqword(0x58));
        const uint32_t index=*prompt.tlsIndex;
        const auto* tls=slots&&index<0x200?static_cast<const uint8_t*>(slots[index]):nullptr;
        const auto* record=tls?*reinterpret_cast<const uint8_t* const*>(tls+prompt.block):nullptr;
        if(!record) return;
        const uint32_t flags=*reinterpret_cast<const volatile uint32_t*>(record+prompt.confirmFlag);
        const uint64_t tests=*reinterpret_cast<const volatile uint64_t*>(record);
        for(unsigned i=0;i<4;++i) if((flags>>(8*i))&0xFFu) bits|=1u<<i;
        if((tests>>prompt.pdaBit)&1u) bits|=0x10u;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        prompt.faults.fetch_add(1,std::memory_order_relaxed);
        return;
    }
    prompt.bits.store(bits,std::memory_order_relaxed);
    prompt.generation.store(runtime.generation.load(std::memory_order_relaxed),std::memory_order_relaxed);
    prompt.stampMs.store(GetTickCount64(),std::memory_order_release);
}
void* __fastcall ReaderHook(void* state,unsigned action,uintptr_t caller) {
    runtime.callbacks.fetch_add(1,std::memory_order_acq_rel);
    void* result=nullptr;
    __try {
        result=reinterpret_cast<Read>(runtime.original[1])(state,action);
        if(prompt.proved.load(std::memory_order_relaxed) && caller==prompt.readReturn)
            PublishPromptState(state);
        result=ReadOwned(state,action,result,caller);
    } __finally {runtime.callbacks.fetch_sub(1,std::memory_order_release);}
    return result;
}
// ODST'S TUTORIAL TEXT IN VR WORDS (odst_prompt_text.h): the chud's text setter, entered through the preserving thunk
// (its callers keep xmm2 live), keeps the rewritten line instead of the game's. Script thread; a line now and then.
struct TitleText {
    wchar_t buffer[odst_prompt_text::kMaxChars+1]{};   // the setter copies it before it returns
    std::atomic<uint32_t> rewritten{}, kept{}, faults{};
    uint32_t reported{};                               // worker: rewritten+kept when last logged
} titleText;
using SetText=void(__fastcall*)(int,const wchar_t*,float);
__declspec(noinline) const wchar_t* TitleTextFor(const wchar_t* text) noexcept {
    __try {
        odst_prompt_text::Controls controls{};
        if(!g_config.vr_action_mapping || !odst_text::Read(controls)) return text;
        if(odst_prompt_text::Rewrite(text,controls,titleText.buffer)) {
            titleText.rewritten.fetch_add(1,std::memory_order_relaxed);
            return titleText.buffer;
        }
        bool pictures=false;
        unsigned n=0;
        for(;text[n]&&n<odst_prompt_text::kMaxChars;++n) pictures|=odst_prompt_text::glyph::Private(text[n]);
        if(pictures) {
            titleText.kept.fetch_add(1,std::memory_order_relaxed);
            odst_text::NoteKept(text);
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        titleText.faults.fetch_add(1,std::memory_order_relaxed);
    }
    return text;
}
void __fastcall TitleTextHook(int user,const wchar_t* text,float seconds) {
    runtime.callbacks.fetch_add(1,std::memory_order_acq_rel);
    __try {
        reinterpret_cast<SetText>(runtime.original[2])(user,text?TitleTextFor(text):text,seconds);
    } __finally {runtime.callbacks.fetch_sub(1,std::memory_order_release);}
}
// Worker: what the text hook did since the last report, and the last line it kept as the game's own.
void ReportTitleText() noexcept {
    const uint32_t rewritten=titleText.rewritten.load(std::memory_order_relaxed);
    const uint32_t kept=titleText.kept.load(std::memory_order_relaxed);
    if(rewritten+kept!=titleText.reported) {
        titleText.reported=rewritten+kept;
        LOG("ODST tutorial text: %u lines said in VR words, %u kept as the game's own%s",rewritten,kept,
            titleText.faults.load(std::memory_order_relaxed)?" (a line could not be read)":"");
    }
    odst_text::ReportKept("");
}
// ODST'S HUD PROMPTS IN VR WORDS ("Press [Back] to open VISR NAV data"): the HUD message text function fills the
// player's 256-character buffer; the line is rewritten in place after it returns. The chud asks for its lines each
// time it draws them, so a line is counted once per showing (the last few lines' sums), not per call.
struct HudText {
    std::atomic<uint32_t> rewritten{}, kept{}, faults{};
    std::atomic<uint32_t> recent[4]{};
    std::atomic<uint32_t> next{};
    uint32_t reported{};
} hudText;
using GetHudMessage=void(__fastcall*)(int,int,wchar_t*);
uint32_t TextSum(const wchar_t* text) noexcept {
    uint32_t sum=2166136261u;
    for(unsigned i=0;text[i]&&i<odst_prompt_text::kMaxChars;++i) sum=(sum^text[i])*16777619u;
    return sum|1u;
}
bool NewShowing(uint32_t sum) noexcept {
    for(auto& seen:hudText.recent) if(seen.load(std::memory_order_relaxed)==sum) return false;
    hudText.recent[hudText.next.fetch_add(1,std::memory_order_relaxed)&3u].store(sum,std::memory_order_relaxed);
    return true;
}
__declspec(noinline) void HudMessageFor(wchar_t* text) noexcept {
    __try {
        if(!text || !text[0]) return;
        odst_prompt_text::Controls controls{};
        if(!g_config.vr_action_mapping || !odst_text::Read(controls)) return;
        wchar_t said[odst_prompt_text::kMaxChars+1];
        const unsigned n=odst_prompt_text::Rewrite(text,controls,said);
        const bool fresh=NewShowing(TextSum(text));
        if(n) {
            static_assert(odst_prompt_text::kMaxChars+1<=odst_prompt_text::kHudMessageChars);
            std::memcpy(text,said,(static_cast<size_t>(n)+1)*sizeof(wchar_t));
            if(fresh) hudText.rewritten.fetch_add(1,std::memory_order_relaxed);
            return;
        }
        if(!fresh) return;
        for(unsigned i=0;text[i]&&i<odst_prompt_text::kMaxChars;++i)
            if(odst_prompt_text::glyph::Private(text[i])) {
                hudText.kept.fetch_add(1,std::memory_order_relaxed);
                odst_text::NoteKept(text);
                break;
            }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        hudText.faults.fetch_add(1,std::memory_order_relaxed);
    }
}
// Entered through the preserving thunk: user, string id, the native caller, the buffer (the native r8).
void __fastcall HudMessageHook(int user,int stringId,uintptr_t,wchar_t* buffer) {
    runtime.callbacks.fetch_add(1,std::memory_order_acq_rel);
    __try {
        reinterpret_cast<GetHudMessage>(runtime.original[3])(user,stringId,buffer);
        HudMessageFor(buffer);
    } __finally {runtime.callbacks.fetch_sub(1,std::memory_order_release);}
}
void ReportHudText() noexcept {
    const uint32_t rewritten=hudText.rewritten.load(std::memory_order_relaxed);
    const uint32_t kept=hudText.kept.load(std::memory_order_relaxed);
    if(rewritten+kept!=hudText.reported) {
        hudText.reported=rewritten+kept;
        LOG("ODST HUD prompts: %u lines said in VR words, %u kept as the game's own%s",rewritten,kept,
            hudText.faults.load(std::memory_order_relaxed)?" (a line could not be read)":"");
    }
}
// The reader is entered through a register-preserving asm thunk (native_vr_reader_thunks.asm).
void* const hooks[]{reinterpret_cast<void*>(&ConverterHook),reinterpret_cast<void*>(&NativeVrReaderThunkOdst)};
bool ProveConsumerBounds(uintptr_t base) {
    DWORD64 imageBase=0;
    const auto* entry=RtlLookupFunctionEntry(base+consumerBegin,&imageBase,nullptr);
    return entry && imageBase==base && entry->BeginAddress==consumerBegin && entry->EndAddress==consumerEnd;
}
bool Retire() {
    runtime.admitted.store(false,std::memory_order_release);
    runtime.readerAt.store(0,std::memory_order_release);
    publication.stamp.store(0,std::memory_order_release);
    publication.cancelEpoch.fetch_add(1,std::memory_order_acq_rel);
    prompt.proved.store(false,std::memory_order_release);
    prompt.stampMs.store(0,std::memory_order_release);
    for(unsigned i=0;i<4;++i) if(runtime.enabled[i]) {
        const auto status=MCCVR_DisableHookForRetirement(runtime.target[i]);
        if(status!=MH_OK && status!=MH_ERROR_DISABLED) return false;
        runtime.enabled[i]=false;
    }
    const void* functions[]{hooks[0],hooks[1],reinterpret_cast<void*>(&ReaderHook),reinterpret_cast<void*>(&ReadOwned),
        reinterpret_cast<void*>(&Current),reinterpret_cast<void*>(&ReadController),reinterpret_cast<void*>(&ReadFrames),
        reinterpret_cast<void*>(&PublishPromptState),reinterpret_cast<void*>(&OdstTitleTextThunk),
        reinterpret_cast<void*>(&TitleTextHook),reinterpret_cast<void*>(&TitleTextFor),
        reinterpret_cast<void*>(&OdstHudMessageThunk),reinterpret_cast<void*>(&HudMessageHook),
        reinterpret_cast<void*>(&HudMessageFor)};
    const void* trampolines[]{runtime.original[0],runtime.original[1],nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,
        runtime.original[2],nullptr,nullptr,runtime.original[3],nullptr,nullptr};
    static_assert(sizeof(functions)==sizeof(trampolines));
    if(!WaitForNativeDetourQuiescence(functions,trampolines,14,runtime.callbacks)) return false;
    prompt.tlsIndex=nullptr;
    prompt.readReturn=0;
    for(unsigned i=0;i<4;++i) if(runtime.target[i]) {
        if(MH_RemoveHook(runtime.target[i])!=MH_OK) return false;
        runtime.target[i]=runtime.original[i]=nullptr;
    }
    if(runtime.module) FreeLibrary(runtime.module);
    runtime.module=nullptr;runtime.base=runtime.state=0;runtime.generation=0;
    runtime.readerAt.store(0,std::memory_order_release);
    runtime.attempted=false;runtime.faulted=false;
    return true;
}
bool CanRoute(uint64_t now) noexcept {
    uint32_t actions=0;bool direct=false;
    return Current() && ReadPublication(now,actions,direct,nullptr,title,runtime.generation.load(std::memory_order_acquire)) &&
        Fresh(runtime.readerAt.load(std::memory_order_acquire),now);
}
bool GestureRouting(uint64_t now) noexcept {
    uint32_t actions=0;bool direct=false;
    return Current() && ReadPublication(now,actions,direct,nullptr,title,runtime.generation.load(std::memory_order_acquire)) && direct;
}
void Poll() {
    const auto active=TitleAdapter_GetActiveTitle();
    const auto generation=TitleAdapter_GetGeneration(title);
    if(runtime.module && runtime.faulted.load(std::memory_order_acquire)) {
        blockedGeneration=runtime.generation.load(std::memory_order_acquire);
        if(!Retire()) return;
        LOG("Native VR action bridge ODST: retired faulted optional hooks; transport routing retained");
    }
    if(runtime.module && (active!=title || generation!=runtime.generation))
        if(!Retire()) return;
    if(active!=title || !generation || generation==blockedGeneration || !(g_config.vr_action_mapping||Config_GestureOnlyWeaponInput())) return;
    if(!runtime.attempted) {
        // The preexisting read-only transport resolver must verify the reader
        // before MinHook changes its prologue. Retain that cache so optional
        // bridge failure can return to working layout-aware transport routing.
        vr_mapping::Transports fallback{};
        if(!Game_ReadVrActionBindings(fallback,GetTickCount64())) return;
        const auto* descriptor=TitleRegistry_Find(title);
        if(!descriptor || !GetModuleHandleExW(0,descriptor->moduleName,&runtime.module)) return;
        runtime.attempted=true;runtime.base=reinterpret_cast<uintptr_t>(runtime.module);
        runtime.generation=generation;
        const auto& d=kGestureBindings[readerIndex];
        uintptr_t base=0;size_t size=0;
        bool proved=sig::ModuleRange(descriptor->moduleName,base,size) && base==runtime.base &&
            Prove(base,size,converterRva,converterPattern) && Prove(base,size,d.readerRva,d.readerPattern) &&
            Prove(base,size,d.stateLoadRva,d.statePattern) &&
            Prove(base,size,consumerBegin,consumerEntryPattern) &&
            Prove(base,size,consumerReloadRva,consumerReloadPattern) &&
            Prove(base,size,controllerProofRva,controllerProofPattern) && ProveConsumerBounds(base);
        if(proved) {
            runtime.state=sig::RipTarget(base+d.stateLoadRva+10,base+d.stateLoadRva+14);
            proved=runtime.state>=base && runtime.state-base<size && size-(runtime.state-base)>=4*d.stride;

        }
        const uintptr_t rvas[]{converterRva,d.readerRva};
        for(unsigned i=0;proved&&i<2;++i) {
            auto* target=reinterpret_cast<void*>(base+rvas[i]);
            proved=MH_CreateHook(target,hooks[i],&runtime.original[i])==MH_OK;
            if(proved) {
                runtime.target[i]=target;
                proved=MH_EnableHook(target)==MH_OK;
                runtime.enabled[i]=proved;
            }
        }
        // The prompt and map state (PromptState), found by unique signatures and cross-checked, never by a recorded
        // address (odst_prompt_resolve.h): the builder's read of accept must call the reader just hooked. Anything not
        // found or not agreeing is named, and the rows stay off: every control keeps its usual job.
        prompt.proved.store(false,std::memory_order_release);
        prompt.stampMs.store(0,std::memory_order_release);
        if(proved) {
            drive_resolve::Image image{};
            const char* why=nullptr;
            if(!drive_cascade::OpenModuleImage(base,size,image,why))
                LOG("ODST prompt and map rows: off; halo3odst.dll could not be read (%s)",why?why:"unknown");
            else {
                const auto found=odst_prompt_resolve::Resolve(image,static_cast<uint32_t>(d.readerRva));
                if(found.ok) {
                    prompt.tlsIndex=reinterpret_cast<const uint32_t*>(base+found.tlsIndexRva);
                    prompt.readReturn=base+found.acceptReadReturn;
                    prompt.block=found.block;
                    prompt.confirmFlag=found.confirmFlag;
                    prompt.pdaBit=found.pdaBit;
                    prompt.proved.store(true,std::memory_order_release);
                    LOG("ODST prompt and map rows: the prompt and VISR map state is found (the builder's read of accept "
                        "returns to +0x%X; engine TLS index +0x%X, records at TLS +0x%X, prompt bytes +0x%X/+0x%X, the "
                        "map test bit %u)",found.acceptReadReturn,found.tlsIndexRva,found.block,found.confirmFlag,
                        found.cancelFlag,found.pdaBit);
                }
                else
                    LOG("ODST prompt and map rows: off, normal controls kept; not found or not agreeing: %s",found.missing);
                // ODST's tutorial text in VR words (odst_prompt_text.h): two signatures that must agree. Optional: not
                // found, the game's own words stay and the bridge is unchanged.
                const auto where=odst_prompt_text::Resolve(image);
                if(!where.ok)
                    LOG("ODST tutorial text: the game's own words kept; not found or not agreeing: %s",where.missing);
                else {
                    auto* target=reinterpret_cast<void*>(base+where.setterRva);
                    if(MH_CreateHook(target,reinterpret_cast<void*>(&OdstTitleTextThunk),&runtime.original[2])==MH_OK) {
                        runtime.target[2]=target;
                        runtime.enabled[2]=MH_EnableHook(target)==MH_OK;
                    }
                    LOG("ODST tutorial text: %s (the chud's text setter +0x%X, called by chud_show_cinematic_title at "
                        "+0x%X)",runtime.enabled[2]?"said in VR words":"the game's own words kept, the hook did not install",
                        where.setterRva,where.titleCallRva);
                }
                // ODST's HUD prompts in VR words: one signature, a function start. Optional, as the titles.
                const auto hud=odst_prompt_text::ResolveHudMessage(image);
                if(!hud.rva)
                    LOG("ODST HUD prompts: the game's own words kept; not found: %s",hud.missing);
                else {
                    auto* target=reinterpret_cast<void*>(base+hud.rva);
                    if(MH_CreateHook(target,reinterpret_cast<void*>(&OdstHudMessageThunk),&runtime.original[3])==MH_OK) {
                        runtime.target[3]=target;
                        runtime.enabled[3]=MH_EnableHook(target)==MH_OK;
                    }
                    LOG("ODST HUD prompts: %s (the HUD message text +0x%X)",
                        runtime.enabled[3]?"said in VR words":"the game's own words kept, the hook did not install",hud.rva);
                }
            }
        }
        else
            LOG("ODST prompt and map rows: off, normal controls kept (the ODST action bridge did not install)");
        runtime.readerAt.store(0,std::memory_order_release);
        runtime.admitted.store(proved,std::memory_order_release);
        LOG("Native VR action bridge ODST: %s; requires observed owned input cadence; runtime validation pending",
            proved?"verified hooks installed":"transport fallback (proof/hook failure)");
        if(!proved) {
            blockedGeneration=generation;
            runtime.faulted.store(true,std::memory_order_release);
            (void)Retire();
            return;
        }
    }
    const auto now=GetTickCount64();
    if(now>=runtime.reportAt) {
        runtime.reportAt=now+5000;
        LOG("Native VR action bridge ODST: converter=%llu directReads=%llu%s",
            runtime.converted.load(),runtime.routed.load(),runtime.faulted.load()?" TRANSPORT FALLBACK (guarded access fault)":"");
    }
    if(runtime.enabled[2]) ReportTitleText();
    if(runtime.enabled[3]) ReportHudText();
}

} // namespace native_odst
