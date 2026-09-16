#include "haloce_first_person.h"
#include "haloce_contact.h"
#include "haloce_stereo_core.h"
#include "haloce_native_bindings.h"
#include "hook_quiescence.h"
#include "title_adapter.h"
#include "../common/haloce_first_person_logic.h"
#include "../common/haloce_snapshot.h"
#include "../common/minhook_lifecycle.h"
#include "../common/log.h"
#include <windows.h>
#include <intrin.h>
#include <MinHook.h>
#include <limits>

namespace
{
using namespace halo_ce;
using PrepareFn=void(__fastcall*)(int16_t);
using PaletteFn=void(__fastcall*)(uint32_t,NodeMatrix*,const void*,const Vec3*,const Vec3*,const Vec3*);
using TagGetFn=uintptr_t(__fastcall*)(uint32_t);
using ModernRayFn=void(__fastcall*)(uint32_t,Vec3*,Vec3*,Vec3*,const Vec3*,bool,bool);
using LegacyRayFn=void(__fastcall*)(uint32_t,Vec3*,Vec3*,float*,bool,bool);
using PlayerRayFn=int16_t(__fastcall*)(uint32_t,Vec3*,Vec3*);
using SkinConvertFn=void(__fastcall*)(const NodeMatrix*,SaberBoneMatrix*);
using ClassicLensFn=void(__fastcall*)(float,bool);
using ParticleDrawFn=void(__fastcall*)(const SaberCamera*,uintptr_t,int,uintptr_t);
using ParticleCommitFn=void(__fastcall*)(uintptr_t,uintptr_t,bool);
using VisibilityPrepareFn=void(__fastcall*)(uintptr_t,int32_t,float,uintptr_t);
using VisibilitySubmitFn=void(__fastcall*)(uintptr_t,uintptr_t,uint8_t);
using GltConstantsFn=void(__fastcall*)(uintptr_t,float*,uintptr_t,const uint32_t*,
    uintptr_t,uintptr_t,uintptr_t,const float*);
struct Hook { void* target{};void* original{};bool enabled{}; };
Hook prepareHook,paletteHook;
Hook modernRayHook,legacyRayHook,assistRayHook;
Hook skinConvertHook;
Hook projectionHook,zfillProjectionHook,sfxProjectionHook;
Hook classicLensHook;
Hook particleDrawHook,particleCommitHook;
Hook visibilityPrepareHook,visibilitySubmitHook;
HMODULE retainedModule{};
uintptr_t moduleBase{};
std::atomic<uint32_t> generation{},callbacks{};
std::atomic<bool> installed{},active{},retiring{};
std::atomic<bool> aimInstalled{};
bool aimRetiring{};
std::atomic<bool> skinInstalled{};
bool skinRetiring{};
std::atomic<bool> projectionInstalled{};
bool projectionRetiring{};
std::atomic<bool> classicLensInstalled{};
bool classicLensRetiring{};
std::atomic<bool> particleProjectionInstalled{};
bool particleProjectionRetiring{};
std::atomic<bool> visibilityInstalled{};
bool visibilityRetiring{};
std::atomic<uint64_t> observed{},applied{},refused{},exceptions{},lastApplied{};
std::atomic<uint64_t> alignmentKnownGraph{},alignmentUnknownGraph{};
std::atomic<uint64_t> aimObserved{},aimApplied{},aimRefused{};
std::atomic<uint64_t> assistApplied{};
std::atomic<uint64_t> skinApplied{},skinRefused{};
std::atomic<uint64_t> projectionApplied{},projectionRefused{};
std::atomic<uint64_t> projectionPolicyRefused{},projectionSelectorRefused{},projectionChangedRefused{};
std::atomic<uint64_t> classicLensApplied{},classicLensRefused{};
std::atomic<uint64_t> particleProjectionApplied{},particleProjectionRefused{};
std::atomic<uint64_t> particleDrawObserved{},particleEyeRefused{};
std::atomic<uint64_t> visibilityObserved{},visibilityApplied{},visibilityRefused{};
uint32_t failedGeneration{};
uint32_t aimFailedGeneration{};
uint32_t skinFailedGeneration{};
uint32_t projectionFailedGeneration{};
uint32_t classicLensFailedGeneration{};
uint32_t particleProjectionFailedGeneration{};
uint32_t visibilityFailedGeneration{};
uint64_t lastReport{};
struct Scope
{
    RenderContext context{};
    NodeMatrix* output{};
    bool valid{};
};
struct PaletteReceipt
{
    RenderContext context{};
    uint64_t capturedAtMs{};
    uint64_t weaponGraph{};
};
Snapshot<PaletteReceipt> paletteReceipt;
thread_local Scope* scope{};
struct ParticleScope { const SaberCamera* camera{};Tracking eye{}; };
thread_local const ParticleScope* particleScope{};
struct ParticleBuffer
{
    uintptr_t vtable{};
    int32_t first{},end{},capacity{};
    uint32_t reserved{};
    float* data{};
    uint64_t trailing{};
    uintptr_t gpu{};
};
static_assert(sizeof(ParticleBuffer)==0x30&&offsetof(ParticleBuffer,data)==0x18&&offsetof(ParticleBuffer,gpu)==0x28);
struct Callback
{
    Callback() { callbacks.fetch_add(1,std::memory_order_acq_rel); }
    ~Callback() { callbacks.fetch_sub(1,std::memory_order_release); }
};
template<class T> bool Read(uintptr_t address,T& value) noexcept
{
    if (!address) return false;
    __try { std::memcpy(&value,reinterpret_cast<const void*>(address),sizeof(value));return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool Current() noexcept
{
    return installed.load(std::memory_order_acquire)&&active.load(std::memory_order_acquire)&&
        !retiring.load(std::memory_order_acquire)&&
        TitleAdapter_GetActiveTitle()==GameTitle::HaloCE&&
        TitleAdapter_GetGeneration(GameTitle::HaloCE)==generation.load(std::memory_order_acquire);
}
bool CurrentPaletteContext(RenderContext& context) noexcept
{
    const uint64_t last=lastApplied.load(std::memory_order_acquire),now=GetTickCount64();
    PaletteReceipt receipt{};
    if (!Current()||!last||now<last||now-last>=250||!paletteReceipt.Read(receipt)||
        receipt.capturedAtMs!=last||!HaloCE_RenderContextCurrent(receipt.context)||
        lastApplied.load(std::memory_order_acquire)!=last) return false;
    context=receipt.context;
    return true;
}
bool AimCurrent() noexcept
{
    return aimInstalled.load(std::memory_order_acquire)&&active.load(std::memory_order_acquire)&&
        !retiring.load(std::memory_order_acquire)&&
        TitleAdapter_GetActiveTitle()==GameTitle::HaloCE&&
        TitleAdapter_GetGeneration(GameTitle::HaloCE)==generation.load(std::memory_order_acquire);
}
bool LocalOnFootShooter(uint32_t shooter) noexcept
{
    if (shooter==0xffffffffu) return false;
    HaloCELocalPlayerState state{};
    return HaloCEControls_GetLocalPlayerState(state)&&state.hasControlledUnit&&state.unit==shooter&&
        state.weapon!=0xffffffffu&&state.onFoot&&state.nativePreparesFirstPerson&&
        !state.nativeInputBlocked&&!state.nativeLookBlocked&&!state.nativePaused&&
        !state.nativeCinematicFlag;
}
bool ControllerShotDirection(uint32_t shooter,Vec3& direction) noexcept
{
    RenderContext context{};NodeMatrix aim{};
    if (!AimCurrent()||!LocalOnFootShooter(shooter)||
        !HaloCE_GetGameplayContext(context)||
        context.tracking.controllers.controlsPresentationBlocked||
        !BuildControllerMatrix(context.camera,context.tracking,context.reference,
            context.tracking.controllers.primaryAim,context.unitsPerMeter,context.positional,aim)||
        !HaloCE_RenderContextCurrent(context)||!AimCurrent()) return false;
    direction=aim.forward;
    return true;
}
bool WriteDirection(Vec3* destination,const Vec3& value) noexcept
{
    if (!destination||!Finite(value)||std::fabs(Dot(value,value)-1)>0.05f) return false;
    __try { std::memcpy(destination,&value,sizeof(value));return true; }
    __except(EXCEPTION_EXECUTE_HANDLER)
    { exceptions.fetch_add(1,std::memory_order_relaxed);return false; }
}
__declspec(noinline) void __fastcall ModernRayHook(uint32_t unit,Vec3* position,
    Vec3* direction,Vec3* inheritedVelocity,const Vec3* offset,bool projectOrigin,bool useUnitAim)
{
    Callback callback;
    const auto original=reinterpret_cast<ModernRayFn>(modernRayHook.original);
    if (!original) return;
    Vec3 controller{};
    const bool callsite=reinterpret_cast<uintptr_t>(_ReturnAddress())==moduleBase+0xb7a796;
    if (callsite&&AimCurrent()) aimObserved.fetch_add(1,std::memory_order_relaxed);
    if (!callsite||!ControllerShotDirection(unit,controller)||!direction)
    {
        if (callsite&&AimCurrent()) aimRefused.fetch_add(1,std::memory_order_relaxed);
        original(unit,position,direction,inheritedVelocity,offset,projectOrigin,useUnitAim);return;
    }
    // The stock native helper still projects/clamps the authored muzzle and
    // computes inherited velocity. Only its optional unit-facing overwrite is
    // disabled. Trigger code applies stock spread/magnetism after this call.
    original(unit,position,&controller,inheritedVelocity,offset,projectOrigin,false);
    if (WriteDirection(direction,controller)) aimApplied.fetch_add(1,std::memory_order_relaxed);
    else aimRefused.fetch_add(1,std::memory_order_relaxed);
}
__declspec(noinline) void __fastcall LegacyRayHook(uint32_t unit,Vec3* position,
    Vec3* direction,float* inheritedSpeed,bool projectOrigin,bool useUnitAim)
{
    Callback callback;
    const auto original=reinterpret_cast<LegacyRayFn>(legacyRayHook.original);
    if (!original) return;
    Vec3 controller{};
    const bool callsite=reinterpret_cast<uintptr_t>(_ReturnAddress())==moduleBase+0xb7a858;
    if (callsite&&AimCurrent()) aimObserved.fetch_add(1,std::memory_order_relaxed);
    if (!callsite||!ControllerShotDirection(unit,controller)||!direction)
    {
        if (callsite&&AimCurrent()) aimRefused.fetch_add(1,std::memory_order_relaxed);
        original(unit,position,direction,inheritedSpeed,projectOrigin,useUnitAim);return;
    }
    original(unit,position,&controller,inheritedSpeed,projectOrigin,false);
    if (WriteDirection(direction,controller)) aimApplied.fetch_add(1,std::memory_order_relaxed);
    else aimRefused.fetch_add(1,std::memory_order_relaxed);
}
__declspec(noinline) int16_t __fastcall AssistRayHook(uint32_t unit,Vec3* position,Vec3* direction)
{
    Callback callback;
    const auto original=reinterpret_cast<PlayerRayFn>(assistRayHook.original);
    if (!original) return -1;
    const int16_t perspective=original(unit,position,direction);
    Vec3 controller{};
    // This is the downstream PLAYER aim-assist ray, after shot adjustment.
    // Other uses of the director ray (camera, AI, scopes) remain untouched.
    if (reinterpret_cast<uintptr_t>(_ReturnAddress())==moduleBase+0xb67be4&&
        ControllerShotDirection(unit,controller)&&WriteDirection(direction,controller))
        assistApplied.fetch_add(1,std::memory_order_relaxed);
    return perspective;
}
bool ReadGraph(uint32_t graph,AnimationNode (&nodes)[kFirstPersonMaxNodes],uint32_t& count) noexcept
{
    if (graph==0xffffffffu||(graph&0xffffu)>=0x8000u) return false;
    __try
    {
        const uintptr_t definition=reinterpret_cast<TagGetFn>(
            moduleBase+contract::first_person::first_person_cached_tag_get)(graph);
        if (!definition) return false;
        const int32_t nodeCount=*reinterpret_cast<const int32_t*>(definition+0x68);
        const int32_t cached=*reinterpret_cast<const int32_t*>(definition+0x6c);
        if (nodeCount<=0||nodeCount>int32_t(kFirstPersonMaxNodes)||!cached) return false;
        const intptr_t tagVirtual=*reinterpret_cast<const intptr_t*>(moduleBase+0x2ea3410);
        const intptr_t tagMapped=*reinterpret_cast<const intptr_t*>(moduleBase+0x2d9ce10);
        const intptr_t relative=intptr_t(cached)-tagVirtual;
        if (tagMapped<=0||relative<0||relative>0x40000000||
            tagMapped>std::numeric_limits<intptr_t>::max()-relative) return false;
        std::memcpy(nodes,reinterpret_cast<const void*>(tagMapped+relative),
            size_t(nodeCount)*sizeof(AnimationNode));
        count=uint32_t(nodeCount);
        return true;
    }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool CopyPalette(const NodeMatrix* source,NodeMatrix* destination,size_t count) noexcept
{
    if (!source||!destination||!count||count>kFirstPersonMaxNodes) return false;
    __try { std::memcpy(destination,source,count*sizeof(NodeMatrix));return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool ScaleConvertedSkin(const NodeMatrix* source,SaberBoneMatrix* destination) noexcept
{
    // Native 0x8B97D converts the model's already-copied instance+0x20C bone.
    // Its explicit scale belongs to that immutable source, not to a different
    // preparation's latest global receipt. Keep the old gate inert: a next
    // PrepareHook could otherwise remove scale halfway through this palette.
    constexpr bool useLatestPaletteReceipt=false;
    NodeMatrix native{};SaberBoneMatrix converted{};RenderContext context{};
    if (!source||!destination||!Current()||
        (useLatestPaletteReceipt&&(!CurrentPaletteContext(context)||
            context.tracking.controllers.controlsPresentationBlocked))||
        !Read(reinterpret_cast<uintptr_t>(source),native)||!Valid(native)||
        !Read(reinterpret_cast<uintptr_t>(destination),converted)||
        !ApplySaberFirstPersonScale(native.scale,converted)||
        (useLatestPaletteReceipt&&!HaloCE_RenderContextCurrent(context))||!Current()) return false;
    __try { std::memcpy(destination,&converted,sizeof(converted));return true; }
    __except(EXCEPTION_EXECUTE_HANDLER)
    { exceptions.fetch_add(1,std::memory_order_relaxed);return false; }
}
__declspec(noinline) void __fastcall SkinConvertHook(const NodeMatrix* source,SaberBoneMatrix* destination)
{
    Callback callback;
    const auto original=reinterpret_cast<SkinConvertFn>(skinConvertHook.original);
    if (!original) return;
    original(source,destination);
    // Only the native first-person skin subclass's proven conversion call.
    // Object carrier conversion and all world-object matrices remain native.
    if (reinterpret_cast<uintptr_t>(_ReturnAddress())!=moduleBase+0x8b982||
        !skinInstalled.load(std::memory_order_acquire)||!Current()) return;
    if (ScaleConvertedSkin(source,destination)) skinApplied.fetch_add(1,std::memory_order_relaxed);
    else skinRefused.fetch_add(1,std::memory_order_relaxed);
}
bool ApplyTrackedProjection(float* constants,uintptr_t model,size_t selectorOffset=0x170) noexcept
{
    // Native 0x2F3460 invokes these material writers from mesh preparation,
    // before the rendering thread owns either eye. They cache a lens POLICY,
    // not an eye matrix: zero selects whichever world matrix the later draw
    // uploads. Neither render-thread eye TLS nor the newest CPU palette owns
    // these constants. Keep both disproven scheduling gates inert.
    constexpr bool useLatestPaletteReceipt=false;
    constexpr bool requireDrawingEye=false;
    RenderContext context{};Tracking eye{},after{};
    uint32_t flags{};float selector[4]{};int32_t renderer=-1;
    if (!constants||!model||!Read(model+0x28,flags)||!(flags&0x10000000u)||
        !Current()||!Read(moduleBase+0x1b7aa84,renderer)||renderer!=1||
        !HaloCE_GetGameplayContext(context)||context.tracking.generation!=generation.load(std::memory_order_acquire)||
        context.tracking.controllers.controlsPresentationBlocked||
        (useLatestPaletteReceipt&&(!CurrentPaletteContext(context)||
            context.tracking.controllers.controlsPresentationBlocked||!HaloCE_RenderContextCurrent(context)))||
        (requireDrawingEye&&(!HaloCE_GetAnniversaryPrimaryEyeTracking(eye)||
            eye.generation!=generation.load(std::memory_order_acquire)||eye.controllers.controlsPresentationBlocked)))
    { projectionPolicyRefused.fetch_add(1,std::memory_order_relaxed);return false; }
    if (!Read(reinterpret_cast<uintptr_t>(constants)+selectorOffset,selector)||
        !SelectSaberTrackedProjection(flags,selector))
    { projectionSelectorRefused.fetch_add(1,std::memory_order_relaxed);return false; }
    RenderContext latest{};
    if (!HaloCE_GetGameplayContext(latest)||latest.tracking.controllers.controlsPresentationBlocked||
        !HaloCE_RenderContextCurrent(context)||!Current()||
        !Read(moduleBase+0x1b7aa84,renderer)||renderer!=1||
        (requireDrawingEye&&(!HaloCE_GetAnniversaryPrimaryEyeTracking(after)||
            after.generation!=eye.generation||after.spaceEpoch!=eye.spaceEpoch||after.serial!=eye.serial)))
    { projectionChangedRefused.fetch_add(1,std::memory_order_relaxed);return false; }
    __try { std::memcpy(reinterpret_cast<uint8_t*>(constants)+selectorOffset,selector,sizeof(selector));return true; }
    __except(EXCEPTION_EXECUTE_HANDLER)
    { exceptions.fetch_add(1,std::memory_order_relaxed);return false; }
}
bool ApplyClassicTrackedProjection(float& verticalFov) noexcept
{
    RenderContext context{},eyeContext{};int32_t renderer=-1;
    if (!CurrentPaletteContext(context)||context.tracking.controllers.controlsPresentationBlocked||
        !Read(moduleBase+0x1b7aa84,renderer)||renderer!=0||
        !HaloCE_GetClassicPrimaryEyeContext(eyeContext)||
        eyeContext.tracking.serial!=context.tracking.serial||
        eyeContext.referenceRevision!=context.referenceRevision||eyeContext.rendererEpoch!=context.rendererEpoch||
        !HaloCE_RenderContextCurrent(context)||!Current()) return false;
    return SelectClassicTrackedProjection(verticalFov);
}
__declspec(noinline) void __fastcall ClassicLensHook(float verticalFov,bool rebuild)
{
    callbacks.fetch_add(1,std::memory_order_acq_rel);
    const auto original=reinterpret_cast<ClassicLensFn>(classicLensHook.original);
    if (!original) { callbacks.fetch_sub(1,std::memory_order_release);return; }
    // Exact calls from native first-person model/effect flag branches. World setup,
    // lens save/restore, HUD and reflections retain their original arguments.
    if (IsClassicFirstPersonLensCallsite(reinterpret_cast<uintptr_t>(_ReturnAddress())-moduleBase)&&
        classicLensInstalled.load(std::memory_order_acquire)&&Current())
    {
        if (ApplyClassicTrackedProjection(verticalFov)) classicLensApplied.fetch_add(1,std::memory_order_relaxed);
        else classicLensRefused.fetch_add(1,std::memory_order_relaxed);
    }
    __try { original(verticalFov,rebuild); }
    __finally { callbacks.fetch_sub(1,std::memory_order_release); }
}
__declspec(noinline) void __fastcall ProjectionHook(uintptr_t material,float* constants,
    uintptr_t model,const uint32_t* variants,uintptr_t a5,uintptr_t a6,uintptr_t a7,const float* a8)
{
    Callback callback;
    const auto original=reinterpret_cast<GltConstantsFn>(projectionHook.original);
    if (!original) return;
    original(material,constants,model,variants,a5,a6,a7,a8);
    if (!projectionInstalled.load(std::memory_order_acquire)||!Current()) return;
    // The native writer is GLT-specific. Only its proven first-person flag
    // and exact four-lane selector can authorize the optional correction.
    uint32_t flags{};
    if (!Read(model+0x28,flags)||!(flags&0x10000000u)) return;
    if (ApplyTrackedProjection(constants,model)) projectionApplied.fetch_add(1,std::memory_order_relaxed);
    else projectionRefused.fetch_add(1,std::memory_order_relaxed);
}
__declspec(noinline) void __fastcall ZfillProjectionHook(uintptr_t material,float* constants,
    uintptr_t model,const uint32_t* variants,uintptr_t a5,uintptr_t a6,uintptr_t a7,const float* a8)
{
    Callback callback;
    const auto original=reinterpret_cast<GltConstantsFn>(zfillProjectionHook.original);
    if (!original) return;
    original(material,constants,model,variants,a5,a6,a7,a8);
    if (!projectionInstalled.load(std::memory_order_acquire)||!Current()) return;
    uint32_t flags{};
    if (!Read(model+0x28,flags)||!(flags&0x10000000u)) return;
    if (ApplyTrackedProjection(constants,model,0x20)) projectionApplied.fetch_add(1,std::memory_order_relaxed);
    else projectionRefused.fetch_add(1,std::memory_order_relaxed);
}
__declspec(noinline) void __fastcall SfxProjectionHook(uintptr_t material,float* constants,
    uintptr_t model,const uint32_t* variants,uintptr_t a5,uintptr_t a6,uintptr_t a7,const float* a8)
{
    Callback callback;
    const auto original=reinterpret_cast<GltConstantsFn>(sfxProjectionHook.original);
    if (!original) return;
    original(material,constants,model,variants,a5,a6,a7,a8);
    if (!projectionInstalled.load(std::memory_order_acquire)||!Current()) return;
    uint32_t flags{};
    if (!Read(model+0x28,flags)||!(flags&0x10000000u)) return;
    if (ApplyTrackedProjection(constants,model,0x70)) projectionApplied.fetch_add(1,std::memory_order_relaxed);
    else projectionRefused.fetch_add(1,std::memory_order_relaxed);
}
bool ReadParticleProjection(uintptr_t buffer,ParticleBuffer& source) noexcept
{
    const auto* owner=particleScope;
    Tracking current{},primary{};
    if (!owner||!Current()||!particleProjectionInstalled.load(std::memory_order_acquire)||
        !HaloCE_GetAnniversaryEyeTracking(owner->camera,current)||
        !HaloCE_GetAnniversaryPrimaryEyeTracking(primary)||
        primary.generation!=current.generation||primary.spaceEpoch!=current.spaceEpoch||primary.serial!=current.serial||
        current.generation!=owner->eye.generation||current.spaceEpoch!=owner->eye.spaceEpoch||
        current.serial!=owner->eye.serial||current.controllers.controlsPresentationBlocked||
        !Read(buffer,source)||source.vtable!=moduleBase+0x17f8c78||!source.data||!source.gpu||
        source.capacity<=0||source.capacity>4096||source.first<0||source.first>source.capacity||
        source.end<source.first||source.end>source.capacity||
        source.end-source.first!=int(kSaberParticleConstantVectors)||
        reinterpret_cast<uintptr_t>(source.data)>std::numeric_limits<uintptr_t>::max()-size_t(source.capacity)*16)
        return false;
    return true;
}
bool CopyParticleBacking(const ParticleBuffer& source,float* copy) noexcept
{
    __try { std::memcpy(copy,source.data,size_t(source.capacity)*16);return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { exceptions.fetch_add(1,std::memory_order_relaxed);return false; }
}
void CommitParticleDirtyRange(uintptr_t buffer,const ParticleBuffer& source,const ParticleBuffer& submitted) noexcept
{
    __try
    {
        // This is the only native bookkeeping changed by 0x202B10. Do not
        // overwrite a newer allocation if the source descriptor changed during
        // a nested call. No backing pointer or emitter bytes are ever written.
        if (std::memcmp(reinterpret_cast<const void*>(buffer),&source,sizeof(source))==0)
        {
            std::memcpy(reinterpret_cast<void*>(buffer+8),&submitted.first,sizeof(int32_t)*2);
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER) { exceptions.fetch_add(1,std::memory_order_relaxed); }
}
__declspec(noinline) bool SubmitTrackedParticleProjection(uintptr_t buffer,uintptr_t backend,bool immediate)
{
    ParticleBuffer source{};
    if (!ReadParticleProjection(buffer,source)) return false;
    // The D3D11 fallback uploads the complete buffer, not just its dirty range.
    // A bounded stack snapshot preserves that contract and prevents any write
    // to a worker's native backing store. No allocation or TLS scratch alias.
    alignas(16) float copied[4096*4];
    if (!CopyParticleBacking(source,copied)) return false;
    unsigned changed{};
    if (!SelectSaberTrackedParticleProjection(copied+size_t(source.first)*4,
            kSaberParticleConstantVectors,changed)) return false;
    ParticleBuffer revalidated{};
    if (!ReadParticleProjection(buffer,revalidated)||std::memcmp(&source,&revalidated,sizeof(source))) return false;
    ParticleBuffer submitted=source;submitted.data=copied;
    reinterpret_cast<ParticleCommitFn>(particleCommitHook.original)(reinterpret_cast<uintptr_t>(&submitted),backend,immediate);
    // A native exception propagates, discarding the private snapshot. Normal
    // completion preserves the engine's dirty-range reset, including recursion.
    CommitParticleDirtyRange(buffer,source,submitted);
    particleProjectionApplied.fetch_add(changed,std::memory_order_relaxed);
    return true;
}
void CommitParticleProjection(uintptr_t buffer,uintptr_t backend,bool immediate,bool nativeCallsite)
{
    const auto original=reinterpret_cast<ParticleCommitFn>(particleCommitHook.original);
    if (!original) return;
    if (nativeCallsite&&particleScope)
    {
        if (SubmitTrackedParticleProjection(buffer,backend,immediate)) return;
        particleProjectionRefused.fetch_add(1,std::memory_order_relaxed);
    }
    original(buffer,backend,immediate);
}
__declspec(noinline) void __fastcall ParticleCommitHook(uintptr_t buffer,uintptr_t backend,bool immediate)
{
    callbacks.fetch_add(1,std::memory_order_acq_rel);
    __try
    {
        CommitParticleProjection(buffer,backend,immediate,
            reinterpret_cast<uintptr_t>(_ReturnAddress())==moduleBase+0x678796);
    }
    __finally { callbacks.fetch_sub(1,std::memory_order_release); }
}
__declspec(noinline) void __fastcall ParticleDrawHook(const SaberCamera* camera,uintptr_t texture,int pass,uintptr_t batch)
{
    callbacks.fetch_add(1,std::memory_order_acq_rel);
    const auto original=reinterpret_cast<ParticleDrawFn>(particleDrawHook.original);
    const auto* previous=particleScope;
    ParticleScope owner{};owner.camera=camera;Tracking primary{};
    particleScope=nullptr;
    if (particleProjectionInstalled.load(std::memory_order_acquire)&&Current())
    {
        particleDrawObserved.fetch_add(1,std::memory_order_relaxed);
        if (HaloCE_GetAnniversaryEyeTracking(camera,owner.eye)&&
            HaloCE_GetAnniversaryPrimaryEyeTracking(primary)&&
            primary.generation==owner.eye.generation&&primary.spaceEpoch==owner.eye.spaceEpoch&&primary.serial==owner.eye.serial&&
            owner.eye.generation==generation.load(std::memory_order_acquire)&&
            !owner.eye.controllers.controlsPresentationBlocked) particleScope=&owner;
        else particleEyeRefused.fetch_add(1,std::memory_order_relaxed);
    }
    __try { if (original) original(camera,texture,pass,batch); }
    __finally { particleScope=previous;callbacks.fetch_sub(1,std::memory_order_release); }
}
bool ApplyPalette(uint32_t graph,NodeMatrix* matrices,const Scope& owner) noexcept
{
    AnimationNode nodes[kFirstPersonMaxNodes]{};
    uint32_t count{};
    if (!ReadGraph(graph,nodes,count)) return false;
    FirstPersonBinding binding{};
    const auto& context=owner.context;
    if (!BuildFirstPersonBinding(graph,context.tracking.generation,nodes,count,binding)) return false;
    if (context.tracking.controllers.leftHanded && context.tracking.controllers.handAlignment)
    {
        auto& counter = FindHandAlignmentPlane(binding) ? alignmentKnownGraph : alignmentUnknownGraph;
        counter.fetch_add(1, std::memory_order_relaxed);
    }
    std::array<NodeMatrix,kFirstPersonMaxNodes> source{},staged{};
    if (!CopyPalette(matrices,source.data(),count)) return false;
    if (!BuildTrackedFirstPersonPalette(binding,source.data(),context.camera,context.tracking,
            context.reference,context.unitsPerMeter,context.positional,staged)) return false;
    HaloCEContactPublication contact{};
    HaloCEContact_ApplyPalette(context,binding,source.data(),staged.data(),contact);
    // Ownership is checked after staging as well: a title transition cannot
    // publish an old graph merely because its native builder completed.
    if (!Current()||generation.load()!=context.tracking.generation||
        !HaloCE_RenderContextCurrent(context)) return false;
    if (!CopyPalette(staged.data(),matrices,count))
    {
        (void)CopyPalette(source.data(),matrices,count);
        return false;
    }
    // Skin/lens consumers must refer to the palette that actually committed.
    // A fresh gameplay sample is not evidence that its native palette tracked
    // successfully, especially after recenter, graphics switch or fallback.
    const uint64_t now=GetTickCount64();
    if (!now||!paletteReceipt.Publish({context,now,binding.nodeIdentity}))
    {
        (void)CopyPalette(source.data(),matrices,count);
        return false;
    }
    lastApplied.store(now,std::memory_order_release);
    // Receipt publication can still roll the native palette back. Contact
    // must enter the simulation queue only after that last rollback point.
    HaloCEContact_CommitPalette(context,contact);
    return true;
}
__declspec(noinline) void __fastcall PaletteHook(uint32_t graph,NodeMatrix* matrices,
    const void* animation,const Vec3* position,const Vec3* forward,const Vec3* up)
{
    Callback callback;
    auto original=reinterpret_cast<PaletteFn>(paletteHook.original);
    if (!original) return;
    original(graph,matrices,animation,position,forward,up);
    if (!Current()||!scope||!scope->valid||scope->output!=matrices||
        reinterpret_cast<uintptr_t>(_ReturnAddress())!=moduleBase+0xb29d8d) return;
    observed.fetch_add(1,std::memory_order_relaxed);
    if (ApplyPalette(graph,matrices,*scope))
    { applied.fetch_add(1,std::memory_order_relaxed); }
    else refused.fetch_add(1,std::memory_order_relaxed);
}
void RunPrepare(PrepareFn original,int16_t user,Scope* current,Scope* previous)
{
    scope=current;
    __try { original(user); }
    __finally { scope=previous; }
}
__declspec(noinline) void __fastcall PrepareHook(int16_t user)
{
    Callback callback;
    auto original=reinterpret_cast<PrepareFn>(prepareHook.original);
    if (!original) return;
    Scope local{};
    Scope* previous=scope;
    // The original prepare rebuilds stock first-person matrices. Invalidate
    // the old success even if this invocation cannot produce a tracked rig.
    if (user==0&&!previous) lastApplied.store(0,std::memory_order_release);
    Camera stock{};
    uintptr_t users{};
    if (user==0&&!previous&&Current()&&Read(moduleBase+0x29af2c4,stock)&&
        Read(moduleBase+0x2d9cd90,users)&&users&&
        users<=std::numeric_limits<uintptr_t>::max()-0x1e94&&
        HaloCE_GetRenderContext(stock,local.context))
    {
        local.output=reinterpret_cast<NodeMatrix*>(users+0x1088);
        local.valid=true;
    }
    RunPrepare(original,user,&local,previous);
}
#include "haloce_first_person_visibility.inl"
bool Remove() noexcept
{
    active=false;retiring=true;installed=false;aimInstalled=false;skinInstalled=false;projectionInstalled=false;classicLensInstalled=false;particleProjectionInstalled=false;visibilityInstalled=false;
    for (Hook* hook:{&prepareHook,&paletteHook,&modernRayHook,&legacyRayHook,&assistRayHook,&skinConvertHook,&projectionHook,&zfillProjectionHook,&sfxProjectionHook,&classicLensHook,&particleDrawHook,&particleCommitHook,&visibilityPrepareHook,&visibilitySubmitHook})
    {
        if (!hook->target||!hook->enabled) continue;
        const auto result=MCCVR_DisableHookForRetirement(hook->target);
        if (result!=MH_OK&&result!=MH_ERROR_DISABLED) return false;
        hook->enabled=false;
    }
    const void* functions[]={reinterpret_cast<const void*>(&PrepareHook),reinterpret_cast<const void*>(&PaletteHook),
        reinterpret_cast<const void*>(&ModernRayHook),reinterpret_cast<const void*>(&LegacyRayHook),
        reinterpret_cast<const void*>(&AssistRayHook),reinterpret_cast<const void*>(&SkinConvertHook),
        reinterpret_cast<const void*>(&ProjectionHook),reinterpret_cast<const void*>(&ZfillProjectionHook),
        reinterpret_cast<const void*>(&SfxProjectionHook),reinterpret_cast<const void*>(&ClassicLensHook),
        reinterpret_cast<const void*>(&ParticleDrawHook),reinterpret_cast<const void*>(&ParticleCommitHook),
        reinterpret_cast<const void*>(&VisibilityPrepareHook),reinterpret_cast<const void*>(&VisibilitySubmitHook)};
    const void* trampolines[]={prepareHook.original,paletteHook.original,modernRayHook.original,legacyRayHook.original,assistRayHook.original,skinConvertHook.original,projectionHook.original,zfillProjectionHook.original,sfxProjectionHook.original,classicLensHook.original,particleDrawHook.original,particleCommitHook.original,visibilityPrepareHook.original,visibilitySubmitHook.original};
    // The shared native stack verifier admits at most eight detour ranges.
    // All fourteen entries above are disabled before either batch is checked;
    // keep every trampoline/module alive until both batches are quiescent.
    if (!WaitForNativeDetourQuiescence(functions,trampolines,8,callbacks)||
        !WaitForNativeDetourQuiescence(functions+8,trampolines+8,6,callbacks)) return false;
    for (Hook* hook:{&prepareHook,&paletteHook,&modernRayHook,&legacyRayHook,&assistRayHook,&skinConvertHook,&projectionHook,&zfillProjectionHook,&sfxProjectionHook,&classicLensHook,&particleDrawHook,&particleCommitHook,&visibilityPrepareHook,&visibilitySubmitHook})
    {
        if (hook->target&&MH_RemoveHook(hook->target)!=MH_OK) return false;
        *hook={};
    }
    if (retainedModule) { FreeLibrary(retainedModule);retainedModule=nullptr; }
    moduleBase=0;generation=0;lastApplied=0;retiring=false;aimRetiring=false;skinRetiring=false;projectionRetiring=false;classicLensRetiring=false;particleProjectionRetiring=false;visibilityRetiring=false;
    return true;
}
bool RemoveSkin() noexcept
{
    skinInstalled=false;skinRetiring=true;
    if (skinConvertHook.target&&skinConvertHook.enabled)
    {
        const auto result=MCCVR_DisableHookForRetirement(skinConvertHook.target);
        if (result!=MH_OK&&result!=MH_ERROR_DISABLED) return false;
        skinConvertHook.enabled=false;
    }
    const void* functions[]={reinterpret_cast<const void*>(&SkinConvertHook)};
    const void* trampolines[]={skinConvertHook.original};
    if (!WaitForNativeDetourQuiescence(functions,trampolines,1,callbacks)) return false;
    if (skinConvertHook.target&&MH_RemoveHook(skinConvertHook.target)!=MH_OK) return false;
    skinConvertHook={};skinRetiring=false;return true;
}
bool InstallSkin(uintptr_t base,size_t size,uint32_t gen) noexcept
{
    const char* failure{};
    const NativeContractSet contracts{contract::first_person_skin::entries,
        contract::first_person_skin::witnesses,contract::first_person_skin::relatives,contract::first_person_skin::pointers};
    if (!VerifyNativeFeatureBindings(base,size,gen,contracts,failure))
    { LOG("CE Anniversary hand scale stock fallback: binding verification failed: %s",failure?failure:"unknown");return false; }
    void* target=reinterpret_cast<void*>(base+contract::first_person_skin::first_person_saber_bone_convert);
    auto result=MH_CreateHook(target,reinterpret_cast<void*>(&SkinConvertHook),&skinConvertHook.original);
    if (result!=MH_OK)
    { LOG("CE Anniversary hand scale stock fallback: create hook status %d",result);return false; }
    skinConvertHook.target=target;
    result=MH_EnableHook(target);
    if (result!=MH_OK)
    { LOG("CE Anniversary hand scale stock fallback: enable hook status %d",result);(void)RemoveSkin();return false; }
    skinConvertHook.enabled=true;skinInstalled=true;
    LOG("CE Anniversary hand scale installed: native copied FP bones retain their own scale across overlapping prepares; stock scale one unchanged");
    return true;
}
bool RemoveProjection() noexcept
{
    projectionInstalled=false;projectionRetiring=true;
    for (Hook* hook:{&projectionHook,&zfillProjectionHook,&sfxProjectionHook})
    {
        if (!hook->target||!hook->enabled) continue;
        const auto result=MCCVR_DisableHookForRetirement(hook->target);
        if (result!=MH_OK&&result!=MH_ERROR_DISABLED) return false;
        hook->enabled=false;
    }
    const void* functions[]={reinterpret_cast<const void*>(&ProjectionHook),reinterpret_cast<const void*>(&ZfillProjectionHook),
        reinterpret_cast<const void*>(&SfxProjectionHook)};
    const void* trampolines[]={projectionHook.original,zfillProjectionHook.original,sfxProjectionHook.original};
    if (!WaitForNativeDetourQuiescence(functions,trampolines,3,callbacks)) return false;
    for (Hook* hook:{&projectionHook,&zfillProjectionHook,&sfxProjectionHook})
    {
        if (hook->target&&MH_RemoveHook(hook->target)!=MH_OK) return false;
        *hook={};
    }
    projectionRetiring=false;return true;
}
bool InstallProjection(uintptr_t base,size_t size,uint32_t gen) noexcept
{
    const char* failure{};
    const NativeContractSet contracts{contract::first_person_projection::entries,
        contract::first_person_projection::witnesses,contract::first_person_projection::relatives,contract::first_person_projection::pointers};
    if (!VerifyNativeFeatureBindings(base,size,gen,contracts,failure))
    { LOG("CE Anniversary FP projection stock fallback: binding verification failed: %s",failure?failure:"unknown");return false; }
    const uintptr_t addresses[]={base+contract::first_person_projection::first_person_glt_constants,
        base+contract::first_person_projection::first_person_zfill_constants,
        base+contract::first_person_projection::first_person_sfx_constants};
    void* detours[]={reinterpret_cast<void*>(&ProjectionHook),reinterpret_cast<void*>(&ZfillProjectionHook),
        reinterpret_cast<void*>(&SfxProjectionHook)};
    Hook* hooks[]={&projectionHook,&zfillProjectionHook,&sfxProjectionHook};
    for (size_t i=0;i<3;++i)
    {
        void* target=reinterpret_cast<void*>(addresses[i]);
        const auto result=MH_CreateHook(target,detours[i],&hooks[i]->original);
        if (result!=MH_OK)
        { LOG("CE Anniversary FP projection stock fallback: create hook %zu status %d",i,result);(void)RemoveProjection();return false; }
        hooks[i]->target=target;
    }
    for (Hook* hook:hooks)
    {
        const auto result=MH_EnableHook(hook->target);
        if (result!=MH_OK)
        { LOG("CE Anniversary FP projection stock fallback: enable hook status %d",result);(void)RemoveProjection();return false; }
        hook->enabled=true;
    }
    projectionInstalled=true;
    LOG("CE Anniversary FP projection installed: native GLT/ZFILL/SFX preparation caches world-lens selection; later draws use their own eye matrix independently of palette and render-thread scheduling");
    return true;
}
bool RemoveParticleProjection() noexcept
{
    particleProjectionInstalled=false;particleProjectionRetiring=true;
    for (Hook* hook:{&particleDrawHook,&particleCommitHook})
    {
        if (!hook->target||!hook->enabled) continue;
        const auto result=MCCVR_DisableHookForRetirement(hook->target);
        if (result!=MH_OK&&result!=MH_ERROR_DISABLED) return false;
        hook->enabled=false;
    }
    const void* functions[]={reinterpret_cast<const void*>(&ParticleDrawHook),reinterpret_cast<const void*>(&ParticleCommitHook)};
    const void* trampolines[]={particleDrawHook.original,particleCommitHook.original};
    if (!WaitForNativeDetourQuiescence(functions,trampolines,2,callbacks)) return false;
    for (Hook* hook:{&particleDrawHook,&particleCommitHook})
    {
        if (hook->target&&MH_RemoveHook(hook->target)!=MH_OK) return false;
        *hook={};
    }
    particleProjectionRetiring=false;return true;
}
bool InstallParticleProjection(uintptr_t base,size_t size,uint32_t gen) noexcept
{
    const char* failure{};
    const NativeContractSet contracts{contract::first_person_particles::entries,
        contract::first_person_particles::witnesses,contract::first_person_particles::relatives,
        contract::first_person_particles::pointers};
    if (!VerifyNativeFeatureBindings(base,size,gen,contracts,failure))
    { LOG("CE Anniversary muzzle projection stock fallback: binding verification failed: %s",failure?failure:"unknown");return false; }
    const uintptr_t addresses[]={base+contract::first_person_particles::first_person_particle_draw,
        base+contract::first_person_particles::first_person_particle_commit};
    void* detours[]={reinterpret_cast<void*>(&ParticleDrawHook),reinterpret_cast<void*>(&ParticleCommitHook)};
    Hook* hooks[]={&particleDrawHook,&particleCommitHook};
    for (size_t i=0;i<2;++i)
    {
        void* target=reinterpret_cast<void*>(addresses[i]);
        const auto result=MH_CreateHook(target,detours[i],&hooks[i]->original);
        if (result!=MH_OK)
        { LOG("CE Anniversary muzzle projection stock fallback: create hook %zu status %d",i,result);(void)RemoveParticleProjection();return false; }
        hooks[i]->target=target;
    }
    for (Hook* hook:hooks)
    {
        const auto result=MH_EnableHook(hook->target);
        if (result!=MH_OK)
        { LOG("CE Anniversary muzzle projection stock fallback: enable hook status %d",result);(void)RemoveParticleProjection();return false; }
        hook->enabled=true;
    }
    particleProjectionInstalled=true;
    LOG("CE Anniversary muzzle projection installed: authored FP emitter attachments use the tracked weapon's primary-eye world lens; source emitters stay native");
    return true;
}
bool RemoveClassicLens() noexcept
{
    classicLensInstalled=false;classicLensRetiring=true;
    if (classicLensHook.target&&classicLensHook.enabled)
    {
        const auto result=MCCVR_DisableHookForRetirement(classicLensHook.target);
        if (result!=MH_OK&&result!=MH_ERROR_DISABLED) return false;
        classicLensHook.enabled=false;
    }
    const void* functions[]={reinterpret_cast<const void*>(&ClassicLensHook)};
    const void* trampolines[]={classicLensHook.original};
    if (!WaitForNativeDetourQuiescence(functions,trampolines,1,callbacks)) return false;
    if (classicLensHook.target&&MH_RemoveHook(classicLensHook.target)!=MH_OK) return false;
    classicLensHook={};classicLensRetiring=false;return true;
}
bool InstallClassicLens(uintptr_t base,size_t size,uint32_t gen) noexcept
{
    const char* failure{};
    const NativeContractSet contracts{contract::classic_first_person_projection::entries,
        contract::classic_first_person_projection::witnesses,contract::classic_first_person_projection::relatives,
        contract::classic_first_person_projection::pointers};
    if (!VerifyNativeFeatureBindings(base,size,gen,contracts,failure))
    { LOG("CE Original FP projection stock fallback: binding verification failed: %s",failure?failure:"unknown");return false; }
    void* target=reinterpret_cast<void*>(base+contract::classic_first_person_projection::first_person_classic_lens);
    auto result=MH_CreateHook(target,reinterpret_cast<void*>(&ClassicLensHook),&classicLensHook.original);
    if (result!=MH_OK)
    { LOG("CE Original FP projection stock fallback: create hook status %d",result);return false; }
    classicLensHook.target=target;
    result=MH_EnableHook(target);
    if (result!=MH_OK)
    { LOG("CE Original FP projection stock fallback: enable hook status %d",result);(void)RemoveClassicLens();return false; }
    classicLensHook.enabled=true;classicLensInstalled=true;
    LOG("CE Original FP projection installed: tracked weapon/hands retain each eye's world lens; native depth range preserved");
    return true;
}
bool RemoveAim() noexcept
{
    aimInstalled=false;aimRetiring=true;
    for (Hook* hook:{&modernRayHook,&legacyRayHook,&assistRayHook})
    {
        if (!hook->target||!hook->enabled) continue;
        const auto result=MCCVR_DisableHookForRetirement(hook->target);
        if (result!=MH_OK&&result!=MH_ERROR_DISABLED) return false;
        hook->enabled=false;
    }
    const void* functions[]={reinterpret_cast<const void*>(&ModernRayHook),reinterpret_cast<const void*>(&LegacyRayHook),
        reinterpret_cast<const void*>(&AssistRayHook)};
    const void* trampolines[]={modernRayHook.original,legacyRayHook.original,assistRayHook.original};
    if (!WaitForNativeDetourQuiescence(functions,trampolines,3,callbacks)) return false;
    for (Hook* hook:{&modernRayHook,&legacyRayHook,&assistRayHook})
    {
        if (hook->target&&MH_RemoveHook(hook->target)!=MH_OK) return false;
        *hook={};
    }
    aimRetiring=false;return true;
}
bool InstallAim(uintptr_t base,size_t size,uint32_t gen) noexcept
{
    const char* failure{};
    const NativeContractSet contracts{contract::first_person_aim::entries,
        contract::first_person_aim::witnesses,contract::first_person_aim::relatives,contract::first_person_aim::pointers};
    if (!VerifyNativeFeatureBindings(base,size,gen,contracts,failure))
    { LOG("CE controller aim stock fallback: binding verification failed: %s",failure?failure:"unknown");return false; }
    if (!retainedModule)
    {
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                reinterpret_cast<LPCWSTR>(base),&retainedModule)) return false;
        moduleBase=base;generation=gen;retiring=false;active=true;
    }
    const uintptr_t addresses[]={base+contract::first_person_aim::shot_adjust_modern,
        base+contract::first_person_aim::shot_adjust_legacy,base+contract::first_person_aim::shot_assist_director_ray};
    void* detours[]={reinterpret_cast<void*>(&ModernRayHook),reinterpret_cast<void*>(&LegacyRayHook),reinterpret_cast<void*>(&AssistRayHook)};
    Hook* hooks[]={&modernRayHook,&legacyRayHook,&assistRayHook};
    for (size_t i=0;i<3;++i)
    {
        void* target=reinterpret_cast<void*>(addresses[i]);
        const auto result=MH_CreateHook(target,detours[i],&hooks[i]->original);
        if (result!=MH_OK)
        { LOG("CE controller aim stock fallback: create hook %zu status %d",i,result);(void)RemoveAim();return false; }
        hooks[i]->target=target;
    }
    for (Hook* hook:hooks)
    {
        const auto result=MH_EnableHook(hook->target);
        if (result!=MH_OK)
        { LOG("CE controller aim stock fallback: enable hook status %d",result);(void)RemoveAim();return false; }
        hook->enabled=true;
    }
    aimInstalled=true;
    LOG("CE controller aim installed: local equipped on-foot shooter, native modern/legacy shot adjustment; engine origin/spread retained");
    return true;
}
bool Install(uintptr_t base,size_t size,uint32_t gen) noexcept
{
    const char* failure{};
    const NativeContractSet contracts{contract::first_person::entries,
        contract::first_person::witnesses,contract::first_person::relatives,contract::first_person::pointers};
    if (!VerifyNativeFeatureBindings(base,size,gen,contracts,failure))
    { LOG("CE first-person stock fallback: binding verification failed: %s",failure?failure:"unknown");return false; }
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
            reinterpret_cast<LPCWSTR>(base),&retainedModule)) return false;
    moduleBase=base;generation=gen;retiring=false;
    const uintptr_t addresses[]={base+contract::first_person::first_person_prepare,
        base+contract::first_person::first_person_graph_palette};
    void* detours[]={reinterpret_cast<void*>(&PrepareHook),reinterpret_cast<void*>(&PaletteHook)};
    Hook* hooks[]={&prepareHook,&paletteHook};
    for (size_t i=0;i<2;++i)
    {
        void* target=reinterpret_cast<void*>(addresses[i]);
        const auto result=MH_CreateHook(target,detours[i],&hooks[i]->original);
        if (result!=MH_OK)
        { LOG("CE first-person stock fallback: create hook %zu status %d",i,result);(void)Remove();return false; }
        hooks[i]->target=target;
    }
    for (Hook* hook:hooks)
    {
        const auto result=MH_EnableHook(hook->target);
        if (result!=MH_OK)
        { LOG("CE first-person stock fallback: enable hook status %d",result);(void)Remove();return false; }
        hook->enabled=true;
    }
    installed=true;active=true;
    LOG("CE first-person installed: native FP graph palette, independent wrists and weapon grip; waiting for coherent VR context");
    return true;
}
}
bool HaloCEFirstPerson_Poll(uintptr_t base,size_t size,uint32_t gen,bool isActive) noexcept
{
    active.store(isActive,std::memory_order_release);
    if (retainedModule&&(!isActive||base!=moduleBase||gen!=generation.load()||retiring.load()))
        if (!Remove()) return false;
    if (!isActive||!base||!gen) return false;
    if (!installed.load()&&gen!=failedGeneration)
        if (!Install(base,size,gen)) failedGeneration=gen;
    if (aimRetiring&&!RemoveAim()) return HaloCEFirstPerson_Armed();
    if (!aimInstalled.load()&&gen!=aimFailedGeneration&&!retiring.load())
        if (!InstallAim(base,size,gen)) aimFailedGeneration=gen;
    if (skinRetiring&&!RemoveSkin()) return HaloCEFirstPerson_Armed();
    if (installed.load()&&!skinInstalled.load()&&gen!=skinFailedGeneration&&!retiring.load())
        if (!InstallSkin(base,size,gen)) skinFailedGeneration=gen;
    if (projectionRetiring&&!RemoveProjection()) return HaloCEFirstPerson_Armed();
    if (installed.load()&&!projectionInstalled.load()&&gen!=projectionFailedGeneration&&!retiring.load())
        if (!InstallProjection(base,size,gen)) projectionFailedGeneration=gen;
    if (visibilityRetiring&&!RemoveVisibility()) return HaloCEFirstPerson_Armed();
    if (installed.load()&&!visibilityInstalled.load()&&gen!=visibilityFailedGeneration&&!retiring.load())
        if (!InstallVisibility(base,size,gen)) visibilityFailedGeneration=gen;
    if (particleProjectionRetiring&&!RemoveParticleProjection()) return HaloCEFirstPerson_Armed();
    if (installed.load()&&!particleProjectionInstalled.load()&&gen!=particleProjectionFailedGeneration&&!retiring.load())
        if (!InstallParticleProjection(base,size,gen)) particleProjectionFailedGeneration=gen;
    if (classicLensRetiring&&!RemoveClassicLens()) return HaloCEFirstPerson_Armed();
    if (installed.load()&&!classicLensInstalled.load()&&gen!=classicLensFailedGeneration&&!retiring.load())
        if (!InstallClassicLens(base,size,gen)) classicLensFailedGeneration=gen;
    const uint64_t now=GetTickCount64();
    if (now-lastReport>=2000)
    {
        lastReport=now;
        LOG("CE FP gen=%u installed=%d observed=%llu applied=%llu stock=%llu exceptions=%llu",
            gen,installed.load(),observed.load(),applied.load(),refused.load(),exceptions.load());
        if (alignmentKnownGraph.load() || alignmentUnknownGraph.load())
            LOG("CE hand alignment gen=%u verifiedGripGraph=%llu unknownGraphPriorMount=%llu; unknown graphs retain prior hand placement",
                gen,alignmentKnownGraph.load(),alignmentUnknownGraph.load());
        LOG("CE aim gen=%u installed=%d observed=%llu applied=%llu stock=%llu assist=%llu",
            gen,aimInstalled.load(),aimObserved.load(),aimApplied.load(),aimRefused.load(),assistApplied.load());
        LOG("CE Anniversary hand scale gen=%u installed=%d applied=%llu stock=%llu",
            gen,skinInstalled.load(),skinApplied.load(),skinRefused.load());
        LOG("CE Anniversary FP projection gen=%u installed=%d applied=%llu stock=%llu policy=%llu selector=%llu changed=%llu",
            gen,projectionInstalled.load(),projectionApplied.load(),projectionRefused.load(),
            projectionPolicyRefused.load(),projectionSelectorRefused.load(),projectionChangedRefused.load());
        LOG("CE Anniversary FP visibility gen=%u installed=%d models=%llu applied=%llu stock=%llu",
            gen,visibilityInstalled.load(),visibilityObserved.load(),visibilityApplied.load(),visibilityRefused.load());
        LOG("CE Original FP projection gen=%u installed=%d applied=%llu stock=%llu",
            gen,classicLensInstalled.load(),classicLensApplied.load(),classicLensRefused.load());
        LOG("CE Anniversary muzzle projection gen=%u installed=%d draws=%llu eyeRefused=%llu emitters=%llu stock=%llu",
            gen,particleProjectionInstalled.load(),particleDrawObserved.load(),particleEyeRefused.load(),
            particleProjectionApplied.load(),particleProjectionRefused.load());
    }
    return HaloCEFirstPerson_Armed();
}
bool HaloCEFirstPerson_AimArmed() noexcept
{
    RenderContext context{};
    return AimCurrent()&&HaloCE_GetGameplayContext(context)&&
        !context.tracking.controllers.controlsPresentationBlocked&&context.tracking.controllers.primaryAim.valid;
}
bool HaloCEFirstPerson_Armed() noexcept
{
    RenderContext context{};
    return CurrentPaletteContext(context);
}

bool HaloCEFirstPerson_GetLocalPlayerState(HaloCELocalPlayerState& state) noexcept
{
    Callback callback;
    return HaloCEControls_GetLocalPlayerState(state);
}

uint64_t HaloCEFirstPerson_WeaponGraph(uint32_t gen,uint64_t space,uint64_t now) noexcept
{
    PaletteReceipt receipt{};
    const uint64_t last=lastApplied.load(std::memory_order_acquire);
    if (!Current()||!last||now<last||now-last>150||!paletteReceipt.Read(receipt)||
        receipt.capturedAtMs!=last||receipt.context.tracking.generation!=gen||
        receipt.context.tracking.spaceEpoch!=space||
        receipt.context.tracking.controllers.controlsPresentationBlocked||
        !HaloCE_RenderContextCurrent(receipt.context)||
        lastApplied.load(std::memory_order_acquire)!=last) return 0;
    return receipt.weaponGraph;
}
