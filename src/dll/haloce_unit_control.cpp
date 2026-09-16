#include "haloce_unit_control.h"
#include "haloce_controls.h"
#include "haloce_stereo_core.h"
#include "haloce_native_bindings.h"
#include "hook_quiescence.h"
#include "title_adapter.h"
#include "../common/haloce_controls_logic.h"
#include "../common/haloce_unit_control_logic.h"
#include "../common/minhook_lifecycle.h"
#include "../common/log.h"
#include <windows.h>
#include <intrin.h>
#include <MinHook.h>

namespace
{
using namespace halo_ce;
using UnitControlFn=void(__fastcall*)(uint32_t,const UnitControlPacket*,int32_t);
using MovementFn=void(__fastcall*)(void*);
void* target{};
void* original{};
void* movementTarget{};
void* movementOriginal{};
HMODULE retained{};
uintptr_t moduleBase{};
bool enabled{};
bool movementEnabled{};
std::atomic<bool> active{},retiring{},ready{};
std::atomic<uint32_t> generation{},callbacks{};
uint32_t rejectedGeneration{};
uintptr_t rejectedBase{};
enum class PendingCleanup { None,Disable,Quiescence,Remove,Callbacks };
PendingCleanup pendingCleanup{};
bool pinFailureReported{};
std::atomic<uint64_t> bodies{},aims{},stock{},declined{},exceptions{};
std::atomic<uint64_t> movements{},movementStock{},movementDeclined{};
uint64_t lastReport{};

bool Current() noexcept
{
    return active.load(std::memory_order_acquire)&&!retiring.load(std::memory_order_acquire)&&
        TitleAdapter_GetActiveTitle()==GameTitle::HaloCE&&
        TitleAdapter_GetGeneration(GameTitle::HaloCE)==generation.load(std::memory_order_acquire);
}
bool Owner(uint32_t unit,HaloCELocalPlayerState& state,RenderContext& context) noexcept
{
    return Current()&&HaloCEControls_GetLocomotionFrame(state,context)&&
        unit!=0xffffffffu&&unit==state.unit&&state.player!=0xffffffffu&&
        state.generation==generation.load(std::memory_order_acquire)&&
        state.generation==context.tracking.generation&&
        OnFootControls({state.hasControlledUnit,state.onFoot,state.nativePerspective==0,
            state.nativeInputBlocked,state.nativeLookBlocked,state.nativePaused,
            state.nativeCinematicFlag,context.tracking.controllers.controlsPresentationBlocked})&&
        Current()&&HaloCE_RenderContextCurrent(context);
}
bool CopyPacket(const UnitControlPacket* source,UnitControlPacket& output) noexcept
{
    if (!source) return false;
    __try { output=*source;return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
void UnitControlBody(uint32_t unit,const UnitControlPacket* source,int32_t clientUpdate,uintptr_t caller)
{
    const auto native=reinterpret_cast<UnitControlFn>(original);
    if (!native) return;
    UnitControlPacket packet{},candidate{};
    HaloCELocalPlayerState state{},latest{};RenderContext context{},latestContext{};
    bool aim{};
    if (caller==moduleBase+0xad0d5b&&ready.load(std::memory_order_acquire)&&
        Owner(unit,state,context)&&CopyPacket(source,packet)&&
        BuildTrackedUnitControl(context,packet,candidate,aim)&&Owner(unit,latest,latestContext)&&
        state.player==latest.player&&state.inputUser==latest.inputUser&&
        state.unit==latest.unit&&
        context.referenceRevision==latestContext.referenceRevision&&
        context.rendererEpoch==latestContext.rendererEpoch&&
        context.tracking.spaceEpoch==latestContext.tracking.spaceEpoch&&
        ready.load(std::memory_order_acquire)&&HaloCE_RenderContextCurrent(context))
    {
        // The original packet and native camera/input-angle state remain intact.
        // The engine owns validation, native interpolation and grenade release.
        native(unit,&candidate,clientUpdate);
        bodies.fetch_add(1,std::memory_order_relaxed);
        if (aim) aims.fetch_add(1,std::memory_order_relaxed);
        else declined.fetch_add(1,std::memory_order_relaxed);
        return;
    }
    stock.fetch_add(1,std::memory_order_relaxed);
    native(unit,source,clientUpdate);
}
__declspec(noinline) void __fastcall UnitControlHook(uint32_t unit,const UnitControlPacket* packet,int32_t clientUpdate)
{
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    callbacks.fetch_add(1,std::memory_order_acq_rel);
    __try { UnitControlBody(unit,packet,clientUpdate,caller); }
    __finally
    {
        if (AbnormalTermination()) exceptions.fetch_add(1,std::memory_order_relaxed);
        callbacks.fetch_sub(1,std::memory_order_release);
    }
}
bool ReadMovement(void* data,uint32_t& unit,UnitMovementBasis& basis) noexcept
{
    if (!data) return false;
    __try
    {
        std::memcpy(&unit,data,sizeof(unit));
        std::memcpy(&basis,static_cast<uint8_t*>(data)+0x14,sizeof(basis));
        return Finite(basis.forward)&&Finite(basis.aim);
    }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool WriteMovement(void* data,const UnitMovementBasis& basis) noexcept
{
    if (!data) return false;
    __try { std::memcpy(static_cast<uint8_t*>(data)+0x14,&basis,sizeof(basis));return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
void MovementBody(void* data,uintptr_t caller)
{
    const auto native=reinterpret_cast<MovementFn>(movementOriginal);
    if (!native) return;
    uint32_t unit{};UnitMovementBasis before{},candidate{};
    HaloCELocalPlayerState state{},latest{};RenderContext context{},latestContext{};
    if ((caller!=moduleBase+contract::unit_control::movement_consumer_return&&
         caller!=moduleBase+contract::unit_control::movement_consumer_return_secondary)||
        !ready.load(std::memory_order_acquire)||!ReadMovement(data,unit,before)||
        !Owner(unit,state,context)||!BuildUnitMovementBasis(context,candidate)||
        !Owner(unit,latest,latestContext)||state.player!=latest.player||
        state.inputUser!=latest.inputUser||state.unit!=latest.unit||
        context.referenceRevision!=latestContext.referenceRevision||
        context.rendererEpoch!=latestContext.rendererEpoch||
        context.tracking.spaceEpoch!=latestContext.tracking.spaceEpoch||
        !ready.load(std::memory_order_acquire)||!HaloCE_RenderContextCurrent(context))
    { movementStock.fetch_add(1,std::memory_order_relaxed);native(data);return; }
    if (!WriteMovement(data,candidate))
    {
        (void)WriteMovement(data,before);
        movementDeclined.fetch_add(1,std::memory_order_relaxed);native(data);return;
    }
    // Only private movement-input vectors change. Native collision, slope,
    // acceleration, output velocity and the live unit's aiming remain native.
    __try { native(data);movements.fetch_add(1,std::memory_order_relaxed); }
    __finally
    {
        if (!WriteMovement(data,before)) movementDeclined.fetch_add(1,std::memory_order_relaxed);
    }
}
__declspec(noinline) void __fastcall MovementHook(void* data)
{
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    callbacks.fetch_add(1,std::memory_order_acq_rel);
    __try { MovementBody(data,caller); }
    __finally
    {
        if (AbnormalTermination()) exceptions.fetch_add(1,std::memory_order_relaxed);
        callbacks.fetch_sub(1,std::memory_order_release);
    }
}
bool Remove() noexcept
{
    active=false;retiring=true;ready=false;
    const auto pending=[](PendingCleanup stage,const char* reason)
    {
        if (pendingCleanup!=stage)
            LOG("CE body/controller grenade aim cleanup pending: %s; optional feature stock, module/trampoline retained",reason);
        pendingCleanup=stage;
        return false;
    };
    if (enabled)
    {
        const auto result=MCCVR_DisableHookForRetirement(target);
        if (result!=MH_OK&&result!=MH_ERROR_DISABLED) return pending(PendingCleanup::Disable,"hook disable failed");
        enabled=false;
    }
    if (movementEnabled)
    {
        const auto result=MCCVR_DisableHookForRetirement(movementTarget);
        if (result!=MH_OK&&result!=MH_ERROR_DISABLED) return pending(PendingCleanup::Disable,"movement hook disable failed");
        movementEnabled=false;
    }
    const void* functions[]{reinterpret_cast<const void*>(&UnitControlHook),reinterpret_cast<const void*>(&MovementHook)};
    const void* originals[]{original,movementOriginal};
    if ((target||movementTarget)&&!WaitForNativeDetourQuiescence(functions,originals,2,callbacks))
        return pending(PendingCleanup::Quiescence,"callback quiescence not proved");
    if (target)
    {
        const auto result=MH_RemoveHook(target);
        if (result!=MH_OK&&result!=MH_ERROR_NOT_CREATED) return pending(PendingCleanup::Remove,"hook removal failed");
        target=original=nullptr;
    }
    if (movementTarget)
    {
        const auto result=MH_RemoveHook(movementTarget);
        if (result!=MH_OK&&result!=MH_ERROR_NOT_CREATED) return pending(PendingCleanup::Remove,"movement hook removal failed");
        movementTarget=movementOriginal=nullptr;
    }
    if (callbacks.load(std::memory_order_acquire)) return pending(PendingCleanup::Callbacks,"callback still active");
    target=original=movementTarget=movementOriginal=nullptr;
    if (retained) { FreeLibrary(retained);retained=nullptr; }
    moduleBase=0;generation=0;retiring=false;pendingCleanup=PendingCleanup::None;return true;
}
bool Install(size_t size) noexcept
{
    const char* failure{};
    const NativeContractSet contracts{contract::unit_control::entries,contract::unit_control::witnesses,
        contract::unit_control::relatives,contract::unit_control::pointers};
    if (!VerifyNativeFeatureBindings(moduleBase,size,generation.load(),contracts,failure))
    { LOG("CE body/controller grenade aim stock fallback: %s",failure?failure:"binding failure");return false; }
    void* location=reinterpret_cast<void*>(moduleBase+contract::unit_control::unit_control_set);
    auto result=MH_CreateHook(location,reinterpret_cast<void*>(&UnitControlHook),&original);
    if (result!=MH_OK)
    { LOG("CE body/controller grenade aim stock fallback: create status %d",result);return false; }
    target=location;
    location=reinterpret_cast<void*>(moduleBase+contract::unit_control::movement_consumer);
    result=MH_CreateHook(location,reinterpret_cast<void*>(&MovementHook),&movementOriginal);
    if (result!=MH_OK)
    { LOG("CE body/controller grenade aim stock fallback: movement create status %d",result);return false; }
    movementTarget=location;
    result=MH_EnableHook(target);
    if (result!=MH_OK)
    { LOG("CE body/controller grenade aim stock fallback: enable status %d",result);return false; }
    enabled=true;
    result=MH_EnableHook(movementTarget);
    if (result!=MH_OK)
    { LOG("CE body/controller grenade aim stock fallback: movement enable status %d",result);return false; }
    movementEnabled=true;ready=true;
    LOG("CE unit control installed: local on-foot head facing/looking, controller aiming; private native-camera movement basis, native grenade release retained");
    return true;
}
}

bool HaloCEUnitControl_Poll(uintptr_t base,size_t size,uint32_t gen,bool isActive) noexcept
{
    if (retained&&(!isActive||base!=moduleBase||gen!=generation.load()||retiring.load()))
        if (!Remove()) return false;
    active.store(isActive,std::memory_order_release);
    if (!isActive||!base||!gen) return false;
    if (rejectedBase==base&&rejectedGeneration==gen) return false;
    if (!retained)
    {
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
            reinterpret_cast<LPCWSTR>(base),&retained))
        {
            if (!pinFailureReported)
                LOG("CE body/controller grenade aim stock fallback: module retention failed; retry pending");
            pinFailureReported=true;
            return false;
        }
        pinFailureReported=false;
        moduleBase=base;generation=gen;retiring=false;
    }
    if (!ready.load()&&!Install(size))
    {
        rejectedBase=base;rejectedGeneration=gen;
        // A partial optional hook owns its trampoline and module until removal
        // is proved. Retry cleanup on later polls without retrying this binding.
        (void)Remove();
        return false;
    }
    const uint64_t now=GetTickCount64();
    if (now-lastReport>=2000)
    {
        lastReport=now;
        LOG("CE unit control gen=%u ready=%d bodies=%llu aims=%llu stock=%llu aimDeclined=%llu movement=%llu movementStock=%llu movementDeclined=%llu exceptions=%llu",
            gen,ready.load(),bodies.load(),aims.load(),stock.load(),declined.load(),movements.load(),movementStock.load(),movementDeclined.load(),exceptions.load());
    }
    return Current()&&ready.load();
}
