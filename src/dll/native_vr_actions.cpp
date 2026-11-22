#include "../common/manual_weapon_input.h"
#include "native_vr_actions.h"
#include "seat_exit_witness.h"
#include "game.h"
#include "title_adapter.h"
#include "sigscan.h"
#include "hook_quiescence.h"
#include "host_unload_gate.h"
#include "../common/native_action_button.h"
#include "../common/config.h"
#include "../common/log.h"
#include "../common/minhook_lifecycle.h"
#include "../common/odst_prompt_resolve.h"
#include "../common/odst_prompt_text.h"
#include "../common/odst_training_reset.h"
#include "drive_cascade.h"
#include "reclaimer_client_image.h"
#include <windows.h>
#include <intrin.h>
#include <MinHook.h>
#include "lifted_minhook.h"
#include <atomic>
#include <cstdio>

// The hooked native READERS are leaves whose LTCG callers keep volatile registers live across the call
// (tools/audit_caller_area.py), which a C++ detour clobbers: Halo 3 then wrote through r10 = 0 at every
// level load. Each reader hook is therefore entered through native_vr_reader_thunks.asm, which saves every
// volatile register but rax and calls the C++ hook through these pointers (defined below).
extern "C" {
void NativeVrReaderThunkReach();
void NativeVrReaderThunkHalo3();
void NativeVrH3ActionEdgesThunk();
void NativeVrReaderThunkOdst();
void NativeVrReaderThunkHalo4();
void OdstTitleTextThunk();
void OdstTitleLookupThunk();
void OdstHudMessageThunk();
void OdstGpBooleanThunk();
}

namespace {
#include "gesture_binding_reader.inl"
// Reach is admitted independently: its kit has distinct generic-use and
// reload consumers (3 and 0x3e). Other titles must prove their own consumer
// separation before enabling this bridge, even when record layouts resemble it.
constexpr auto title = GameTitle::HaloReach;
constexpr unsigned readerIndex = 4;
constexpr uintptr_t converterRva = 0xABDBC;
constexpr const char* converterPattern =
    "48 8B C4 48 89 58 08 48 89 68 10 48 89 70 18 48 89 78 20 41 55 41 56 41 57 "
    "48 8B 4C 24 48 45 33 F6 8B F2 BD 01 00 00 00 66 45 39 70 04 74 03 40 88 29 "
    "41 80 78 06 1E 73 07 41 80 78 07 1E 72 03 40 88 29";
constexpr uintptr_t padProofRva=0x23D00;
constexpr const char* padProofPattern=
    "48 63 C3 48 6B C8 3C 48 8D 05 ?? ?? ?? ?? 47 89 8C 82 B0 03 00 00 48 03 C8 48 8D 57 FB";
constexpr uintptr_t controllerLoopRva=0x23E72;
constexpr const char* controllerLoopPattern=
    "BA 01 00 00 00 48 81 C7 30 01 00 00 03 DA 48 03 F2 83 FB 04 0F 8C 45 FE FF FF";
constexpr uintptr_t consumerBegin=0x5DF80,consumerEnd=0x60F12,consumerReloadRva=0x60C85;
constexpr const char* consumerEntryPattern=
    "48 8B C4 55 53 56 57 41 54 41 55 41 56 41 57 48 8D A8 E8 FD FF FF 48 81 EC D8 02 00 00";
constexpr const char* consumerReloadPattern=
    "BB 3E 00 00 00 48 8B CE 8B D3 E8 DC 03 07 00 48 8B C8 E8 9C C6 FF FF 44 8A C0 8D 53 D2 49 8B CC";
// MCC's jump-in-the-air jetpack: the builder reads jump (action 0) here only after the profile's jump-to-jetpack byte
// (+0x69) and its armor-ability checks pass, and a held jump then fires the jetpack while airborne. Counted only, by
// the read's return address: whether the game ran that check, and whether it saw jump held.
constexpr uintptr_t jetpackJumpRva=0x5FD2D,jetpackJumpReturn=0x5FD43;
constexpr const char* jetpackJumpPattern=
    "38 88 8E 00 00 00 0F 85 A0 00 00 00 33 D2 48 8B CE E8 ?? ?? ?? ?? 48 8B C8 E8 ?? ?? ?? ?? 41 8B 0E 84 C0 0F 95 44 24 31";
// Before that check the builder reads the ability button (action 5) on one of two paths, chosen by the held ability's
// own definition: the jetpack's path (which then tests the profile's byte) or every other ability's plain press.
constexpr uintptr_t jetpackAbilityRva=0x5FCB3,jetpackAbilityReturn=0x5FCBE,plainAbilityRva=0x5FDF8,plainAbilityReturn=0x5FE05;
constexpr const char* jetpackAbilityPattern=
    "8D 57 05 48 8B CE E8 ?? ?? ?? ?? 48 8B C8 E8 ?? ?? ?? ?? 84 C0 48 8B 44 24 78 0F 95 C3 40 38 78 69 0F 84";
constexpr const char* plainAbilityPattern=
    "BA 05 00 00 00 48 8B CE E8 ?? ?? ?? ?? 48 8B C8 E8 ?? ?? ?? ?? BB 01 00 00 00 3A C3";
// The consumer's profile: controller c's at +0x2A03C58 + c*0xAB8 while that profile is in use, else the default one.
constexpr uintptr_t profileLoadRva=0x5E0B6;
constexpr const char* profileLoadPattern=
    "48 69 CE B8 0A 00 00 48 8D 05 ?? ?? ?? ?? 48 8D 98 ?? ?? ?? ?? 48 03 D9 48 8D 3D ?? ?? ?? ?? 48 8B CB E8";
struct Runtime {
    HMODULE module{};
    uintptr_t base{}, state{}, padState{}, profiles{}, defaultProfile{};
    std::atomic<uint32_t> generation{};
    void* target[2]{}, *original[2]{};
    bool enabled[2]{}, attempted{}, jetpackCounted{};
    std::atomic<bool> admitted{}, faulted{};
    std::atomic<uint32_t> callbacks{};
    std::atomic<uint64_t> converted{}, routed{}, readerAt{}, jetpackChecks{}, jetpackHeld{}, jetpackAbility{}, plainAbility{};
    uint64_t reportAt{};
} runtime;
uint32_t blockedGeneration{}; // worker-only: a fault is not retried every poll
struct Publication {
    std::atomic<uint64_t> sequence{}, stamp{}, cancelEpoch{};
    std::atomic<uint32_t> generation{}, actions{}, blocked{}, extras{}, extrasServed{};
    std::atomic<bool> direct{};
    std::atomic<GameTitle> title{GameTitle::None};
} publication;
thread_local bool pollPublished{};
// THE EXTRA ROWS A TITLE'S BRIDGE CARRIES (vr_mapping::ExtraBridgeOnly, NativeVrActions_RoutesExtras): a record per row,
// advanced at the converter with the action rows, and returned at the player-control builder's read of the row's own
// action while it is pressed -- and the game's own button is not held, as the direct route does for an action row.
struct ExtraRecords {
    uint32_t served{};
    native_action_button::Record records[vr_mapping::ExtraCount]{};
    void Reset(GameTitle game) noexcept {
        served=0;
        for(unsigned i=0;i<vr_mapping::ExtraCount;++i)
            records[i].Reset(game,vr_mapping::NativeExtraAction(game,static_cast<vr_mapping::ExtraAction>(i)));
    }
    void Advance(bool direct,uint32_t pressed,uint32_t extrasServed,unsigned elapsedMs) noexcept {
        served=direct?extrasServed:0u;
        for(unsigned i=0;i<vr_mapping::ExtraCount;++i) records[i].Advance(((pressed&served)>>i)&1u,elapsedMs);
    }
    // The record for the builder's read of `action` while its row is pressed, or nullptr: the game's own.
    native_action_button::Record* Pressed(GameTitle game,unsigned action) noexcept {
        for(unsigned i=0;i<vr_mapping::ExtraCount;++i)
            if(((served>>i)&1u)&&vr_mapping::NativeExtraAction(game,static_cast<vr_mapping::ExtraAction>(i))==action)
                return records[i].ReadCounters().frames?&records[i]:nullptr;
        return nullptr;
    }
};
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
            records[i].Reset(title,vr_mapping::NativeAction(title,static_cast<vr_mapping::Action>(i)));
        extra.Reset(title);
    }
} thread_local local;
bool Current() noexcept {
    return runtime.admitted.load(std::memory_order_acquire) &&
        !runtime.faulted.load(std::memory_order_relaxed) && (g_config.vr_action_mapping||Config_GestureOnlyWeaponInput()) &&
        TitleAdapter_GetActiveTitle()==title &&
        TitleAdapter_GetGeneration(title)==runtime.generation;
}
bool Fresh(uint64_t stamp,uint64_t now) noexcept {
    return stamp && now>=stamp && now-stamp<=150;
}
bool ReadPublication(uint64_t now,uint32_t& actions,bool& direct,uint64_t* observedAt=nullptr,
    GameTitle expectedTitle=title,uint32_t expectedGeneration=0,uint32_t* blocked=nullptr,
    uint32_t* extras=nullptr,uint32_t* extrasServed=nullptr) noexcept {
    // Every payload member is atomic; rejected/torn attempts never access
    // native memory. Writers claim an odd sequence with a bounded CAS.
    for(unsigned attempt=0;attempt<3;++attempt) {
        const auto before=publication.sequence.load(std::memory_order_acquire);
        if(before&1) continue;
        const auto stamp=publication.stamp.load(std::memory_order_relaxed);
        const auto generation=publication.generation.load(std::memory_order_relaxed);
        const auto bits=publication.actions.load(std::memory_order_relaxed);
        const auto suppress=publication.blocked.load(std::memory_order_relaxed);
        const auto route=publication.direct.load(std::memory_order_relaxed);
        const auto publishedTitle=publication.title.load(std::memory_order_relaxed);
        const auto extraBits=publication.extras.load(std::memory_order_relaxed);
        const auto served=publication.extrasServed.load(std::memory_order_relaxed);
        if(before!=publication.sequence.load(std::memory_order_acquire)) continue;
        if(publishedTitle!=expectedTitle || generation!=(expectedGeneration?expectedGeneration:
            runtime.generation.load(std::memory_order_acquire))||!Fresh(stamp,now))return false;
        actions=bits;direct=route;if(blocked)*blocked=suppress;if(observedAt)*observedAt=stamp;
        if(extras)*extras=extraBits;if(extrasServed)*extrasServed=served;
        return true;
    }
    return false;
}
using Convert = void(__fastcall*)(uintptr_t,int,const void*,void*,void*,void*);
using Read = void*(__fastcall*)(void*,unsigned);

void __fastcall ConverterHook(uintptr_t unused,int elapsedMs,const void* raw,
    void* output,void* axes,void* activity) {
    runtime.callbacks.fetch_add(1,std::memory_order_acq_rel);
    __try {
        reinterpret_cast<Convert>(runtime.original[0])(unused,elapsedMs,raw,output,axes,activity);
        const auto now=GetTickCount64();
        const auto cancelEpoch=publication.cancelEpoch.load(std::memory_order_acquire);
        uint32_t actions=0,blocked=0,extras=0,extrasServed=0;bool direct=false;uint64_t publishedAt=0;
        // Both official input paths target raw-pad base + controller*0x3c:
        // direct XInput and the MCC host's copied gamepad packet. Output row
        // zero proves ownership even when XInput was polled on another thread.
        // RCX is unused by the optimized converter and is never an owner ID.
        if(Current() && reinterpret_cast<uintptr_t>(output)==runtime.padState &&
            ReadPublication(now,actions,direct,&publishedAt,title,runtime.generation.load(),&blocked,&extras,&extrasServed) &&
            elapsedMs>=0 && elapsedMs<=1000 &&
            cancelEpoch==publication.cancelEpoch.load(std::memory_order_acquire)) {
            if(local.generation!=runtime.generation) local.Reset(runtime.generation);
            local.publishedAt=publishedAt;local.actions=actions;local.direct=direct;local.blocked=blocked;
            local.cancelEpoch=cancelEpoch;
            local.converterAt=now;local.converted=true;
            for(unsigned i=0;i<vr_mapping::Count;++i)
                local.records[i].Advance((local.direct||(local.blocked&(1u<<i))) && (local.actions&(1u<<i)),unsigned(elapsedMs));
            local.extra.Advance(local.direct,extras,extrasServed,unsigned(elapsedMs));
            runtime.converted.fetch_add(1,std::memory_order_relaxed);
        } else if(reinterpret_cast<uintptr_t>(output)==runtime.padState) local.converted=false;
    } __finally {runtime.callbacks.fetch_sub(1,std::memory_order_release);}
}
__declspec(noinline) bool ReadController(uintptr_t state,unsigned& value) noexcept {
    __try {value=*reinterpret_cast<const volatile unsigned*>(state+0x6FC);return true;}
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
// The game's own test of the record it was given (+0x5D31C): not consumed (+3 bit 0), its owner (+0xC) open (0x7F) or
// this context's (+8), then held frames (+2).
__declspec(noinline) void NoteJetpackJump(const void* record) noexcept {
    runtime.jetpackChecks.fetch_add(1,std::memory_order_relaxed);
    if(!record) return;
    __try {
        const auto* bytes=reinterpret_cast<const volatile uint8_t*>(record);
        const int32_t owner=*reinterpret_cast<const volatile int32_t*>(bytes+0xC);
        if(!(bytes[3]&1)&&(owner==0x7F||owner==*reinterpret_cast<const volatile int32_t*>(bytes+8))&&bytes[2])
            runtime.jetpackHeld.fetch_add(1,std::memory_order_relaxed);
    } __except(EXCEPTION_EXECUTE_HANDLER){}
}
// The double-press jump-to-jetpack byte (+0x69) the jetpack's path tests; -1 when unknown or unreadable.
int ProfileByte(uintptr_t profile) noexcept {
    if(!profile) return -1;
    __try { return *reinterpret_cast<const volatile uint8_t*>(profile+0x69); }
    __except(EXCEPTION_EXECUTE_HANDLER) { return -1; }
}
void* __fastcall ReaderHook(void* state,unsigned action,uintptr_t caller) {
    runtime.callbacks.fetch_add(1,std::memory_order_acq_rel);
    void* result=nullptr;
    __try {
        result=reinterpret_cast<Read>(runtime.original[1])(state,action);
        result=ReadOwned(state,action,result,caller);
        if(runtime.jetpackCounted) {
            if(caller==runtime.base+jetpackJumpReturn) NoteJetpackJump(result);
            else if(caller==runtime.base+jetpackAbilityReturn) runtime.jetpackAbility.fetch_add(1,std::memory_order_relaxed);
            else if(caller==runtime.base+plainAbilityReturn) runtime.plainAbility.fetch_add(1,std::memory_order_relaxed);
        }
    } __finally {runtime.callbacks.fetch_sub(1,std::memory_order_release);}
    return result;
}
// The reader is entered through a register-preserving asm thunk (native_vr_reader_thunks.asm).
void* const hooks[]{reinterpret_cast<void*>(&ConverterHook),reinterpret_cast<void*>(&NativeVrReaderThunkReach)};
bool Prove(uintptr_t base,size_t size,uintptr_t rva,const char* pattern) {
    const auto found=sig::Find(base,size,pattern);
    return rva<size && found==base+rva && !sig::Find(found+1,size-(found+1-base),pattern);
}
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
    for(unsigned i=0;i<2;++i) if(runtime.enabled[i]) {
        const auto status=MCCVR_DisableHookForRetirement(runtime.target[i]);
        if(status!=MH_OK && status!=MH_ERROR_DISABLED) return false;
        runtime.enabled[i]=false;
    }
    const void* functions[]{hooks[0],hooks[1],reinterpret_cast<void*>(&ReaderHook),reinterpret_cast<void*>(&ReadOwned),
        reinterpret_cast<void*>(&Current),reinterpret_cast<void*>(&ReadController),reinterpret_cast<void*>(&ReadFrames),
        reinterpret_cast<void*>(&NoteJetpackJump)};
    const void* trampolines[]{runtime.original[0],runtime.original[1],nullptr,nullptr,nullptr,nullptr,nullptr,nullptr};
    if(!WaitForNativeDetourQuiescence(functions,trampolines,8,runtime.callbacks)) return false;
    for(unsigned i=0;i<2;++i) if(runtime.target[i]) {
        if(MH_RemoveHook(runtime.target[i])!=MH_OK) return false;
        runtime.target[i]=runtime.original[i]=nullptr;
    }
    if(runtime.module) FreeLibrary(runtime.module);
    runtime.module=nullptr;runtime.base=runtime.state=runtime.padState=runtime.profiles=runtime.defaultProfile=0;
    runtime.generation=0;
    runtime.readerAt.store(0,std::memory_order_release);
    runtime.attempted=false;runtime.faulted=false;runtime.jetpackCounted=false;
    return true;
}
#if HALOMCCVR_DEV_DIAGNOSTICS
// SEATEXIT witness: one native action record's held counters (native_action_button::Counters: milliseconds at
// +0, frames at +2), read under SEH. Dev builds only.
__declspec(noinline) void WitnessNativeRecord(GameTitle requested,unsigned action,const void* record) noexcept {
    uint16_t milliseconds=0;uint8_t frames=0;bool present=false;
    if(record) {
        __try {
            milliseconds=*reinterpret_cast<const volatile uint16_t*>(record);
            frames=*(reinterpret_cast<const volatile uint8_t*>(record)+2);
            present=true;
        } __except(EXCEPTION_EXECUTE_HANDLER) {present=false;}
    }
    SeatWitness_NoteNativeAction(requested,action,present,frames,milliseconds);
}
#endif
// ODST'S TUTORIAL TEXT IN VR WORDS (odst_prompt_text.h): what presses VISR and each gamepad button, as the input
// thread last read the player's rows and the game's layout (NativeVrActions_PublishOdstTextControls), for the chud
// text hooks on the game's threads. Each input packs as three 5-bit controls (held, held, press), four to a word (VISR,
// then the buttons A B X Y LB RB LT RT); bit 63 of each word: read. A rebind read mid-change mixes two readings for one
// line at most.
namespace odst_text {
constexpr unsigned kItems=1+odst_prompt_text::glyph::kPadButtons, kWords=(kItems+3)/4;
std::atomic<uint64_t> controls[kWords]{};
constexpr uint64_t Pack(const vr_mapping::Input& in) noexcept {
    const auto bits=[](int source){ return static_cast<uint64_t>(source>=0&&source<32?source:vr_mapping::Unbound); };
    return bits(in.held[0])|bits(in.held[1])<<5|bits(in.press)<<10;
}
constexpr vr_mapping::Input Unpack(uint64_t bits) noexcept {
    vr_mapping::Input in{};
    in.held[0]=static_cast<int>(bits&31u);
    in.held[1]=static_cast<int>(bits>>5&31u);
    in.press=static_cast<int>(bits>>10&31u);
    return in;
}
// The last line kept as the game's own (its pictures as <U+....> in the log), from either title's hook: the worker
// names it, so a line the rewrite does not know can be added.
wchar_t lastKept[128]{};
std::atomic<uint32_t> lastKeptSerial{};
uint32_t lastKeptShown=0;
void NoteKept(const wchar_t* text) noexcept {   // the script thread
    unsigned n=0;
    for(;text[n]&&n<127u;++n) lastKept[n]=text[n];
    lastKept[n]=0;
    lastKeptSerial.fetch_add(1,std::memory_order_release);
}
// A line for the log: printable as itself, CR as |, LF as /, other characters (the pictures) as <U+....>.
template<size_t N> void Escape(const wchar_t* text,char (&line)[N]) noexcept {
    size_t used=0;
    for(unsigned i=0;text[i]&&used+10<N;++i) {
        const wchar_t ch=text[i];
        if(ch>=0x20&&ch<0x7F) line[used++]=static_cast<char>(ch);
        else if(ch==L'\r') line[used++]='|';
        else if(ch==L'\n') line[used++]='/';
        else used+=static_cast<size_t>(std::snprintf(line+used,N-used,"<U+%04X>",static_cast<unsigned>(ch)));
    }
    line[used<N?used:N-1]=0;
}
void ReportKept(const char* who) noexcept {   // the worker
    const uint32_t serial=lastKeptSerial.load(std::memory_order_acquire);
    if(serial==lastKeptShown) return;
    lastKeptShown=serial;
    char line[128*9]{};
    Escape(lastKept,line);
    LOG("ODST tutorial text%s: kept the game's own words for \"%s\" (CR as |, LF as /)",who,line);
}
// The last line said in VR words, as the game wrote it and as it was said: the worker names both, so the log shows
// which tutorials and prompts appeared.
wchar_t lastSaidFrom[128]{}, lastSaidTo[256]{};
std::atomic<uint32_t> lastSaidSerial{};
uint32_t lastSaidShown=0;
void NoteSaid(const wchar_t* from,const wchar_t* to) noexcept {   // the script or chud thread
    unsigned n=0;
    for(;from[n]&&n<127u;++n) lastSaidFrom[n]=from[n];
    lastSaidFrom[n]=0;
    for(n=0;to[n]&&n<255u;++n) lastSaidTo[n]=to[n];
    lastSaidTo[n]=0;
    lastSaidSerial.fetch_add(1,std::memory_order_release);
}
void ReportSaid(const char* who) noexcept {   // the worker
    const uint32_t serial=lastSaidSerial.load(std::memory_order_acquire);
    if(serial==lastSaidShown) return;
    lastSaidShown=serial;
    char from[128*9]{}, to[256*2]{};
    Escape(lastSaidFrom,from);
    Escape(lastSaidTo,to);
    LOG("ODST tutorial text%s: said \"%s\" as \"%s\" (CR as |, LF as /)",who,from,to);
}
// The controls the text names, or false while the input thread has not read them (the game's own text stays).
bool Read(odst_prompt_text::Controls& c) noexcept {
    uint64_t packed[kWords]{};
    for(unsigned i=0;i<kWords;++i) {
        packed[i]=controls[i].load(std::memory_order_acquire);
        if(!(packed[i]>>63)) return false;
    }
    const auto item=[&packed](unsigned k){ return Unpack(packed[k/4]>>(15*(k%4))); };
    c.odst=g_config.vr_odst_bindings;
    c.visr=item(0);
    for(unsigned b=0;b<odst_prompt_text::glyph::kPadButtons;++b) c.pad[b]=item(1+b);
    c.leftHanded=g_config.left_handed;
    c.dpadRightHand=g_config.dpad_hand!=0;
    return true;
}
}
#include "native_vr_actions_ce.inl"
#include "native_vr_actions_h3.inl"
#include "native_vr_actions_odst.inl"
#include "native_vr_actions_h4.inl"
#include "native_vr_actions_h2.inl"
}

void NativeVrActions_NoteZoomInput(unsigned kind,bool down) {
    using namespace native_h3;
    const uint8_t which=kind==0?kPadButton:kind==1?kMapperZoom:kTrigger;
    if(which==kTrigger&&down) zoomTriggerDownMs.store(GetTickCount64(),std::memory_order_relaxed);
    NoteZoomTick(which,down,0,0);
}

// The last 1.5 s of zoom changes (native_h3::ZoomTick), when the game's zoom changed with no pull just before it, or
// zoomed in and back out within 0.6 s.
void NativeVrActions_Halo3ZoomChanged(int previous,int level) {
    using namespace native_h3;
    static uint64_t zoomedInMs=0;
    static uint32_t written=0;
    const uint64_t now=GetTickCount64();
    const uint64_t pulled=zoomTriggerDownMs.load(std::memory_order_relaxed);
    const bool unprompted=!pulled||now-pulled>150;
    const bool bounced=previous>=0&&level<0&&zoomedInMs&&now-zoomedInMs<=600;
    if(previous<0&&level>=0) zoomedInMs=now;
    if(!(unprompted||bounced)||written>=40) return;
    ++written;
    constexpr const char* kKinds[]{"adv","adv-refused","read-vr","read-game","pad-zoom","mapper-zoom","trigger"};
    const uint32_t next=zoomTickNext.load(std::memory_order_relaxed);
    char line[900];
    int used=std::snprintf(line,sizeof(line),"ZOOM TICKS Halo 3: the game's zoom %d -> %d, %s (the trigger last pulled "
        "%lld ms ago); t(ms) thread kind pressed/frames/gameFrames:",previous,level,
        unprompted?"with no pull just before":"zoomed in and back out",pulled?static_cast<long long>(now-pulled):-1ll);
    for(uint32_t k=0;k<kZoomTicks;++k) {
        const ZoomTick t=zoomTicks[(next+k)%kZoomTicks];
        if(!t.ms||t.ms+1500<now) continue;
        const int n=std::snprintf(line+used,sizeof(line)-used," [%lld %u %s %u/%u/%u]",
            static_cast<long long>(t.ms)-static_cast<long long>(now),t.thread,kKinds[t.kind<7?t.kind:0],t.pressed,
            t.frames,t.gameFrames);
        if(n>0) used+=n;
        if(used>780) {LOG("%s",line);used=std::snprintf(line,sizeof(line),"ZOOM TICKS Halo 3 (cont.):");}
    }
    LOG("%s",line);
}
namespace {
bool CurrentForTitle(GameTitle requested) noexcept {
    if(requested==GameTitle::HaloCE)return native_ce::Current();
    if(requested==GameTitle::Halo3)return native_h3::Current();
    if(requested==GameTitle::Halo3ODST)return native_odst::Current();
    if(requested==GameTitle::Halo4)return native_h4::Current();
    if(requested==GameTitle::Halo2)return native_h2::Current();
    return requested==title&&Current();
}
uint32_t GenerationForTitle(GameTitle requested) noexcept {
    if(requested==GameTitle::HaloCE)return native_ce::runtime.generation.load(std::memory_order_acquire);
    if(requested==GameTitle::Halo3)return native_h3::runtime.generation.load(std::memory_order_acquire);
    if(requested==GameTitle::Halo3ODST)return native_odst::runtime.generation.load(std::memory_order_acquire);
    if(requested==GameTitle::Halo4)return native_h4::runtime.generation.load(std::memory_order_acquire);
    if(requested==GameTitle::Halo2)return native_h2::runtime.generation.load(std::memory_order_acquire);
    return requested==title?runtime.generation.load(std::memory_order_acquire):0;
}
}

extern "C" {
void* (*g_nativeVrReaderReach)(void*,unsigned,uintptr_t)=&ReaderHook;
uintptr_t (*g_nativeVrH3ActionEdges)(uint32_t,void*,uintptr_t)=&native_h3::ActionEdgesHook;
void* (*g_nativeVrReaderHalo3)(void*,unsigned,uintptr_t)=&native_h3::ReaderHook;
void* (*g_nativeVrReaderOdst)(void*,unsigned,uintptr_t)=&native_odst::ReaderHook;
void* (*g_nativeVrReaderHalo4)(void*,unsigned,uintptr_t)=&native_h4::ReaderHook;
void (*g_odstTitleTextHook)(int,const wchar_t*,float)=&native_odst::TitleTextHook;
void (*g_odstHudMessageHook)(int,int,uintptr_t,wchar_t*)=&native_odst::HudMessageHook;
uintptr_t (*g_odstGpBooleanHook)(uint32_t,unsigned,uintptr_t,uintptr_t)=&native_odst::GpBooleanHook;
}
void NativeVrActions_PublishOdstTextControls(const vr_mapping::Input& visr,
    const vr_mapping::Input (&pad)[kOdstTextPadButtons]) noexcept {
    using namespace odst_text;
    static_assert(kOdstTextPadButtons==odst_prompt_text::glyph::kPadButtons);
    uint64_t packed[kWords]{};
    for(unsigned k=0;k<kItems;++k) packed[k/4]|=Pack(k?pad[k-1]:visr)<<(15*(k%4));
    for(unsigned i=0;i<kWords;++i) {
        packed[i]|=1ull<<63;
        if(controls[i].load(std::memory_order_relaxed)!=packed[i]) controls[i].store(packed[i],std::memory_order_release);
    }
}
void NativeVrActions_BeginPoll() noexcept {
    pollPublished=false;
}
void NativeVrActions_EndPoll() noexcept {
    if(!pollPublished) {
        publication.stamp.store(0,std::memory_order_release);
        publication.cancelEpoch.fetch_add(1,std::memory_order_acq_rel);
    }
}
bool NativeVrActions_CanRoute(GameTitle requested,uint64_t now) noexcept {
    if(requested==GameTitle::HaloCE)return native_ce::CanRoute(now);
    if(requested==GameTitle::Halo3)return native_h3::CanRoute(now);
    if(requested==GameTitle::Halo3ODST)return native_odst::CanRoute(now);
    if(requested==GameTitle::Halo4)return native_h4::CanRoute(now);
    if(requested==GameTitle::Halo2)return native_h2::CanRoute(now);
    uint32_t actions=0;bool direct=false;
    return requested==title && Current() && ReadPublication(now,actions,direct) &&
        Fresh(runtime.readerAt.load(std::memory_order_acquire),now);
}
void NativeVrActions_Publish(const void* raw,GameTitle requested,uint64_t now,
    uint32_t actions,bool direct,uint32_t blockedActions,uint32_t extras,uint32_t extrasServed) noexcept {
    const auto generation=GenerationForTitle(requested);
    if(!generation || !CurrentForTitle(requested)) return;
    (void)raw; // converter ownership is the proven native output row, not an input address
    uint64_t before=publication.sequence.load(std::memory_order_acquire);
    bool claimed=false;
    for(unsigned attempt=0;attempt<3&&!claimed;++attempt)
        if(!(before&1)) claimed=publication.sequence.compare_exchange_weak(before,before+1,std::memory_order_acq_rel);
        else before=publication.sequence.load(std::memory_order_acquire);
    if(!claimed)return;
    if(!CurrentForTitle(requested) || generation!=GenerationForTitle(requested)) {
        publication.sequence.store(before+2,std::memory_order_release);
        return;
    }
    publication.generation.store(generation,std::memory_order_relaxed);
    publication.title.store(requested,std::memory_order_relaxed);
    publication.actions.store(actions&((1u<<vr_mapping::Count)-1),std::memory_order_relaxed);
    publication.blocked.store(blockedActions&manual_weapon_input::Blocked(true,true,true),std::memory_order_relaxed);
    publication.direct.store(direct,std::memory_order_relaxed);
    const uint32_t served=NativeVrActions_RoutesExtras(requested)&&direct?extrasServed&((1u<<vr_mapping::ExtraCount)-1):0u;
    publication.extrasServed.store(served,std::memory_order_relaxed);
    publication.extras.store(extras&served,std::memory_order_relaxed);
    publication.stamp.store(now,std::memory_order_relaxed);
    publication.sequence.store(before+2,std::memory_order_release);
    pollPublished=true;
}
// PROJECT RECLAIMER'S ODST PROMPTS (reclaimer_client::FindOdstPrompt): its client runs ODST's prompt natives itself and
// keeps a byte per controller, a bit per waiting prompt (0 confirm, 1 cancel, 2 retrain). Found once, by shape and
// cross-checked, on the worker the first time an ODST level runs on Reclaimer; read from any thread (a plain byte of the
// client's data). Reclaimer's VISR map is its own screen and is not found: there the map rows stay off.
namespace reclaimer_odst_prompt {
std::atomic<uintptr_t> flags{0};
std::atomic<bool> attempted{false};
// The VISR map probe (reclaimer_client::FindOdstPdaProbe): LOG ONLY. Worker-only state.
reclaimer_client::OdstPdaProbe pda{};
uintptr_t clientBase=0;
uint64_t pdaShown=~0ull;
unsigned pdaLines=0;
__declspec(noinline) bool Find(reclaimer_client::OdstPrompt& found,reclaimer_client::OdstPdaProbe& probe) noexcept {
    __try {
        reclaimer_client::Image image;
        if(!reclaimer_client::ClientImageInMemory(image)) return false;
        found=reclaimer_client::FindOdstPrompt(image);
        probe=reclaimer_client::FindOdstPdaProbe(image);
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
// Player 0's PDA records as the client's reader would take them: the game-state record (when its pointers are set)
// and the static one, each's flags word; the snapshot's selecting byte (-1: no snapshot); whether "ready" is set.
struct PdaRead { bool stateRead=false; uint32_t stateFlags=0, staticFlags=0; int snapshot=-1; bool ready=false; };
__declspec(noinline) bool ReadPda(PdaRead& out) noexcept {
    __try {
        const auto u64=[](uintptr_t at) { return *reinterpret_cast<const volatile uint64_t*>(at); };
        const auto u32=[](uintptr_t at) { return *reinterpret_cast<const volatile uint32_t*>(at); };
        out.ready=u64(clientBase+pda.readyGlobal)!=0;
        if(const uintptr_t snapshot=static_cast<uintptr_t>(u64(clientBase+pda.snapshotGlobal)))
            out.snapshot=*reinterpret_cast<const volatile uint8_t*>(snapshot+pda.snapshotField);
        out.staticFlags=u32(clientBase+pda.staticBase+pda.staticOffset);
        const uintptr_t guest=pda.guestDirect?clientBase+pda.guestGlobal:static_cast<uintptr_t>(u64(clientBase+pda.guestGlobal));
        const uintptr_t layout=static_cast<uintptr_t>(u64(clientBase+pda.layoutGlobal));
        const uintptr_t state=guest?static_cast<uintptr_t>(u64(guest+pda.guestOffset)):0;
        if(state&&layout) {
            const uint64_t s0=u64(layout);
            const uint32_t sum=static_cast<uint32_t>(s0)+static_cast<uint32_t>(s0>>32)+pda.add;
            const uintptr_t base=(static_cast<uintptr_t>(sum)+state+u32(layout+8)+static_cast<intptr_t>(pda.sub))&
                ~static_cast<uintptr_t>(0xF);
            out.stateFlags=u32(base+pda.record);
            out.stateRead=true;
        }
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
// "Active" as the client's own pda_is_active_deterministic answers it: bit 5 or bit 11 of the record's flags word.
inline int PdaActive(uint32_t f) noexcept { return (f&0x20u)||(f&0x800u)?1:0; }
// The record the reader asks: the game-state one while the snapshot's byte is 1 and its pointers are set, else the static
// one. The user's run of 2026-11-19 (0.10.6) showed the game-state record's bit 11 set for the VISR map's tutorial and
// clear before and after it, the static record never active: the reader's answer is the map's state.
inline bool PdaMapOpen(const PdaRead& r) noexcept {
    if(!r.ready) return false;
    return r.snapshot==1&&r.stateRead?PdaActive(r.stateFlags)!=0:PdaActive(r.staticFlags)!=0;
}
// Set (release) once `pda` and `clientBase` are filled on the worker; the input thread reads the map only after it.
std::atomic<bool> pdaLive{false};
// Worker, every poll while an ODST level runs on Reclaimer: a line each time what the probe reads changes (at most 120).
void ProbePda() {
    if(!pda.reader||!clientBase||pdaLines>=120||!Game_ReclaimerOdstLevel()) return;
    PdaRead r{};
    const bool read=ReadPda(r);
    const auto active=[](uint32_t f) { return PdaActive(f); };
    const uint64_t shown=read?(uint64_t(r.stateRead?(r.stateFlags&0xFFFu):0xFFFFu)<<32)|(uint64_t(r.staticFlags&0xFFFu)<<8)|
        (uint64_t(r.snapshot&0xFF)<<1)|(r.ready?1u:0u):~1ull;
    if(shown==pdaShown) return;
    pdaShown=shown;
    ++pdaLines;
    if(!read) {LOG("ODST map probe (Project Reclaimer, log only): the PDA records could not be read");return;}
    LOG("ODST map (Project Reclaimer): player 0's PDA, game-state record %s0x%03X (active %d), static record 0x%03X "
        "(active %d); snapshot byte %d (1: the game-state record is the one asked), ready %d: the map is %s",
        r.stateRead?"":"unread ",r.stateFlags&0xFFFu,r.stateRead?active(r.stateFlags):0,r.staticFlags&0xFFFu,
        active(r.staticFlags),r.snapshot,r.ready?1:0,PdaMapOpen(r)?"open":"closed");
}
// Worker: once per process, when an ODST level runs on Reclaimer.
void Poll() {
    if(!g_hostIsProjectReclaimer) return;
    ProbePda();
    if(attempted.load(std::memory_order_relaxed) || !Game_ReclaimerOdstLevel()) return;
    attempted.store(true,std::memory_order_relaxed);
    reclaimer_client::OdstPrompt found{};
    reclaimer_client::OdstPdaProbe probe{};
    if(!Find(found,probe))
        LOG("ODST prompt and map rows (Project Reclaimer): off, normal controls kept; the client executable could not be "
            "read");
    else {
        if(found.missing)
            LOG("ODST prompt and map rows (Project Reclaimer): off, normal controls kept; not found: %s",found.missing);
        else {
            flags.store(reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr))+found.flags,std::memory_order_release);
            LOG("ODST prompt and map rows (Project Reclaimer): its prompts are found (setter exe+0x%X, prompt bytes "
                "exe+0x%X); confirm and reject answer them",found.setter,found.flags);
        }
        if(probe.missing)
            LOG("ODST map rows (Project Reclaimer): off, the right stick and grips keep their jobs in the map; not found: %s",
                probe.missing);
        else {
            pda=probe;
            clientBase=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
            pdaLive.store(true,std::memory_order_release);
            LOG("ODST map rows (Project Reclaimer): the VISR map is read as the client's pda_is_active_deterministic reads "
                "it (reader exe+0x%X, game-state record via guest +0x%X then +0x%X, static record exe+0x%X)",probe.reader,
                probe.guestOffset,probe.record,probe.staticBase+probe.staticOffset);
        }
    }
}
__declspec(noinline) bool Read(uint8_t& value) noexcept {
    const uintptr_t at=flags.load(std::memory_order_acquire);
    if(!at) return false;
    __try { value=*reinterpret_cast<const volatile uint8_t*>(at); return true; }   // controller 0
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
}
bool NativeVrActions_OdstPromptState(uint64_t now,bool& prompt,bool& map,uint32_t* raw) noexcept {
    prompt=map=false;
    if(raw) *raw=0;
    if(g_hostIsProjectReclaimer) {
        uint8_t value=0;
        if(TitleAdapter_GetActiveTitle()!=GameTitle::Halo3 || !Game_ReclaimerOdstLevel() ||
           !reclaimer_odst_prompt::Read(value)) return false;
        prompt=(value&0x3u)!=0;   // a prompt waiting on confirm or cancel
        if(reclaimer_odst_prompt::pdaLive.load(std::memory_order_acquire)) {
            reclaimer_odst_prompt::PdaRead r{};
            map=reclaimer_odst_prompt::ReadPda(r)&&reclaimer_odst_prompt::PdaMapOpen(r);
        }
        if(raw) *raw=(value&0xFu)|(map?0x10u:0u);
        return true;
    }
    auto& p=native_odst::prompt;
    if(!p.proved.load(std::memory_order_acquire) || TitleAdapter_GetActiveTitle()!=GameTitle::Halo3ODST) return false;
    const uint64_t stamp=p.stampMs.load(std::memory_order_acquire);
    // The builder reads accept every tick: older than a quarter second, the game is not running its controls (a pause).
    if(!stamp || now<stamp || now-stamp>250) return false;
    const uint32_t generation=TitleAdapter_GetGeneration(GameTitle::Halo3ODST);
    if(!generation || p.generation.load(std::memory_order_relaxed)!=generation) return false;
    const uint32_t bits=p.bits.load(std::memory_order_relaxed);
    prompt=(bits&0x3u)!=0;   // a prompt waiting on accept or cancel
    map=(bits&0x10u)!=0;
    if(raw) *raw=bits;
    return true;
}
bool NativeVrActions_GestureRouting(GameTitle requested,uint64_t now) noexcept {
    if(requested==GameTitle::HaloCE)return native_ce::GestureRouting(now);
    if(requested==GameTitle::Halo3)return native_h3::GestureRouting(now);
    if(requested==GameTitle::Halo3ODST)return native_odst::GestureRouting(now);
    if(requested==GameTitle::Halo4)return native_h4::GestureRouting(now);
    if(requested==GameTitle::Halo2)return native_h2::GestureRouting(now);
    uint32_t actions=0;bool direct=false;
    return requested==title && Current() && ReadPublication(now,actions,direct) && direct;
}
void NativeVrActions_Halo3CoreRetired() {
    native_h3::Poll();
}
// PROJECT RECLAIMER'S ODST TUTORIAL TEXT IN VR WORDS (reclaimer_client::FindOdstTitleText, odst_prompt_text.h): its
// chud_show_cinematic_title is a native of the client's own that looks the title's string up into a 1024-character
// buffer, then measures it and copies it into the chud. The client's lookup is hooked; only when it returns to that
// native, and only while an ODST level runs, the buffer is rewritten before the native measures it. Found once, by shape,
// on the worker; the hook stays for the process (the client's code never unloads).
namespace reclaimer_odst_text {
std::atomic<bool> attempted{false};
void* original=nullptr;
uintptr_t returns[2]{};
wchar_t buffer[odst_prompt_text::kMaxChars+1]{};   // the script thread's only: the line is copied back at once
std::atomic<uint32_t> rewritten{}, kept{};
uint32_t reported=0;
using Lookup=uintptr_t(__fastcall*)(void*);
__declspec(noinline) void Rewrite(wchar_t* text) noexcept {
    __try {
        odst_prompt_text::Controls controls{};
        if(!g_config.vr_action_mapping || !Game_ReclaimerOdstLevel() || !odst_text::Read(controls)) return;
        const unsigned n=odst_prompt_text::Rewrite(text,controls,buffer);
        if(n) {
            odst_text::NoteSaid(text,buffer);
            std::memcpy(text,buffer,(static_cast<size_t>(n)+1)*sizeof(wchar_t));   // within the native's 1024
            rewritten.fetch_add(1,std::memory_order_relaxed);
            return;
        }
        for(unsigned i=0;text[i]&&i<odst_prompt_text::kMaxChars;++i)
            if(odst_prompt_text::glyph::Private(text[i])||text[i]==L'[') {
                kept.fetch_add(1,std::memory_order_relaxed);
                odst_text::NoteKept(text);
                break;
            }
    } __except(EXCEPTION_EXECUTE_HANDLER) {}
}
__declspec(noinline) wchar_t* BufferOf(void* context) noexcept {
    __try { return *reinterpret_cast<wchar_t* const*>(static_cast<uint8_t*>(context)+0x40); }
    __except(EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
// Another caller of the lookup whose line is one of the HUD's VISR prompts: where that prompt is fetched (logged by
// the worker, read-only; the line is not changed there until that caller's buffer is known).
std::atomic<uintptr_t> hudCallers[4]{};
std::atomic<uint32_t> hudCallersShown{};
wchar_t hudLine[64]{};
__declspec(noinline) void NoteOtherCaller(wchar_t* text,uintptr_t caller) noexcept {
    __try {
        const wchar_t* kit=odst_prompt_text::detail::KitLineFor(text);
        if(!kit || !std::wcsstr(kit,L"VISR") || std::wcschr(kit,L'\r')) return;   // the HUD's one-line VISR prompts
        for(auto& seen:hudCallers) {
            uintptr_t expected=0;
            if(seen.load(std::memory_order_relaxed)==caller) return;
            if(seen.compare_exchange_strong(expected,caller,std::memory_order_acq_rel)) {
                unsigned i=0;
                for(;kit[i]&&i<63;++i) hudLine[i]=kit[i]<0x80?kit[i]:L'#';
                hudLine[i]=0;
                return;
            }
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {}
}
// Entered through the preserving thunk (OdstTitleLookupThunk): `caller` is the lookup's own return address.
uintptr_t __fastcall Hook(void* context,unsigned,uintptr_t caller) {
    const bool title=caller&&(caller==returns[0]||caller==returns[1]);
    wchar_t* text=context?BufferOf(context):nullptr;
    const uintptr_t result=reinterpret_cast<Lookup>(original)(context);
    if(text && title) Rewrite(text);
    else if(text && caller) NoteOtherCaller(text,caller);
    return result;
}
__declspec(noinline) bool Find(reclaimer_client::OdstTitleText& found) noexcept {
    __try {
        reclaimer_client::Image image;
        if(!reclaimer_client::ClientImageInMemory(image)) return false;
        found=reclaimer_client::FindOdstTitleText(image);
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
// Worker: once per process, as the first ODST level runs on Reclaimer; then what it did, each time that changes.
void Poll() {
    if(!g_hostIsProjectReclaimer) return;
    if(attempted.load(std::memory_order_relaxed)) {
        const uint32_t r=rewritten.load(std::memory_order_relaxed),k=kept.load(std::memory_order_relaxed);
        if(r+k!=reported) {
            reported=r+k;
            LOG("ODST tutorial text (Project Reclaimer): %u lines said in VR words, %u kept as the game's own",r,k);
        }
        odst_text::ReportKept(" (Project Reclaimer)");
        odst_text::ReportSaid(" (Project Reclaimer)");
        const uintptr_t base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        for(uint32_t i=hudCallersShown.load(std::memory_order_relaxed);i<4;++i) {
            const uintptr_t caller=hudCallers[i].load(std::memory_order_acquire);
            if(!caller) break;
            hudCallersShown.store(i+1,std::memory_order_relaxed);
            char line[64]{};
            for(unsigned k=0;k<63&&hudLine[k];++k) line[k]=static_cast<char>(hudLine[k]>=0x20&&hudLine[k]<0x7F?hudLine[k]:'#');
            LOG("ODST HUD prompts (Project Reclaimer): \"%s\" is looked up by exe+0x%llX (not said in VR words yet)",
                line,static_cast<unsigned long long>(caller-base));
        }
        return;
    }
    if(!Game_ReclaimerOdstLevel()) return;
    attempted.store(true,std::memory_order_relaxed);
    reclaimer_client::OdstTitleText found{};
    if(!Find(found)) {
        LOG("ODST tutorial text (Project Reclaimer): the game's own words kept; the client executable could not be read");
        return;
    }
    if(found.missing) {
        LOG("ODST tutorial text (Project Reclaimer): the game's own words kept; not found: %s",found.missing);
        return;
    }
    const uintptr_t base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    returns[0]=base+found.returns[0];
    returns[1]=found.returns[1]?base+found.returns[1]:0;
    auto* target=reinterpret_cast<void*>(base+found.lookup);
    const bool hooked=MH_CreateHook(target,reinterpret_cast<void*>(&OdstTitleLookupThunk),&original)==MH_OK &&
        MH_EnableHook(target)==MH_OK;
    LOG("ODST tutorial text (Project Reclaimer): %s (chud_show_cinematic_title's native exe+0x%X, its string lookup "
        "exe+0x%X)",hooked?"said in VR words":"the game's own words kept, the hook did not install",found.native,
        found.lookup);
}
}
extern "C" uintptr_t (*g_odstTitleLookupHook)(void*,unsigned,uintptr_t)=&reclaimer_odst_text::Hook;
void NativeVrActions_Poll() {
    reclaimer_odst_prompt::Poll();
    reclaimer_odst_text::Poll();
    native_ce::Poll();
    native_h3::Poll();
    native_odst::Poll();
    native_h4::Poll();
    native_h2::Poll();
    const auto active=TitleAdapter_GetActiveTitle();
    const auto generation=TitleAdapter_GetGeneration(title);
    if(runtime.module && runtime.faulted.load(std::memory_order_acquire)) {
        blockedGeneration=runtime.generation.load(std::memory_order_acquire);
        if(!Retire()) return;
        LOG("Native VR action bridge Reach: retired faulted optional hooks; transport routing retained");
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
            Prove(base,size,d.stateLoadRva,d.statePattern) && Prove(base,size,padProofRva,padProofPattern) &&
            Prove(base,size,controllerLoopRva,controllerLoopPattern) &&
            Prove(base,size,consumerBegin,consumerEntryPattern) &&
            Prove(base,size,consumerReloadRva,consumerReloadPattern) && ProveConsumerBounds(base);
        if(proved) {
            runtime.state=sig::RipTarget(base+d.stateLoadRva+10,base+d.stateLoadRva+14);
            proved=runtime.state>=base && runtime.state-base<size && size-(runtime.state-base)>=4*d.stride;
            runtime.padState=sig::RipTarget(base+padProofRva+10,base+padProofRva+14);
            proved=proved && runtime.padState>=base && runtime.padState-base<size && size-(runtime.padState-base)>=4*0x3c;
        }
        runtime.jetpackCounted=proved&&Prove(base,size,jetpackJumpRva,jetpackJumpPattern)&&
            Prove(base,size,jetpackAbilityRva,jetpackAbilityPattern)&&Prove(base,size,plainAbilityRva,plainAbilityPattern);
        if(runtime.jetpackCounted&&Prove(base,size,profileLoadRva,profileLoadPattern)) {
            const uintptr_t profiles=sig::RipTarget(base+profileLoadRva+10,base+profileLoadRva+14)+
                *reinterpret_cast<const int32_t*>(base+profileLoadRva+17);
            const uintptr_t fallback=sig::RipTarget(base+profileLoadRva+27,base+profileLoadRva+31);
            if(profiles>=base&&profiles-base<size&&size-(profiles-base)>=4*0xAB8) runtime.profiles=profiles;
            if(fallback>=base&&fallback-base<size&&size-(fallback-base)>=0xAB8) runtime.defaultProfile=fallback;
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
        runtime.readerAt.store(0,std::memory_order_release);
        runtime.admitted.store(proved,std::memory_order_release);
        LOG("Native VR action bridge Reach: %s; requires observed owned input cadence; runtime validation pending",
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
        LOG("Native VR action bridge Reach: converter=%llu directReads=%llu%s; jump-in-the-air jetpack check %s: ran %llu, "
            "saw jump held %llu; ability button read on the jetpack's path %llu, on any other ability's %llu; "
            "double-press jump to jetpack byte: player 1's profile %d, default profile %d (-1 unread)",
            runtime.converted.load(),runtime.routed.load(),runtime.faulted.load()?" TRANSPORT FALLBACK (guarded access fault)":"",
            runtime.jetpackCounted?"counted":"not proven",runtime.jetpackChecks.load(),runtime.jetpackHeld.load(),
            runtime.jetpackAbility.load(),runtime.plainAbility.load(),ProfileByte(runtime.profiles),
            ProfileByte(runtime.defaultProfile));
    }
}
