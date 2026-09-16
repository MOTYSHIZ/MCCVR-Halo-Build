#include "haloce_orientation.h"
#include "haloce_controls.h"
#include "haloce_stereo_core.h"
#include "haloce_native_bindings.h"
#include "hook_quiescence.h"
#include "title_adapter.h"
#include "../common/haloce_controls_logic.h"
#include "../common/haloce_audio_logic.h"
#include "../common/minhook_lifecycle.h"
#include "../common/log.h"
#include <windows.h>
#include <intrin.h>
#include <MinHook.h>
#include <array>
#include <cstring>

namespace
{
using namespace halo_ce;
// HCEEK player_effects.c camera transform; independently matched to the native
// matrix composed into both CE camera records. See E-CE-ORIENTATION-1.
using EffectMatrix=std::array<float,13>;
constexpr EffectMatrix identityEffect{1,1,0,0,0,1,0,0,0,1,0,0,0};
using EffectFn=void(__fastcall*)(int16_t,EffectMatrix*);
using AudioFn=void(__fastcall*)(int32_t,const AudioListenerPacket*);
struct Feature
{
    void* target{};
    void* original{};
    bool enabled{};
    std::atomic<bool> ready{};
    uint32_t rejectedGeneration{};
    uintptr_t rejectedBase{};
    bool cleanupReported{};
};
Feature effect,audio;
HMODULE retained{};
uintptr_t moduleBase{};
std::atomic<bool> active{},retiring{};
std::atomic<uint32_t> generation{},callbacks{};
std::atomic<uint64_t> suppressed{},stockEffects{},exceptions{},declined{};
std::atomic<uint64_t> headListeners{},stockListeners{},audioDeclined{};
uint64_t lastReport{};
bool retirementReported{};
uintptr_t pinFailureBase{};
uint32_t pinFailureGeneration{};

bool Current() noexcept
{
    return active.load(std::memory_order_acquire)&&!retiring.load(std::memory_order_acquire)&&
        TitleAdapter_GetActiveTitle()==GameTitle::HaloCE&&
        TitleAdapter_GetGeneration(GameTitle::HaloCE)==generation.load(std::memory_order_acquire);
}
bool Gameplay(RenderContext& context) noexcept
{
    HaloCELocalPlayerState state{};
    return Current()&&HaloCEControls_GetLocomotionFrame(state,context)&&
        state.generation==generation.load(std::memory_order_acquire)&&
        state.generation==context.tracking.generation&&Current()&&HaloCE_RenderContextCurrent(context);
}
bool WriteEffect(EffectMatrix* destination) noexcept
{
    if (!destination) return false;
    __try { *destination=identityEffect;return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
void EffectBody(int16_t outputUser,EffectMatrix* matrix,uintptr_t caller)
{
    const auto original=reinterpret_cast<EffectFn>(effect.original);
    if (!original) return;
    // Let native timers, RNG, accumulated effects and interpolation advance.
    // Only the exact local camera's returned transform is replaced afterward.
    original(outputUser,matrix);
    RenderContext context{};
    if (caller==moduleBase+0xac461d&&outputUser==0&&
        effect.ready.load(std::memory_order_acquire)&&Gameplay(context)&&
        effect.ready.load(std::memory_order_acquire)&&HaloCE_RenderContextCurrent(context))
    {
        if (WriteEffect(matrix)) { suppressed.fetch_add(1,std::memory_order_relaxed);return; }
        declined.fetch_add(1,std::memory_order_relaxed);
    }
    stockEffects.fetch_add(1,std::memory_order_relaxed);
}
__declspec(noinline) void __fastcall EffectHook(int16_t outputUser,EffectMatrix* matrix)
{
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    callbacks.fetch_add(1,std::memory_order_acq_rel);
    __try { EffectBody(outputUser,matrix,caller); }
    __finally
    {
        if (AbnormalTermination()) exceptions.fetch_add(1,std::memory_order_relaxed);
        callbacks.fetch_sub(1,std::memory_order_release);
    }
}
bool ReadListener(const AudioListenerPacket* source,AudioListenerPacket& output) noexcept
{
    if (!source) return false;
    __try { output=*source;return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
void AudioBody(int32_t listener,const AudioListenerPacket* packet,uintptr_t caller)
{
    const auto original=reinterpret_cast<AudioFn>(audio.original);
    if (!original) return;
    RenderContext context{};AudioListenerPacket source{},adapted{};
    if (caller==moduleBase+0xb4cefe&&listener==0&&
        audio.ready.load(std::memory_order_acquire)&&Gameplay(context))
    {
        if (ReadListener(packet,source)&&BuildHeadListener(context,source,adapted)&&
            audio.ready.load(std::memory_order_acquire)&&Current()&&HaloCE_RenderContextCurrent(context))
        {
            // The verified backend consumes its argument synchronously. A private
            // copy keeps the native observer and sound-manager globals untouched.
            original(listener,&adapted);headListeners.fetch_add(1,std::memory_order_relaxed);return;
        }
        audioDeclined.fetch_add(1,std::memory_order_relaxed);
    }
    original(listener,packet);stockListeners.fetch_add(1,std::memory_order_relaxed);
}
__declspec(noinline) void __fastcall AudioHook(int32_t listener,const AudioListenerPacket* packet)
{
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    callbacks.fetch_add(1,std::memory_order_acq_rel);
    __try { AudioBody(listener,packet,caller); }
    __finally
    {
        if (AbnormalTermination()) exceptions.fetch_add(1,std::memory_order_relaxed);
        callbacks.fetch_sub(1,std::memory_order_release);
    }
}
bool RemoveFeature(Feature& feature,const void* hook) noexcept
{
    feature.ready=false;
    if (feature.enabled)
    {
        const auto result=MCCVR_DisableHookForRetirement(feature.target);
        if (result!=MH_OK&&result!=MH_ERROR_DISABLED) return false;
        feature.enabled=false;
    }
    const void* functions[]{hook};
    const void* originals[]{feature.original};
    if (feature.target&&!WaitForNativeDetourQuiescence(functions,originals,1,callbacks)) return false;
    if (feature.target)
    {
        const auto result=MH_RemoveHook(feature.target);
        if (result!=MH_OK&&result!=MH_ERROR_NOT_CREATED) return false;
    }
    if (callbacks.load(std::memory_order_acquire)) return false;
    feature.target=feature.original=nullptr;return true;
}
bool Remove() noexcept
{
    active=false;retiring=true;effect.ready=false;audio.ready=false;
    const bool effectRemoved=RemoveFeature(effect,reinterpret_cast<const void*>(&EffectHook));
    const bool audioRemoved=RemoveFeature(audio,reinterpret_cast<const void*>(&AudioHook));
    if (!effectRemoved||!audioRemoved) return false;
    if (retained) { FreeLibrary(retained);retained=nullptr; }
    moduleBase=0;generation=0;retiring=false;return true;
}
bool InstallFeature(Feature& feature,size_t size,const NativeContractSet& contracts,
    uintptr_t rva,void* hook,const char* name) noexcept
{
    const char* failure{};
    if (!VerifyNativeFeatureBindings(moduleBase,size,generation.load(),contracts,failure))
    { LOG("CE %s stock fallback: %s",name,failure?failure:"binding failure");return false; }
    void* target=reinterpret_cast<void*>(moduleBase+rva);
    auto result=MH_CreateHook(target,hook,&feature.original);
    if (result!=MH_OK)
    { LOG("CE %s stock fallback: create status %d",name,result);return false; }
    feature.target=target;
    result=MH_EnableHook(target);
    if (result!=MH_OK)
    { LOG("CE %s stock fallback: enable status %d",name,result);return false; }
    feature.enabled=true;feature.ready=true;
    LOG("CE %s installed",name);return true;
}
}

bool HaloCEOrientation_Poll(uintptr_t base,size_t size,uint32_t gen,bool isActive) noexcept
{
    if (retained&&(!isActive||base!=moduleBase||gen!=generation.load()||retiring.load()))
    {
        if (!Remove())
        {
            if (!retirementReported)
            {
                LOG("CE orientation retirement pending: optional callbacks disabled for admission; module/trampolines retained until disable, quiescence and removal succeed");
                retirementReported=true;
            }
            return false;
        }
        retirementReported=false;
    }
    active.store(isActive,std::memory_order_release);
    if (!isActive||!base||!gen) return false;
    if (!retained)
    {
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
            reinterpret_cast<LPCWSTR>(base),&retained))
        {
            if (pinFailureBase!=base||pinFailureGeneration!=gen)
            {
                LOG("CE orientation stock fallback: module pin failed gen=%u error=%lu",gen,GetLastError());
                pinFailureBase=base;pinFailureGeneration=gen;
            }
            return false;
        }
        pinFailureBase=0;pinFailureGeneration=0;
        moduleBase=base;generation=gen;retiring=false;
    }
    auto pollFeature=[&](Feature& feature,const void* hook,const NativeContractSet& contracts,
        uintptr_t rva,const char* name)
    {
        if (feature.ready.load()) return;
        // A failed enable still owns a trampoline. Finish only this feature's
        // cleanup before retrying; the healthy sibling remains installed.
        if (feature.target&&!RemoveFeature(feature,hook))
        {
            if (!feature.cleanupReported)
            {
                LOG("CE %s stock fallback: partial installation cleanup pending; sibling feature remains independent",name);
                feature.cleanupReported=true;
            }
            return;
        }
        feature.cleanupReported=false;
        if (gen==feature.rejectedGeneration&&base==feature.rejectedBase) return;
        if (!InstallFeature(feature,size,contracts,rva,const_cast<void*>(hook),name))
        { feature.rejectedGeneration=gen;feature.rejectedBase=base; }
    };
    pollFeature(effect,reinterpret_cast<const void*>(&EffectHook),
        {contract::camera_effect::entries,contract::camera_effect::witnesses,
            contract::camera_effect::relatives,contract::camera_effect::pointers},
        contract::camera_effect::camera_effect_transform,
        "camera recoil/shake: native state advances; tracked local camera consumes identity effect");
    pollFeature(audio,reinterpret_cast<const void*>(&AudioHook),
        {contract::audio_listener::entries,contract::audio_listener::witnesses,
            contract::audio_listener::relatives,contract::audio_listener::pointers},
        contract::audio_listener::audio_listener_submit,
        "headset audio orientation: private native listener packet; native position/velocity/environment retained");
    const uint64_t now=GetTickCount64();
    if (now-lastReport>=2000)
    {
        lastReport=now;
        LOG("CE orientation gen=%u effect=%d suppressed=%llu stock=%llu declined=%llu audio=%d head=%llu stockAudio=%llu declinedAudio=%llu exceptions=%llu",
            gen,effect.ready.load(),suppressed.load(),stockEffects.load(),declined.load(),
            audio.ready.load(),headListeners.load(),stockListeners.load(),audioDeclined.load(),exceptions.load());
    }
    return Current()&&(effect.ready.load()||audio.ready.load());
}
