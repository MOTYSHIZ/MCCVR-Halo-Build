// Production CE crosshair transaction and lifetime tests. Shared D3D/XR calls
// are explicit fixtures: these do not establish native HUD pixels or framing.
#include "../src/dll/haloce_hud.cpp"
#include <cstdio>
#include <vector>

static GameTitle testTitle=GameTitle::HaloCE;
static uint32_t testGeneration=3;
static halo_ce::RenderContext testContext{};
static bool contextValid=true,beginAllowed=true,endAllowed=true;
static bool redirectActive{},redirectAuthored{},raiseNative{},reenterNative{},changeRevision{};
static unsigned nativeCalls{},captureBegins{},suppressionBegins{},ends{},invalidations{};
static unsigned layoutSuspensions{};
static bool nativeFramingIsolated=true,canPrepare=true,suppressionReady=true;
static AuthoredReticlePreparationResult preparationResult=AuthoredReticlePreparationResult::Ready;
static bool aimAvailable=true,playerEligible=true;
static bool coldTargetProof=true;
bool HaloCE_HudTargetBindingsVerified(uintptr_t base,size_t size,uint32_t gen) noexcept
{ return coldTargetProof&&base==moduleBase&&size==halo_ce::contract::imageSize&&gen==testGeneration; }
bool HaloCEFirstPerson_AimArmed() noexcept { return aimAvailable; }
bool HaloCEFirstPerson_GetLocalPlayerState(HaloCELocalPlayerState& state) noexcept
{
    state.hasControlledUnit=state.onFoot=state.nativePreparesFirstPerson=playerEligible;
    state.nativeInputBlocked=state.nativeLookBlocked=false;
    return true;
}
bool HaloCEHudTarget_Read(uintptr_t,ID3D11DeviceContext*,CeHudTargetSnapshot&) noexcept { return false; }
void HaloCEHudLayout_Suspend() noexcept { ++layoutSuspensions; }
void HaloCEHudLayout_Resume() noexcept { --layoutSuspensions; }
GameTitle TitleAdapter_GetActiveTitle() { return testTitle; }
uint32_t TitleAdapter_GetGeneration(GameTitle) { return testGeneration; }
bool HaloCE_Armed() noexcept { return testTitle==GameTitle::HaloCE; }
bool HaloCE_GetRenderContext(const halo_ce::Camera&,halo_ce::RenderContext& out) noexcept
{ out=testContext;return contextValid; }
bool HaloCE_RenderContextCurrent(const halo_ce::RenderContext& context) noexcept
{
    return contextValid&&testTitle==GameTitle::HaloCE&&context.tracking.generation==testGeneration&&
        context.referenceRevision==testContext.referenceRevision&&
        context.rendererEpoch==testContext.rendererEpoch;
}
bool VR_CeAuthoredReticleFrameMatches(ID3D11DeviceContext* context,uint64_t serial)
{ return context==reinterpret_cast<ID3D11DeviceContext*>(0x12340)&&serial==testContext.tracking.serial; }
bool VR_CanPrepareAuthoredReticleResources() { return canPrepare; }
AuthoredReticlePreparationResult VR_PrepareAuthoredReticleResources()
{ return preparationResult; }
bool VR_PrepareAuthoredReticleSuppressionResources() { return suppressionReady; }
bool VR_BeginPreparedAuthoredReticleCapture()
{
    ++captureBegins;
    if (!beginAllowed||redirectActive) return false;
    redirectActive=redirectAuthored=true;return true;
}
bool VR_BeginPreparedAuthoredReticleSuppression()
{
    ++suppressionBegins;
    if (!beginAllowed||redirectActive) return false;
    redirectActive=true;redirectAuthored=false;return true;
}
bool VR_EndPreparedAuthoredReticleCapture()
{ ++ends;const bool match=redirectActive&&redirectAuthored;redirectActive=false;return match&&endAllowed; }
bool VR_EndPreparedAuthoredReticleSuppression()
{ ++ends;const bool match=redirectActive&&!redirectAuthored;redirectActive=false;return match&&endAllowed; }
void VR_InvalidatePreparedAuthoredReticleCapture() { ++invalidations; }
void Logf(const char*,...) { }
bool WaitForNativeDetourQuiescence(const void* const*,const void* const*,size_t,
    const std::atomic<uint32_t>& count) { return !count.load(); }
static void __fastcall NativeCrosshair(int32_t user,uint32_t weapon,uint32_t hud,const void* state)
{
    ++nativeCalls;
    nativeFramingIsolated=nativeFramingIsolated&&layoutSuspensions>0;
    if (reenterNative) { reenterNative=false;CrosshairHook(user,weapon,hud,state); }
    if (changeRevision) ++testContext.referenceRevision;
    if (raiseNative) RaiseException(0xe0424242,0,0,nullptr);
}
static bool ExceptionPassesThrough()
{
    __try { CrosshairHook(0,17,29,nullptr); }
    __except(GetExceptionCode()==0xe0424242?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH)
    { return true; }
    return false;
}
int main()
{
    unsigned failures=0;
    const auto check=[&](bool ok,const char* text) {
        if (!ok) { ++failures;std::fprintf(stderr,"CE HUD: %s\n",text); }
    };
    std::vector<uint8_t> memory(halo_ce::contract::imageSize);
    moduleBase=reinterpret_cast<uintptr_t>(memory.data());
    const uintptr_t nativeContext=0x12340;
    std::memcpy(memory.data()+0x2ea2d30,&nativeContext,sizeof(nativeContext));
    active=installed=prepared=true;retiring=false;generation=3;original=&NativeCrosshair;
    testContext.tracking.generation=3;testContext.tracking.serial=10;
    testContext.tracking.controllers.primaryAim.valid=true;
    testContext.tracking.controllers.controlsPresentationBlocked=false;
    testContext.referenceRevision=2;testContext.rendererEpoch=4;
    prepared=false;targetBindingsVerified=false;
    coldTargetProof=false;
    PrepareCapture(moduleBase,memory.size(),3);
    check(!prepared.load()&&rejectedCaptureGeneration==3&&HaloCEHud_HasCrosshairScope(),
        "missing cold target proof isolates capture without disarming native HUD");
    coldTargetProof=true;rejectedCaptureGeneration=0;
    canPrepare=false;
    PrepareCapture(moduleBase,memory.size(),3);
    check(targetBindingsVerified&&!prepared.load()&&!rejectedCaptureGeneration,
        "current core proof admits target bindings without rescanning patched native bytes");
    canPrepare=true;
    check(HaloCEHud_HasCrosshairScope(),"native crosshair scope does not depend on private capture resources");
    CrosshairHook(0,17,29,nullptr);
    check(!captureBegins&&nativeCalls==1&&nativeFramingIsolated&&!layoutSuspensions,
        "pending capture runs native art with the gameplay HUD affine suspended");
    preparationResult=AuthoredReticlePreparationResult::Failed;
    PrepareCapture(moduleBase,memory.size(),3);
    check(HaloCEHud_HasCrosshairScope()&&!prepared.load()&&rejectedCaptureGeneration==3,
        "optional capture preparation failure retains native crosshair scope and HUD independence");
    preparationResult=AuthoredReticlePreparationResult::Ready;rejectedCaptureGeneration=0;
    suppressionReady=false;PrepareCapture(moduleBase,memory.size(),3);
    check(HaloCEHud_HasCrosshairScope()&&!prepared.load()&&rejectedCaptureGeneration==3,
        "optional discard preparation failure cannot remove the crosshair boundary");
    suppressionReady=true;rejectedCaptureGeneration=0;canPrepare=false;
    PrepareCapture(moduleBase,memory.size(),3);
    check(HaloCEHud_HasCrosshairScope()&&!prepared.load()&&!rejectedCaptureGeneration,
        "resources not ready yet retain scope and allow a later preparation retry");
    canPrepare=true;PrepareCapture(moduleBase,memory.size(),3);
    check(prepared.load(),"ready optional resources enable capture without reinstalling the native scope");
    nativeCalls=0;
    CrosshairHook(0,17,29,nullptr);
    const auto key=HaloCEHud_CrosshairKey();
    check(key&&captureBegins==1&&nativeCalls==1&&ends==1&&!redirectActive&&!callbacks.load()&&!layoutSuspensions,
        "first native phase completes capture and retires its scope");
    CrosshairHook(0,17,29,nullptr);
    check(suppressionBegins==1&&nativeCalls==2&&HaloCEHud_CrosshairKey()==key,
        "second eye runs native logic in discard phase without replacing art identity");
    ++testContext.tracking.serial;
    CrosshairHook(0,18,30,nullptr);
    check(HaloCEHud_CrosshairKey()!=key,"weapon and HUD change publish a different art identity");
    ++testContext.tracking.serial;reenterNative=true;
    const unsigned before=captureBegins;
    CrosshairHook(0,18,30,nullptr);
    check(captureBegins==before+1&&!drawing&&!redirectActive&&!callbacks.load(),
        "nested native work never opens a second redirect or leaks callback ownership");
    ++testContext.tracking.serial;beginAllowed=false;
    const unsigned fallbackBefore=nativeCalls;
    CrosshairHook(0,17,29,nullptr);
    check(nativeCalls==fallbackBefore+1&&!HaloCEHud_CapturedCrosshair()&&!redirectActive,
        "failed optional capture still runs stock native drawing and revokes authored ownership");
    beginAllowed=true;
    CrosshairHook(0,17,29,nullptr);
    check(HaloCEHud_CapturedCrosshair(),"next native phase recovers without camera or hook reinstallation");
    beginAllowed=false;
    CrosshairHook(0,17,29,nullptr);
    beginAllowed=true;
    const unsigned recaptures=captureBegins;
    CrosshairHook(0,17,29,nullptr);
    check(captureBegins==recaptures+1&&HaloCEHud_CapturedCrosshair(),
        "failed second-eye suppression requires a real new capture before claiming authored art again");
    ++testContext.tracking.serial;endAllowed=false;
    CrosshairHook(0,17,29,nullptr);endAllowed=true;
    check(!HaloCEHud_CapturedCrosshair()&&invalidations&&!redirectActive,
        "failed capture completion invalidates shared art and restores redirect scope");
    ++testContext.tracking.serial;changeRevision=true;
    CrosshairHook(0,17,29,nullptr);changeRevision=false;
    check(!HaloCEHud_CapturedCrosshair(),"recenter during native callback cannot publish old-owner pixels");
    ++testContext.tracking.serial;raiseNative=true;
    check(ExceptionPassesThrough(),"native structured exception retains native propagation");raiseNative=false;
    check(!callbacks.load()&&!drawing&&!redirectActive&&!HaloCEHud_CapturedCrosshair(),
        "exception finally restores feature scope and invalidates unfinished capture");
    CrosshairHook(0,17,29,nullptr);
    check(HaloCEHud_CapturedCrosshair(),"successful phase recovers after native exception fixture");
    ++testContext.rendererEpoch;
    check(!HaloCEHud_CapturedCrosshair(),"graphics-mode epoch rejects old capture identity");
    const unsigned modeCaptures=captureBegins;
    CrosshairHook(0,17,29,nullptr);
    check(captureBegins==modeCaptures+1,"new renderer cannot claim pixels from a discard-only phase");
    const unsigned eligibleCaptures=captureBegins;
    aimAvailable=false;
    CrosshairHook(0,17,29,nullptr);
    check(captureBegins==eligibleCaptures&&!HaloCEHud_CapturedCrosshair(),
        "unavailable controller aim leaves the native reticle visible and revokes old art");
    aimAvailable=true;playerEligible=false;
    CrosshairHook(0,17,29,nullptr);
    check(captureBegins==eligibleCaptures&&!HaloCEHud_CapturedCrosshair(),
        "unproven on-foot ownership cannot hide the native reticle");
    playerEligible=true;
    CrosshairHook(0,17,29,nullptr);
    check(captureBegins==eligibleCaptures+1&&HaloCEHud_CapturedCrosshair(),
        "eligible local player must recapture after stock fallback");
    ++testGeneration;
    check(!HaloCEHud_CapturedCrosshair(),"module generation change rejects previous crosshair ownership");
    --testGeneration;testTitle=GameTitle::Halo3;
    check(!HaloCEHud_CapturedCrosshair(),"CE does not own another title's reticle");
    testTitle=GameTitle::HaloCE;
    CaptureReceipt receipt{};lastCapture.Read(receipt);receipt.capturedAtMs=GetTickCount64()-300;
    lastCapture.Publish(receipt);
    check(!HaloCEHud_CapturedCrosshair(),"missing native phases cannot indefinitely retain captured-once ownership");
    check(nativeFramingIsolated,"every native phase including stock fallbacks excludes gameplay HUD framing");
    return failures?1:0;
}
