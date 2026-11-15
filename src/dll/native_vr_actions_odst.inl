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
// scripts' unit_confirm_message / unit_confirm_cancel_message set bytes +0x1C / +0x1D of the controller's action-test
// record (the engine TLS block +0xD8, a 0x30-byte record per controller); the game clears all four bytes once the
// answer's test fires. The record's tests (+0) hold bit 3 while the controller's PDA, the VISR map, is up. Read here,
// on the game thread, where the control builder reads accept (0x12) for controller 0 -- once a tick -- and published
// for the input thread. Read only; nothing is written.
constexpr uintptr_t promptReadRva=0x10FD11,promptReadCall=0x1A,promptReadReturn=0x1F;
constexpr const char* promptReadPattern=
    "BA 12 00 00 00 48 8B 85 E0 08 00 00 48 03 F1 48 8B CE 48 89 74 24 58 48 89 30 E8 ?? ?? ?? ?? 48 8B C8 "
    "E8 ?? ?? ?? ?? 8B 4F 28 84 C0 74 05 83 C9 20 EB 03 83 E1 DF 89 4F 28 BA 13 00 00 00 48 8B CE E8";
constexpr uintptr_t confirmSiteRva=0x22D1C6,cancelSiteRva=0x22D24E;
constexpr const char* confirmSitePattern=
    "8B 0D ?? ?? ?? ?? 65 48 8B 04 25 58 00 00 00 BA 20 00 00 00 4C 8B 14 C8 41 8B 00 0F B7 C8 4A 8B 14 12 "
    "48 8D 0C 49 48 8B 42 48 48 8B 4C C8 10 8B 89 70 01 00 00 83 F9 FF 74 25 E8 ?? ?? ?? ?? 83 F8 03 77 1B 48 98 "
    "BA D8 00 00 00 48 8D 0C 40 48 C1 E1 04 4A 03 0C 12 C6 41 1C 01 48 83 21 FD";
constexpr const char* cancelSitePattern=
    "8B 0D ?? ?? ?? ?? 65 48 8B 04 25 58 00 00 00 BA 20 00 00 00 4C 8B 14 C8 41 8B 00 0F B7 C8 4A 8B 14 12 "
    "48 8D 0C 49 48 8B 42 48 48 8B 4C C8 10 8B 89 70 01 00 00 83 F9 FF 74 25 E8 ?? ?? ?? ?? 83 F8 03 77 1B 48 98 "
    "BA D8 00 00 00 48 8D 0C 40 48 C1 E1 04 4A 03 0C 12 C6 41 1D 01 48 83 21 FB";
// The tests' update: accept (+0x28 bit 5) is test bit 1, cancel (bit 6) test bit 2; the PDA up (+0x30) is test bit 3.
constexpr uintptr_t promptTestsRva=0x114CCA,pdaTestRva=0x114FB9,promptClearRva=0x11240A;
constexpr const char* promptTestsPattern=
    "41 8B 40 28 A8 20 74 09 4B 83 0C D3 02 41 8B 40 28 A8 40 74 09 4B 83 0C D3 04";
constexpr const char* pdaTestPattern="48 8B C1 48 83 E0 F7 48 83 C9 08 44 38 73 30 48 0F 44 C8 4B 89 0C D3";
constexpr const char* promptClearPattern=
    "45 38 57 1C 74 0F 41 8A 07 D0 E8 41 84 C3 74 05 41 83 67 1C 00 45 38 57 1D 74 10 41 8A 07 C0 E8 02 41 84 C3 "
    "74 05 41 83 67 1C 00";
constexpr uint32_t kActionTestBlock=0xD8,kPromptFlags=0x1C;
struct PromptState {
    std::atomic<bool> proved{};
    const uint32_t* tlsIndex{};
    // Bits 0-3: the four prompt bytes (+0x1C accept, +0x1D cancel, +0x1E, +0x1F) set; bit 4: the PDA up.
    std::atomic<uint32_t> bits{}, generation{};
    std::atomic<uint64_t> stampMs{}, faults{};
} prompt;
struct Runtime {
    HMODULE module{};
    uintptr_t base{}, state{};
    std::atomic<uint32_t> generation{};
    void* target[2]{}, *original[2]{};
    bool enabled[2]{}, attempted{};
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
        const auto* record=tls?*reinterpret_cast<const uint8_t* const*>(tls+kActionTestBlock):nullptr;
        if(!record) return;
        const uint32_t flags=*reinterpret_cast<const volatile uint32_t*>(record+kPromptFlags);
        const uint64_t tests=*reinterpret_cast<const volatile uint64_t*>(record);
        for(unsigned i=0;i<4;++i) if((flags>>(8*i))&0xFFu) bits|=1u<<i;
        if((tests>>3)&1u) bits|=0x10u;
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
        if(caller==runtime.base+promptReadRva+promptReadReturn && prompt.proved.load(std::memory_order_relaxed))
            PublishPromptState(state);
        result=ReadOwned(state,action,result,caller);
    } __finally {runtime.callbacks.fetch_sub(1,std::memory_order_release);}
    return result;
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
    for(unsigned i=0;i<2;++i) if(runtime.enabled[i]) {
        const auto status=MCCVR_DisableHookForRetirement(runtime.target[i]);
        if(status!=MH_OK && status!=MH_ERROR_DISABLED) return false;
        runtime.enabled[i]=false;
    }
    const void* functions[]{hooks[0],hooks[1],reinterpret_cast<void*>(&ReaderHook),reinterpret_cast<void*>(&ReadOwned),
        reinterpret_cast<void*>(&Current),reinterpret_cast<void*>(&ReadController),reinterpret_cast<void*>(&ReadFrames),
        reinterpret_cast<void*>(&PublishPromptState)};
    const void* trampolines[]{runtime.original[0],runtime.original[1],nullptr,nullptr,nullptr,nullptr,nullptr,nullptr};
    if(!WaitForNativeDetourQuiescence(functions,trampolines,8,runtime.callbacks)) return false;
    prompt.tlsIndex=nullptr;
    for(unsigned i=0;i<2;++i) if(runtime.target[i]) {
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
        // The prompt and map state (PromptState): every site at its place and unique, the builder's accept read calling
        // the reader just hooked, and both script functions naming the same engine TLS index.
        if(proved) {
            const bool sites=Prove(base,size,promptReadRva,promptReadPattern) &&
                Prove(base,size,confirmSiteRva,confirmSitePattern) && Prove(base,size,cancelSiteRva,cancelSitePattern) &&
                Prove(base,size,promptTestsRva,promptTestsPattern) && Prove(base,size,pdaTestRva,pdaTestPattern) &&
                Prove(base,size,promptClearRva,promptClearPattern);
            const uintptr_t call=sites?sig::RipTarget(base+promptReadRva+promptReadCall+1,
                base+promptReadRva+promptReadReturn):0;
            const uintptr_t confirmIndex=sites?sig::RipTarget(base+confirmSiteRva+2,base+confirmSiteRva+6):0;
            const uintptr_t cancelIndex=sites?sig::RipTarget(base+cancelSiteRva+2,base+cancelSiteRva+6):0;
            const bool bound=sites&&call==base+d.readerRva&&confirmIndex&&confirmIndex==cancelIndex&&
                confirmIndex>=base&&confirmIndex+4<=base+size;
            prompt.tlsIndex=bound?reinterpret_cast<const uint32_t*>(confirmIndex):nullptr;
            prompt.stampMs.store(0,std::memory_order_release);
            prompt.proved.store(bound,std::memory_order_release);
            LOG("ODST prompt and map rows: %s",bound?"the prompt and VISR map state is read (accept read +0x10FD11, "
                "unit_confirm_message +0x22D1C6, PDA test +0x114FB9)":
                "the prompt and VISR map state did not prove; the ODST prompt and map rows stay off");
        }
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
}

} // namespace native_odst
