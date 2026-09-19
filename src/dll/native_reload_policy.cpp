#include "native_reload_policy.h"
#include "title_adapter.h"
#include "sigscan.h"
#include "hook_quiescence.h"
#include "../common/native_reload_logic.h"
#include "../common/config.h"
#include "../common/log.h"
#include "../common/minhook_lifecycle.h"
#include <windows.h>
#include <intrin.h>
#include <MinHook.h>
#include <array>
#include <atomic>
#include <cmath>
#include <limits>

namespace
{
enum Role : unsigned { Auto,State,Action,Play,Duration,RoleCount };
struct Binding { uint32_t rva; const char* pattern; };
struct BindingSet {
    GameTitle title; std::array<Binding,RoleCount> entries;
    Binding caller; uint32_t autoReturn;
};
#include "native_reload_bindings.generated.h"
struct TailBindingSet { std::array<Binding,6> proofs; uint32_t globals[3]; };
#include "native_reload_tail_bindings.generated.h"
struct Runtime {
    HMODULE module{}; uintptr_t base{}; uint32_t generation{};
    std::atomic<unsigned> options{};
    std::atomic<bool> faulted{};
    std::atomic<uint32_t> callbacks{};
    std::atomic<uint64_t> suppressed{},shortened{},animations{},faults{},stockOwner{};
    void* target[RoleCount]{}; void* original[RoleCount]{};
    bool enabled[RoleCount]{},autoReady{},skipReady{},attempted{};
    uint64_t reportAt{};
    void* tailFunctions[3]{};
    uint8_t* (*tailUsers)(unsigned){};
    uint32_t* tlsIndex{};
    bool tailReady{};
    std::atomic<bool> tailFaulted{};
    std::atomic<uint64_t> tails{},tailFallback{};
};
Runtime runtime[native_reload::Count];
thread_local int animationScope=-1;
thread_local int timingScope=-1;
struct TailCapture {
    unsigned title; uint32_t weapon; int magazine;
    uint32_t user{},slot{},animation{}; bool played{},ambiguous{};
};
thread_local TailCapture* tailScope=nullptr;
thread_local bool tailAction=false;

bool Current(unsigned i,unsigned option)
{
    const auto& r=runtime[i];
    return (r.options.load(std::memory_order_acquire)&option)!=0 &&
        (option!=2 || !r.faulted.load(std::memory_order_acquire)) &&
        (option!=4 || !r.tailFaulted.load(std::memory_order_acquire)) &&
        TitleAdapter_GetActiveTitle()==kBindings[i].title &&
        TitleAdapter_GetGeneration(kBindings[i].title)==r.generation;
}
bool Owned(unsigned i,uint32_t weapon)
{
    if (Game_ReloadPolicyWeapon(kBindings[i].title,weapon)) return true;
    runtime[i].stockOwner.fetch_add(1,std::memory_order_relaxed);
    return false;
}
void Shorten(unsigned i,uint32_t weapon,int magazine)
{
    if (!Current(i,2)) return;
    __try {
        auto* bytes=static_cast<uint8_t*>(Game_ReloadPolicyWeapon(kBindings[i].title,weapon));
        const auto& layout=native_reload::layouts[i];
        if (bytes && native_reload::ShortenMagazine(i,{bytes,size_t(layout.base)+2*layout.stride},magazine))
            runtime[i].shortened.fetch_add(1,std::memory_order_relaxed);
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        runtime[i].faults.fetch_add(1,std::memory_order_relaxed);
        runtime[i].faulted.store(true,std::memory_order_release);
    }
}
#include "native_reload_tail.inl"
template<unsigned I> uint8_t __fastcall AutoHook(uint32_t weapon,int16_t magazine,uint8_t continuation)
{
    auto& r=runtime[I]; r.callbacks.fetch_add(1,std::memory_order_acq_rel);
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    uint8_t result=0;
    __try {
        const bool suppress=Current(I,1) && caller==r.base+kBindings[I].autoReturn && Owned(I,weapon);
        if (suppress) r.suppressed.fetch_add(1,std::memory_order_relaxed);
        else if constexpr(I==native_reload::CE) {
            TailCapture capture{I,weapon,magazine};
            auto* previous=tailScope;
            __try {
                tailScope=Current(I,4) && !Current(I,2) && Owned(I,weapon)?&capture:nullptr;
                reinterpret_cast<void(__fastcall*)(uint32_t,int16_t,uint8_t)>(r.original[Auto])(weapon,magazine,continuation);
                Shorten(I,weapon,magazine);
                if(tailScope) KeepReloadTail(capture);
            } __finally {tailScope=previous;}
        } else if constexpr(I==native_reload::H2) {
            result=reinterpret_cast<uint8_t(__fastcall*)(uint32_t,int16_t)>(r.original[Auto])(weapon,magazine);
        } else {
            result=reinterpret_cast<uint8_t(__fastcall*)(uint32_t,int16_t,uint8_t)>(r.original[Auto])(weapon,magazine,continuation);
        }
    } __finally { r.callbacks.fetch_sub(1,std::memory_order_release); }
    return result;
}
template<unsigned I> void __fastcall StateHook(uint32_t weapon,int16_t magazine,int32_t state)
{
    auto& r=runtime[I]; r.callbacks.fetch_add(1,std::memory_order_acq_rel);
    const int previous=timingScope;
    auto* previousTail=tailScope;
    TailCapture capture{I,weapon,magazine};
    __try {
        timingScope=-1;
        tailScope=nullptr;
        if (state>=1 && state<=3 && magazine>=0 && magazine<2 && Current(I,2) && Owned(I,weapon))
            timingScope=I;
        if ((state==1 || state==3) && magazine>=0 && magazine<2 && Current(I,4) && !Current(I,2) && Owned(I,weapon))
            tailScope=&capture;
        reinterpret_cast<void(__fastcall*)(uint32_t,int16_t,int32_t)>(r.original[State])(weapon,magazine,state);
        Shorten(I,weapon,magazine);
        if(tailScope) KeepReloadTail(capture);
    } __finally { tailScope=previousTail;timingScope=previous;r.callbacks.fetch_sub(1,std::memory_order_release); }
}
template<unsigned I> void __fastcall ActionHook(uint32_t unit,uint32_t weapon,uint32_t action,int32_t slot,uint8_t flag)
{
    auto& r=runtime[I]; r.callbacks.fetch_add(1,std::memory_order_acq_rel);
    const int previous=animationScope,previousTiming=timingScope;
    const bool previousTailAction=tailAction;
    __try {
        const uint32_t actualWeapon=I==native_reload::CE ? unit : weapon;
        const uint32_t actualAction=I==native_reload::CE ? weapon : action;
        // Nested stock actions must not inherit an outer suppression decision.
        animationScope=-1;timingScope=-1;
        tailAction=tailScope && tailScope->title==I && tailScope->weapon==actualWeapon &&
            native_reload::ReloadAction(I,actualAction);
        if (Current(I,2) && native_reload::SuppressAction(I,actualAction) && Owned(I,actualWeapon)) {
            animationScope=I;timingScope=I;
        }
        if constexpr(I==native_reload::CE)
            reinterpret_cast<void(__fastcall*)(uint32_t,uint32_t)>(r.original[Action])(unit,weapon);
        else if constexpr(I==native_reload::H4)
            reinterpret_cast<void(__fastcall*)(uint32_t,uint32_t,uint32_t,int32_t,uint8_t)>(r.original[Action])(unit,weapon,action,slot,flag);
        else
            reinterpret_cast<void(__fastcall*)(uint32_t,uint32_t,uint32_t,int32_t)>(r.original[Action])(unit,weapon,action,slot);
    } __finally { tailAction=previousTailAction;animationScope=previous;timingScope=previousTiming;r.callbacks.fetch_sub(1,std::memory_order_release); }
}
template<unsigned I> uint8_t __fastcall PlayHook(uint32_t user,uint32_t slot,uint32_t animation,uint8_t flag)
{
    auto& r=runtime[I];r.callbacks.fetch_add(1,std::memory_order_acq_rel);
    uint8_t result=0;
    __try {
        if (animationScope==int(I) && Current(I,2)) {
            r.animations.fetch_add(1,std::memory_order_relaxed);
            // H4's specialized-animation handler returns false without changes
            // for these reload/ready IDs; true bypasses the fallback play.
            result=1;
        } else if constexpr(I==native_reload::CE)
            reinterpret_cast<void(__fastcall*)(int16_t,int16_t,uint8_t)>(r.original[Play])(int16_t(user),int16_t(slot),uint8_t(animation));
        else if constexpr(I==native_reload::H3 || I==native_reload::ODST || I==native_reload::Reach || I==native_reload::H4)
            result=reinterpret_cast<uint8_t(__fastcall*)(uint32_t,uint32_t,uint32_t,uint8_t)>(r.original[Play])(user,slot,animation,flag);
        else
            reinterpret_cast<void(__fastcall*)(uint32_t,uint32_t,uint32_t,uint8_t)>(r.original[Play])(user,slot,animation,flag);
        if(tailAction && tailScope && tailScope->title==I) {
            if(tailScope->played) tailScope->ambiguous=true;
            tailScope->played=true;tailScope->user=user;tailScope->slot=I==native_reload::CE?0:slot;
            tailScope->animation=I==native_reload::CE?slot:animation;
        }
    } __finally { r.callbacks.fetch_sub(1,std::memory_order_release); }
    return result;
}
template<unsigned I> int16_t __fastcall DurationHook(uint32_t weapon,uint32_t animation,int32_t mode,uint32_t extra)
{
    auto& r=runtime[I];r.callbacks.fetch_add(1,std::memory_order_acq_rel);
    int16_t result=0;
    __try {
        constexpr uint32_t ready[]{10,0x5000024,0x26,0x26,0x27,0x71};
        const bool readyQuery=I==native_reload::CE ? (animation==0 && mode==10) : animation==ready[I];
        // Mode 2 is a keyframe query: preserve its native -1 sentinel.
        const bool durationQuery=I==native_reload::CE || mode==0 || mode==1 || mode==3;
        if (durationQuery && Current(I,2) && Owned(I,weapon) && (readyQuery || timingScope==int(I))) result=0;
        else if constexpr(I==native_reload::CE)
            result=reinterpret_cast<int16_t(__fastcall*)(uint32_t,int16_t,int16_t,int16_t)>(r.original[Duration])(weapon,int16_t(animation),int16_t(mode),int16_t(extra));
        else
            result=reinterpret_cast<int16_t(__fastcall*)(uint32_t,uint32_t,int32_t)>(r.original[Duration])(weapon,animation,mode);
    } __finally { r.callbacks.fetch_sub(1,std::memory_order_release); }
    return result;
}
#define RELOAD_HOOK_ROW(i) {reinterpret_cast<void*>(&AutoHook<i>),reinterpret_cast<void*>(&StateHook<i>),reinterpret_cast<void*>(&ActionHook<i>),reinterpret_cast<void*>(&PlayHook<i>),reinterpret_cast<void*>(&DurationHook<i>)}
void* const hooks[][RoleCount]{RELOAD_HOOK_ROW(0),RELOAD_HOOK_ROW(1),RELOAD_HOOK_ROW(2),RELOAD_HOOK_ROW(3),RELOAD_HOOK_ROW(4),RELOAD_HOOK_ROW(5)};
#undef RELOAD_HOOK_ROW

bool Prove(uintptr_t base,size_t size,const Binding& b)
{
    if (!b.rva || !b.pattern || b.rva>=size) return false;
    const auto hit=sig::Find(base,size,b.pattern);
    return hit==base+b.rva && !sig::Find(hit+1,base+size-hit-1,b.pattern);
}
bool ProveTail(unsigned i,size_t size)
{
    auto& r=runtime[i];const auto& b=kTailBindings[i];
    for(const auto& proof:b.proofs) if(proof.rva && !Prove(r.base,size,proof)) return false;
    for(auto global:b.globals) if(global && (global>=size || size-global<sizeof(uintptr_t))) return false;
    if(kTailLayout[i].tlsMember) {
        const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(r.base);
        const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(r.base+dos->e_lfanew);
        const auto& directory=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS];
        if(!directory.VirtualAddress || directory.VirtualAddress>=size ||
            size-directory.VirtualAddress<sizeof(IMAGE_TLS_DIRECTORY64)) return false;
        const auto* tls=reinterpret_cast<const IMAGE_TLS_DIRECTORY64*>(r.base+directory.VirtualAddress);
        if(tls->AddressOfIndex<r.base || tls->AddressOfIndex-r.base>=size ||
            size-(tls->AddressOfIndex-r.base)<sizeof(uint32_t)) return false;
        r.tlsIndex=reinterpret_cast<uint32_t*>(tls->AddressOfIndex);
    }
    for(unsigned n=0;n<3;++n) r.tailFunctions[n]=b.proofs[n].rva?reinterpret_cast<void*>(r.base+b.proofs[n].rva):nullptr;
    r.tailUsers=&NativeTailUsers;
    return true;
}
bool Install(unsigned i,Role role,size_t size)
{
    auto& r=runtime[i];
    if (r.enabled[role]) return true;
    const auto& binding=kBindings[i].entries[role];
    if (!Prove(r.base,size,binding)) return false;
    void* target=reinterpret_cast<void*>(r.base+binding.rva);
    if (MH_CreateHook(target,hooks[i][role],&r.original[role])!=MH_OK) return false;
    r.target[role]=target;
    if (MH_EnableHook(target)!=MH_OK) return false;
    r.enabled[role]=true;
    return true;
}
bool Retire(unsigned i)
{
    auto& r=runtime[i];r.options.store(0,std::memory_order_release);
    for (unsigned j=0;j<RoleCount;++j) if (r.enabled[j]) {
        const auto status=MCCVR_DisableHookForRetirement(r.target[j]);
        if (status!=MH_OK && status!=MH_ERROR_DISABLED) return false;
        r.enabled[j]=false;
    }
    const void* functions[RoleCount+3]{},*trampolines[RoleCount+3]{};
    for (unsigned j=0;j<RoleCount;++j) { functions[j]=hooks[i][j];trampolines[j]=r.original[j]; }
    functions[RoleCount]=reinterpret_cast<const void*>(&Current);
    functions[RoleCount+1]=reinterpret_cast<const void*>(&Owned);
    functions[RoleCount+2]=reinterpret_cast<const void*>(&Shorten);
    if (!WaitForNativeDetourQuiescence(functions,trampolines,RoleCount+3,r.callbacks)) return false;
    for (unsigned j=0;j<RoleCount;++j) if (r.target[j]) {
        if (MH_RemoveHook(r.target[j])!=MH_OK) return false;
        r.target[j]=r.original[j]=nullptr;
    }
    if (r.module) FreeLibrary(r.module);
    r.module=nullptr;r.base=0;r.generation=0;r.attempted=false;r.autoReady=r.skipReady=false;r.faulted=false;
    r.tailReady=false;r.tailFaulted=false;r.tailUsers=nullptr;r.tlsIndex=nullptr;
    for(auto& function:r.tailFunctions) function=nullptr;
    return true;
}
}

void NativeReloadPolicy_Poll()
{
    const auto active=TitleAdapter_GetActiveTitle();
    const auto now=GetTickCount64();
    for (unsigned i=0;i<native_reload::Count;++i) {
        auto& r=runtime[i];const auto title=kBindings[i].title;
        const auto generation=TitleAdapter_GetGeneration(title);
        if (r.module && (active!=title || generation!=r.generation)) {
            if (!Retire(i)) { if(now>=r.reportAt) {r.reportAt=now+2000;LOG("Native reload policy: title %u cleanup pending; stock feature admission",i);} continue; }
        }
        if (active!=title || !generation) continue;
        const unsigned requested=native_reload::Options(g_config.manual_reload,g_config.manual_reload_disable_auto,
            g_config.manual_reload_skip_animations,g_config.manual_reload_shortened_animation);
        if (!r.attempted && requested) {
            const auto* descriptor=TitleRegistry_Find(title);
            if (!descriptor || !GetModuleHandleExW(0,descriptor->moduleName,&r.module)) continue;
            r.base=reinterpret_cast<uintptr_t>(r.module);r.generation=generation;r.attempted=true;
            const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(r.base);
            const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(r.base+dos->e_lfanew);
            const size_t size=nt->OptionalHeader.SizeOfImage;
            // Prove before installing Play/Duration: their prologues may occur
            // within an independently verified data-reference proof.
            r.tailReady=ProveTail(i,size);
            // Independent transactions: one missing feature never gates another.
            r.autoReady=Prove(r.base,size,kBindings[i].caller) && Install(i,Auto,size);
            r.skipReady=(i==native_reload::CE ? (r.enabled[Auto] || Install(i,Auto,size)) : Install(i,State,size));
            for (Role role:{Action,Play,Duration}) r.skipReady=Install(i,role,size) && r.skipReady;
            r.tailReady=r.tailReady && r.skipReady;
            LOG("Native reload policy %s: disable-auto=%s skip-animation=%s shortened-animation=%s; native ammo transfer, local owner only; headset verification pending",
                descriptor->displayName,r.autoReady?"installed":"STOCK FALLBACK (binding/hook)",r.skipReady?"installed":"STOCK FALLBACK (binding/hook)",
                r.tailReady?"installed":"STOCK FALLBACK (binding/hook)");
        }
        r.options.store(requested & ((r.autoReady?1u:0u)|(r.skipReady?2u:0u)|(r.tailReady?4u:0u)),std::memory_order_release);
        if (r.module && now>=r.reportAt) {
            r.reportAt=now+5000;
            LOG("Native reload policy title=%u options=%u autoBlocked=%llu timersShortened=%llu animationsSkipped=%llu faults=%llu stockOwnerGuard=%llu%s",
                i,r.options.load(),r.suppressed.load(),r.shortened.load(),r.animations.load(),r.faults.load(),
                r.stockOwner.load(),
                r.faulted.load()?" SKIP STOCK FALLBACK (guarded access fault)":"");
            LOG("Native reload tail title=%u retained=%llu stockFallback=%llu%s",i,r.tails.load(),r.tailFallback.load(),
                r.tailFaulted.load()?" SHORTENED STOCK FALLBACK (guarded access fault)":"");
        }
    }
}
