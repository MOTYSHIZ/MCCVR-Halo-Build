#include "haloce_controls.h"
#include "haloce_stereo_core.h"
#include "haloce_native_bindings.h"
#include "hook_quiescence.h"
#include "title_adapter.h"
#include "../common/haloce_controls_logic.h"
#include "../common/minhook_lifecycle.h"
#include "../common/log.h"
#include <windows.h>
#include <intrin.h>
#include <MinHook.h>

namespace
{
using namespace halo_ce;
using ObjectGetFn=uintptr_t(__fastcall*)(uint32_t,uint32_t);
using WeaponOwnerFn=uint32_t(__fastcall*)(uint32_t);
using PerspectiveFn=int16_t(__fastcall*)(int16_t);
using DatumGetFn=uintptr_t(__fastcall*)(uintptr_t,uint32_t);
using TurnFn=void(__fastcall*)(int32_t,float,float);
HMODULE retainedModule{};
uintptr_t moduleBase{};
void* turnTarget{};
void* turnOriginal{};
bool turnEnabled{};
std::atomic<uint32_t> generation{},callbacks{};
std::atomic<bool> active{},retiring{},stateReady{},turnReady{};
std::atomic<uint64_t> lastOwned{},observed{},applied{},refused{},exceptions{};
uint32_t stateFailedGeneration{},turnFailedGeneration{};
double qpcSeconds{};
uint64_t lastReport{};
thread_local ControlTurnState turnState;
struct Callback
{
    Callback() { callbacks.fetch_add(1,std::memory_order_acq_rel); }
    ~Callback() { callbacks.fetch_sub(1,std::memory_order_release); }
};
bool StateCurrent() noexcept
{
    return stateReady.load(std::memory_order_acquire)&&active.load(std::memory_order_acquire)&&
        !retiring.load(std::memory_order_acquire)&&TitleAdapter_GetActiveTitle()==GameTitle::HaloCE&&
        TitleAdapter_GetGeneration(GameTitle::HaloCE)==generation.load(std::memory_order_acquire);
}
bool TurnCurrent() noexcept
{ return turnReady.load(std::memory_order_acquire)&&StateCurrent(); }

static bool ReadLocalPlayerState(HaloCELocalPlayerState& state) noexcept
{
    if (!StateCurrent()) return false;
    HaloCELocalPlayerState candidate{};
    __try
    {
        candidate.generation=generation.load(std::memory_order_acquire);
        const uintptr_t mapping=*reinterpret_cast<const uintptr_t*>(moduleBase+0x2ea2d90);
        const uintptr_t control=*reinterpret_cast<const uintptr_t*>(moduleBase+0x2d8fe70);
        const uintptr_t players=*reinterpret_cast<const uintptr_t*>(moduleBase+0x1c40480);
        const uintptr_t clock=*reinterpret_cast<const uintptr_t*>(moduleBase+0x2e9fd68);
        const uintptr_t cinematic=*reinterpret_cast<const uintptr_t*>(moduleBase+0x2ea0208);
        if (!mapping||!control||!players||!clock||!cinematic||
            *reinterpret_cast<const uint8_t*>(clock)!=1) return false;
        candidate.nativePaused=*reinterpret_cast<const uint8_t*>(clock+2)!=0;
        candidate.nativeCinematicFlag=*reinterpret_cast<const uint8_t*>(cinematic+0xa)!=0;
        candidate.player=*reinterpret_cast<const uint32_t*>(mapping+0xb8);
        // The native table API accepts salt zero as an index lookup. Our
        // published ownership requires a full salted datum instead.
        if (candidate.player==0xffffffffu||!(candidate.player>>16)||
            *reinterpret_cast<const uint16_t*>(players+0x22)!=0xc20) return false;
        const uintptr_t player=reinterpret_cast<DatumGetFn>(
            moduleBase+contract::player_state::state_datum_get)(players,candidate.player);
        if (!player) return false;
        candidate.unit=*reinterpret_cast<const uint32_t*>(player+0x64);
        // Match the native input->output routing by complete player identity.
        for (int16_t input=0;input<4;++input)
            if (*reinterpret_cast<const uint32_t*>(mapping+4+input*4)==candidate.player)
            {
                if (candidate.inputUser!=-1) return false;
                candidate.inputUser=input;
            }
        if (candidate.inputUser<0) return false;
        const uintptr_t director=moduleBase+0x2d9b960+size_t(candidate.inputUser)*0xf8;
        candidate.nativeLookBlocked=*reinterpret_cast<const uint8_t*>(director+0x58)!=0;
        candidate.nativeInputBlocked=*reinterpret_cast<const uint8_t*>(director+0x59)!=0||
            (*reinterpret_cast<const int32_t*>(moduleBase+0x1c34fc8)==0&&
             *reinterpret_cast<const int32_t*>(moduleBase+0x1b85760)!=-1);
        candidate.nativePerspective=reinterpret_cast<PerspectiveFn>(
            moduleBase+contract::player_state::state_user_perspective)(0);
        if (candidate.nativePerspective<0||candidate.nativePerspective>3) return false;
        candidate.nativePreparesFirstPerson=candidate.nativePerspective==0;
        if (*reinterpret_cast<const uint32_t*>(control+0x170)!=candidate.unit||
            *reinterpret_cast<const uint32_t*>(control+0x10+size_t(candidate.inputUser)*0x58)!=candidate.unit)
            return false;
        const auto get=reinterpret_cast<ObjectGetFn>(moduleBase+contract::player_state::state_object_try_get);
        if (candidate.unit!=0xffffffffu)
        {
            if (!(candidate.unit>>16)) return false;
            const uintptr_t unit=get(candidate.unit,3);
            if (!unit||*reinterpret_cast<const uint32_t*>(unit+0x1f8)!=candidate.player) return false;
            candidate.parent=*reinterpret_cast<const uint32_t*>(unit+0xd8);
            candidate.onFoot=candidate.parent==0xffffffffu;
            candidate.hasControlledUnit=true;
            const uintptr_t users=*reinterpret_cast<const uintptr_t*>(moduleBase+0x2d9cd90);
            if (users)
            {
                const uint32_t weapon=*reinterpret_cast<const uint32_t*>(users+8);
                candidate.firstPersonVisible=(*reinterpret_cast<const uint8_t*>(users)&1)!=0;
                if (weapon!=0xffffffffu&&get(weapon,4)&&
                    reinterpret_cast<WeaponOwnerFn>(moduleBase+contract::player_state::state_weapon_owner)(weapon)==candidate.unit)
                    candidate.weapon=weapon;
            }
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    { exceptions.fetch_add(1,std::memory_order_relaxed);return false; }
    if (!StateCurrent()||candidate.generation!=generation.load(std::memory_order_acquire)) return false;
    state=candidate;return true;
}

bool AdmittedContext(HaloCELocalPlayerState& state,RenderContext& context) noexcept
{
    if (!ReadLocalPlayerState(state)||!HaloCE_GetGameplayContext(context)||
        state.generation!=context.tracking.generation||!context.tracking.controllers.padValid) return false;
    return OnFootControls({state.hasControlledUnit,state.onFoot,state.nativePreparesFirstPerson,
        state.nativeInputBlocked,state.nativeLookBlocked,state.nativePaused,state.nativeCinematicFlag,
        context.tracking.controllers.controlsPresentationBlocked});
}

void TurnBody(int32_t inputUser,float yawDelta,float pitchDelta,uintptr_t caller)
{
    const auto original=reinterpret_cast<TurnFn>(turnOriginal);
    if (!original) return;
    if (caller!=moduleBase+0xa99660||!TurnCurrent())
    { original(inputUser,yawDelta,pitchDelta);return; }
    observed.fetch_add(1,std::memory_order_relaxed);
    HaloCELocalPlayerState state{};RenderContext context{};
    LARGE_INTEGER counter{};QueryPerformanceCounter(&counter);
    const double nowSeconds=static_cast<double>(counter.QuadPart)*qpcSeconds;
    const bool admitted=AdmittedContext(state,context);
    if (state.inputUser>=0&&inputUser!=state.inputUser)
    { original(inputUser,yawDelta,pitchDelta);return; }
    if (!admitted)
    {
        (void)turnState.Step(context,false,nowSeconds);
        lastOwned=0;refused.fetch_add(1,std::memory_order_relaxed);
        original(inputUser,yawDelta,pitchDelta);return;
    }
    const float turn=turnState.Step(context,true,nowSeconds);
    if (!TurnCurrent()||!HaloCE_RenderContextCurrent(context))
    { lastOwned=0;original(inputUser,yawDelta,pitchDelta);return; }
    // Let the native updater normalize yaw and apply its own angular clamps.
    // On-foot HMD pitch is already part of the render pose; raw native pitch
    // would create a second independently accumulated look rotation.
    original(inputUser,turn,0.0f);
    lastOwned.store(GetTickCount64(),std::memory_order_release);
    applied.fetch_add(1,std::memory_order_relaxed);
}

void TurnDispatch(int32_t inputUser,float yawDelta,float pitchDelta,uintptr_t caller)
{
    callbacks.fetch_add(1,std::memory_order_acq_rel);
    __try { TurnBody(inputUser,yawDelta,pitchDelta,caller); }
    __finally
    {
        if (AbnormalTermination())
        { lastOwned=0;exceptions.fetch_add(1,std::memory_order_relaxed); }
        callbacks.fetch_sub(1,std::memory_order_release);
    }
}
__declspec(noinline) void __fastcall TurnHook(int32_t inputUser,float yawDelta,float pitchDelta)
{
    // Preserve the actual native caller before entering the cleanup boundary.
    // Native structured exceptions must not strand retirement ownership.
    TurnDispatch(inputUser,yawDelta,pitchDelta,reinterpret_cast<uintptr_t>(_ReturnAddress()));
}

bool Remove() noexcept
{
    active=false;retiring=true;stateReady=false;turnReady=false;lastOwned=0;
    if (turnTarget&&turnEnabled)
    {
        const auto result=MCCVR_DisableHookForRetirement(turnTarget);
        if (result!=MH_OK&&result!=MH_ERROR_DISABLED) return false;
        turnEnabled=false;
    }
    const void* functions[]{reinterpret_cast<const void*>(&TurnHook),
        reinterpret_cast<const void*>(&TurnDispatch),
        reinterpret_cast<const void*>(&TurnBody),
        reinterpret_cast<const void*>(&HaloCEControls_GetLocalPlayerState),
        reinterpret_cast<const void*>(&HaloCEControls_OwnsLookStick),
        reinterpret_cast<const void*>(&HaloCEControls_MapMoveStick),
        reinterpret_cast<const void*>(&HaloCEControls_GetLocomotionFrame)};
    const void* trampolines[]{turnOriginal,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr};
    if (!WaitForNativeDetourQuiescence(functions,trampolines,7,callbacks)) return false;
    if (turnTarget&&MH_RemoveHook(turnTarget)!=MH_OK) return false;
    turnTarget=turnOriginal=nullptr;
    if (retainedModule) { FreeLibrary(retainedModule);retainedModule=nullptr; }
    moduleBase=0;generation=0;retiring=false;return true;
}
bool InstallState(uintptr_t base,size_t size,uint32_t gen) noexcept
{
    const char* failure{};
    const NativeContractSet contracts{contract::player_state::entries,
        contract::player_state::witnesses,contract::player_state::relatives,contract::player_state::pointers};
    if (!VerifyNativeFeatureBindings(base,size,gen,contracts,failure))
    { LOG("CE native control state unavailable: %s",failure?failure:"binding failure");return false; }
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        reinterpret_cast<LPCWSTR>(base),&retainedModule)) return false;
    LARGE_INTEGER frequency{};QueryPerformanceFrequency(&frequency);
    if (frequency.QuadPart<=0) { FreeLibrary(retainedModule);retainedModule=nullptr;return false; }
    qpcSeconds=1.0/static_cast<double>(frequency.QuadPart);
    moduleBase=base;generation=gen;active=true;retiring=false;stateReady=true;
    LOG("CE native control state verified independently: player/input/output ownership and native admission flags");
    return true;
}
bool InstallTurn(uintptr_t base,size_t size,uint32_t gen) noexcept
{
    const char* failure{};
    const NativeContractSet contracts{contract::controls::entries,
        contract::controls::witnesses,contract::controls::relatives,contract::controls::pointers};
    if (!VerifyNativeFeatureBindings(base,size,gen,contracts,failure))
    { LOG("CE VR turn stock fallback: %s",failure?failure:"binding failure");return false; }
    void* target=reinterpret_cast<void*>(base+contract::controls::input_angle_delta);
    auto result=MH_CreateHook(target,reinterpret_cast<void*>(&TurnHook),&turnOriginal);
    if (result!=MH_OK)
    { LOG("CE VR turn stock fallback: create status %d",result);return false; }
    turnTarget=target;
    result=MH_EnableHook(target);
    if (result!=MH_OK)
    {
        LOG("CE VR turn stock fallback: enable status %d",result);
        // Keep an unenabled failed entry retained until ordinary retirement;
        // state readers remain independently available.
        return false;
    }
    turnEnabled=true;turnReady=true;
    LOG("CE VR turn installed: exact native input delta phase, shared snap latch/smooth rate, on-foot only");
    return true;
}
}

bool HaloCEControls_Poll(uintptr_t base,size_t size,uint32_t gen,bool isActive) noexcept
{
    active.store(isActive,std::memory_order_release);
    if (retainedModule&&(!isActive||base!=moduleBase||gen!=generation.load()||retiring.load()))
        if (!Remove()) return false;
    if (!isActive||!base||!gen) return false;
    if (!stateReady.load()&&gen!=stateFailedGeneration)
        if (!InstallState(base,size,gen)) stateFailedGeneration=gen;
    if (stateReady.load()&&!turnReady.load()&&gen!=turnFailedGeneration)
        if (!InstallTurn(base,size,gen)) turnFailedGeneration=gen;
    const uint64_t now=GetTickCount64();
    if (now-lastReport>=2000)
    {
        lastReport=now;
        LOG("CE controls gen=%u state=%d turn=%d observed=%llu applied=%llu stock=%llu exceptions=%llu",
            gen,stateReady.load(),turnReady.load(),observed.load(),applied.load(),refused.load(),exceptions.load());
    }
    return TurnCurrent();
}
bool HaloCEControls_GetLocalPlayerState(HaloCELocalPlayerState& state) noexcept
{ Callback callback;return ReadLocalPlayerState(state); }
bool HaloCEControls_OwnsLookStick() noexcept
{
    Callback callback;
    const uint64_t stamp=lastOwned.load(std::memory_order_acquire),now=GetTickCount64();
    HaloCELocalPlayerState state{};RenderContext context{};
    return TurnCurrent()&&stamp&&now>=stamp&&now-stamp<150&&AdmittedContext(state,context)&&
        HaloCE_RenderContextCurrent(context);
}
bool HaloCEControls_MapMoveStick(float x,float y,float& outputX,float& outputY) noexcept
{
    Callback callback;HaloCELocalPlayerState state{};RenderContext context{};
    float candidateX{},candidateY{};
    if (!AdmittedContext(state,context)||!HeadRelativeMovement(context,x,y,candidateX,candidateY)||
        !StateCurrent()||!HaloCE_RenderContextCurrent(context)) return false;
    outputX=candidateX;outputY=candidateY;return true;
}
bool HaloCEControls_GetLocomotionFrame(HaloCELocalPlayerState& state,RenderContext& context) noexcept
{
    Callback callback;HaloCELocalPlayerState candidateState{};RenderContext candidateContext{};
    if (!AdmittedContext(candidateState,candidateContext)||!StateCurrent()||
        !HaloCE_RenderContextCurrent(candidateContext)) return false;
    state=candidateState;context=candidateContext;return true;
}
