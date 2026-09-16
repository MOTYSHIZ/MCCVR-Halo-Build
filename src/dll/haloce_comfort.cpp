#include "haloce_comfort.h"
#include "haloce_stereo_core.h"
#include "haloce_native_bindings.h"
#include "hook_quiescence.h"
#include "title_adapter.h"
#include "../common/minhook_lifecycle.h"
#include "../common/log.h"
#include "../common/haloce_flare_logic.h"
#include <windows.h>
#include <intrin.h>
#include <MinHook.h>

namespace
{
using namespace halo_ce;
using BlurFn=void(__fastcall*)(uintptr_t,const void*,uintptr_t,const SaberCamera*,float);
HMODULE retained{};
uintptr_t moduleBase{};
void* target{};
BlurFn original{};
bool hookEnabled{};
std::atomic<bool> installed{},active{},retiring{};
std::atomic<uint32_t> generation{},callbacks{};
std::atomic<uint64_t> suppressed{},stock{},exceptions{};
uint32_t rejectedGeneration{};
uint64_t lastReport{};

bool Current() noexcept
{
    return active.load(std::memory_order_acquire)&&installed.load(std::memory_order_acquire)&&
        !retiring.load(std::memory_order_acquire)&&TitleAdapter_GetActiveTitle()==GameTitle::HaloCE&&
        TitleAdapter_GetGeneration(GameTitle::HaloCE)==generation.load(std::memory_order_acquire);
}
void BlurBody(uintptr_t effect,const void* quad,uintptr_t source,const SaberCamera* camera,
    float strength,uintptr_t caller)
{
    Tracking tracking{};
    // E-CE-COMFORT-1: the native off branch returns before its history updater
    // and GPU work. The exact primary-eye scope applies the same behavior to
    // config.motion_blur=false; other callbacks retain their original inputs.
    if (caller==moduleBase+0x45107f&&Current()&&
        HaloCE_GetAnniversaryEyeTracking(camera,tracking)&&!tracking.motionBlur&&
        tracking.generation==generation.load(std::memory_order_acquire)&&Current())
    { suppressed.fetch_add(1,std::memory_order_relaxed);return; }
    stock.fetch_add(1,std::memory_order_relaxed);
    original(effect,quad,source,camera,strength);
}
void BlurDispatch(uintptr_t effect,const void* quad,uintptr_t source,const SaberCamera* camera,
    float strength,uintptr_t caller)
{
    callbacks.fetch_add(1,std::memory_order_acq_rel);
    __try { BlurBody(effect,quad,source,camera,strength,caller); }
    __finally
    {
        if (AbnormalTermination()) exceptions.fetch_add(1,std::memory_order_relaxed);
        callbacks.fetch_sub(1,std::memory_order_release);
    }
}
__declspec(noinline) void __fastcall BlurHook(uintptr_t effect,const void* quad,uintptr_t source,
    const SaberCamera* camera,float strength)
{ BlurDispatch(effect,quad,source,camera,strength,reinterpret_cast<uintptr_t>(_ReturnAddress())); }

bool Remove() noexcept
{
    active=false;retiring=true;installed=false;
    if (hookEnabled)
    {
        const auto status=MCCVR_DisableHookForRetirement(target);
        if (status!=MH_OK&&status!=MH_ERROR_DISABLED) return false;
        hookEnabled=false;
    }
    const void* functions[]{reinterpret_cast<void*>(&BlurHook)};
    const void* originals[]{reinterpret_cast<void*>(original)};
    if (target&&!WaitForNativeDetourQuiescence(functions,originals,1,callbacks)) return false;
    if (target)
    {
        const auto status=MH_RemoveHook(target);
        if (status!=MH_OK&&status!=MH_ERROR_NOT_CREATED) return false;
    }
    if (callbacks.load(std::memory_order_acquire)) return false;
    target=nullptr;original=nullptr;moduleBase=0;generation=0;
    if (retained) { FreeLibrary(retained);retained=nullptr; }
    retiring=false;return true;
}
bool Install(uintptr_t base,size_t size,uint32_t gen) noexcept
{
    const char* failure=nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        reinterpret_cast<LPCWSTR>(base),&retained)) return false;
    moduleBase=base;generation=gen;retiring=false;
    const NativeContractSet contracts{contract::comfort::entries,contract::comfort::witnesses,
        contract::comfort::relatives,contract::comfort::pointers};
    if (!VerifyNativeFeatureBindings(base,size,gen,contracts,failure))
    { LOG("CE motion blur stock fallback: %s",failure?failure:"binding failure");Remove();return false; }
    target=reinterpret_cast<void*>(base+contract::comfort::motion_blur_draw);
    auto status=MH_CreateHook(target,reinterpret_cast<void*>(&BlurHook),reinterpret_cast<void**>(&original));
    if (status!=MH_OK)
    { LOG("CE motion blur stock fallback: create status %d",status);target=nullptr;Remove();return false; }
    status=MH_EnableHook(target);
    if (status!=MH_OK)
    { LOG("CE motion blur stock fallback: enable status %d",status);Remove();return false; }
    hookEnabled=true;installed=true;
    LOG("CE motion blur setting installed: exact Anniversary primary eyes honor shared motion_blur; other phases stay native");
    return true;
}
}
#include "haloce_flare_guard.inl"
bool HaloCEComfort_Poll(uintptr_t base,size_t size,uint32_t gen,bool isActive) noexcept
{
    ce_flare::Poll(base,size,gen,isActive);
    active.store(isActive,std::memory_order_release);
    if (retained&&(!isActive||base!=moduleBase||gen!=generation.load()||retiring.load()))
        if (!Remove()) return false;
    if (!isActive||!base||!gen) return false;
    if (!installed.load()&&gen!=rejectedGeneration)
        if (!Install(base,size,gen)) rejectedGeneration=gen;
    const uint64_t now=GetTickCount64();
    if (now-lastReport>=2000)
    {
        lastReport=now;
        LOG("CE motion blur gen=%u installed=%d suppressed=%llu stock=%llu exceptions=%llu",
            gen,installed.load(),suppressed.load(),stock.load(),exceptions.load());
    }
    return Current();
}
