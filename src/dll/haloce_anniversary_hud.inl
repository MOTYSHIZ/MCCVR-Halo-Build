// Native HUD output routing is an independent optional feature. The forced
// two-view list uses bit0, while the native in-frame HUD branch requires bit1.
// E-CE-AHUD-1..4, HALOCE-ANNIVERSARY-HUD-EVIDENCE-2026-09-15.md.
using AnniversaryHudFn=void(__fastcall*)();
Hook anniversaryHudHook;
// 884de13 reached this replay once, failed native cleanup and stopped
// presenting after the graphics switch. Keep the complete adapter dormant;
// its native callback owns more state than our target/raster wrapper can
// restore. See HALOCE-ANNIVERSARY-REPLAY-ROLLBACK-2026-09-15.md.
constexpr bool kCeAnniversaryManualHudReplayEnabled=false;
// 22cb813 counted completed late HUD copies while the headset showed no HUD.
// Retain the adapter, but disable its unprepared raster/target transaction
// before implementing its independently verified replacement.
constexpr bool kCeAnniversaryUnpreparedHudReplayEnabled=false;
std::atomic<bool> anniversaryHudInstalled{};
std::atomic<bool> anniversaryHudNaturalInstalled{};
std::atomic<uint64_t> anniversaryHudDraws{},anniversaryHudFallbacks{};
std::atomic<uint32_t> anniversaryHudFailure{};
const char* AnniversaryHudFailureName(uint32_t value) noexcept
{
    switch (value)
    {
    case 0:return "none";
    case 1:return "legacy-admission";
    case 2:return "raster-or-target-stack";
    case 3:return "callback-or-restoration";
    case 4:return "restored-native-exception";
    case 5:return "unverified-cleanup";
    case 10:return "native-HUD-frame-bit";
    case 11:return "primary-eye-ownership";
    case 12:return "native-callback-owner";
    case 13:return "native-player-count";
    case 14:return "native-HUD-initialized";
    case 15:return "native-HUD-disabled";
    case 16:return "native-rendering-disabled";
    case 17:return "native-HUD-unavailable";
    case 18:return "native-display-mode";
    case 19:return "native-display-size";
    case 20:return "stock-camera";
    case 21:return "tracking-reference";
    case 22:return "output-source";
    case 30:return "eye-frame-scope";
    case 31:return "eye-core-ownership";
    case 32:return "eye-scene-index";
    case 33:return "eye-camera-identity";
    case 34:return "eye-consumer-stages";
    case 35:return "eye-reference-revision";
    case 36:return "eye-generation";
    case 37:return "eye-tracking-age";
    case 38:return "eye-camera-changed";
    case 39:return "eye-selected-camera";
    case 40:return "eye-ownership-changed";
    case 50:return "natural-HUD-frame";
    case 51:return "natural-HUD-packed-target";
    case 52:return "natural-HUD-gameplay-or-raster";
    case 53:return "natural-HUD-source-changed";
    default:return "unknown";
    }
}
struct AnniversaryHudReplay
{
    RenderContext owner;
    ID3D11DeviceContext* context{};
    UINT width{},height{},eyeWidth{},eyeHeight{};
    bool entered{},returned{},rasterRestored{true};
};
thread_local AnniversaryHudReplay* anniversaryHudReplay{};

// The normal native callback runs once, late in the frame, after the native
// copies have packed the two worlds. Only its gameplay HUD draw is framed
// twice. No callback, preamble, target push/pop or lock is invoked manually.
struct AnniversaryNaturalHud
{
    FrameScope* frame{};
    RenderContext owner;
    CeHudTargetSnapshot target;
    ID3D11DeviceContext* context{};
    bool entered{},complete{};
};
thread_local AnniversaryNaturalHud* anniversaryNaturalHud{};

bool AnniversaryHud_NaturalCurrent(const AnniversaryNaturalHud& hud) noexcept
{
    const auto* f=hud.frame;
    return !anniversaryMaterialMasked&&f&&f==frameScope&&f->capture&&f->synthetic&&f->prepared.valid&&
        f->diagnostic.eyeMask==3&&f->packedEyeMask==3&&f->packedResource&&
        f->prepared.referenceRevision==referenceRevision.load(std::memory_order_acquire)&&
        Current()&&Anniversary()&&HaloCE_RenderContextCurrent(hud.owner);
}
bool AnniversaryHud_BeginGameplay(ID3D11DeviceContext* context,UINT& width,UINT& height) noexcept
{
    auto* hud=anniversaryNaturalHud;
    if (!hud) return false;
    if (hud->entered) { hud->complete=false;return false; }
    if (!context||!AnniversaryHud_NaturalCurrent(*hud)) return false;
    auto& f=*hud->frame;
    anniversaryHudFailure=51;
    ResourceRegistry::Record resource{};
    if (!hudTargetBindingsVerified.load()||
        reinterpret_cast<uintptr_t>(context)!=f.diagnostic.copyContext[0]||
        f.diagnostic.copyContext[0]!=f.diagnostic.copyContext[1]||
        !resources.Read(f.packedResource,f.packedResource,resource)||resource.revision!=f.packedRevision||
        !HaloCEHudTarget_Read(bindings.base,context,hud->target)||hud->target.count!=1||
        hud->target.resources[0]!=f.packedResource||
        std::memcmp(&resource.descriptor,&f.packedDescriptor,sizeof(resource.descriptor))) return false;
    const auto& d=f.packedDescriptor;
    if (!d.Width||!d.Height||d.Height%2||d.Width!=f.diagnostic.sourceWidth||
        d.Height!=2*f.diagnostic.sourceHeight) return false;
    hud->context=context;hud->entered=true;
    width=d.Width;height=d.Height;
    anniversaryHudFailure=52;
    return true;
}
void AnniversaryHud_EndGameplay(bool complete) noexcept
{
    auto* hud=anniversaryNaturalHud;
    if (!hud||!hud->entered) return;
    CeHudTargetSnapshot after{};
    hud->complete=complete&&AnniversaryHud_NaturalCurrent(*hud)&&
        HaloCEHudTarget_Read(bindings.base,hud->context,after)&&
        after.backend==hud->target.backend&&after.descriptor==hud->target.descriptor&&
        after.count==hud->target.count&&after.resources[0]==hud->target.resources[0]&&
        after.rtvs[0]==hud->target.rtvs[0]&&after.dsv==hud->target.dsv;
}
void AnniversaryHud_NaturalBody(AnniversaryHudFn original,uintptr_t caller)
{
    auto* f=frameScope;
    if (!anniversaryHudNaturalInstalled.load()||anniversaryNaturalHud||anniversaryMaterialMasked||
        caller!=bindings.base+0x4572f5||!f||
        !f->capture||!(f->renderFlags&0x10)||f->diagnostic.eyeMask!=3)
    { original();return; }
    AnniversaryNaturalHud local{};local.frame=f;
    ReferenceSample sample{};
    uintptr_t players{};int16_t playerCount{};
    if (!Read(bindings.base+0x2ea2d90,players)||!Read(players+0xb4,playerCount)||playerCount!=1||
        !Read(bindings.base+0x2d9cb34,local.owner.camera)||!Valid(local.owner.camera)||
        !publishedReference.Read(sample)||sample.revision!=f->prepared.referenceRevision)
    { anniversaryHudFailure=50;anniversaryHudFallbacks.fetch_add(1);original();return; }
    local.owner.tracking=f->prepared.receipt.tracking;
    local.owner.reference=sample.value;local.owner.referenceRevision=sample.revision;
    local.owner.rendererEpoch=ceRendererEpoch.load();
    local.owner.unitsPerMeter=Game_GetWorldScale();local.owner.positional=Game_IsPositionalTracking();
    if (!AnniversaryHud_NaturalCurrent(local))
    { anniversaryHudFailure=50;anniversaryHudFallbacks.fetch_add(1);original();return; }
    anniversaryHudFailure=52;
    anniversaryNaturalHud=&local;
    bool returned=false;
    __try { original();returned=true; }
    __finally
    {
        anniversaryNaturalHud=nullptr;
        ResourceRegistry::Record after{};
        const bool committed=returned&&local.complete&&AnniversaryHud_NaturalCurrent(local)&&
            HaloCEHudTarget_CaptureSourceCurrent(local.target)&&
            resources.Read(f->packedResource,f->packedResource,after)&&after.revision==f->packedRevision&&
            !std::memcmp(&after.descriptor,&f->packedDescriptor,sizeof(after.descriptor))&&
            cache.CapturePacked(f->key,local.context,reinterpret_cast<ID3D11Resource*>(f->packedResource),after.descriptor);
        if (committed) { anniversaryHudFailure=0;anniversaryHudDraws.fetch_add(2,std::memory_order_relaxed); }
        else { if (local.complete) anniversaryHudFailure=53;anniversaryHudFallbacks.fetch_add(1,std::memory_order_relaxed); }
    }
}

void AnniversaryHudCallbackBody(uintptr_t caller=0)
{
    const auto original=reinterpret_cast<AnniversaryHudFn>(anniversaryHudHook.original);
    auto* replay=anniversaryHudReplay;
    if (!replay) { AnniversaryHud_NaturalBody(original,caller); return; }
    if (replay->entered||!HaloCE_RenderContextCurrent(replay->owner)) return;
    if (!HaloCEHudLayout_BeginEyeReplay(replay->context,replay->width,replay->height,
        replay->eyeWidth,replay->eyeHeight,&replay->rasterRestored)) return;
    replay->entered=true;
    __try { original(); replay->returned=true; }
    __finally { replay->rasterRestored=HaloCEHudLayout_EndEyeReplay(); }
}
void __fastcall AnniversaryHudCallbackHook()
{
    callbacks.fetch_add(1,std::memory_order_acq_rel);
    __try { AnniversaryHudCallbackBody(reinterpret_cast<uintptr_t>(_ReturnAddress())); }
    __finally { callbacks.fetch_sub(1,std::memory_order_release); }
}

bool AnniversaryHud_Remove() noexcept
{
    anniversaryHudInstalled=false;
    anniversaryHudNaturalInstalled=false;
    if (anniversaryHudHook.enabled)
    {
        const auto result=MCCVR_DisableHookForRetirement(anniversaryHudHook.target);
        if (result!=MH_OK&&result!=MH_ERROR_DISABLED) return false;
        anniversaryHudHook.enabled=false;
    }
    if (!anniversaryHudHook.target) return true;
    const void* detours[]{reinterpret_cast<void*>(&AnniversaryHudCallbackHook)};
    const void* originals[]{anniversaryHudHook.original};
    if (!WaitForNativeDetourQuiescence(detours,originals,1,callbacks)) return false;
    const auto result=MH_RemoveHook(anniversaryHudHook.target);
    if (result!=MH_OK&&result!=MH_ERROR_NOT_CREATED) return false;
    anniversaryHudHook={};
    return true;
}
bool AnniversaryHud_Install() noexcept
{
    if (!kCeAnniversaryManualHudReplayEnabled)
    {
        LOG("CE Anniversary HUD stock fallback: manual callback replay disabled after native cleanup failure; camera retained");
        return false;
    }
    const NativeContractSet set{contract::anniversary_hud::entries,contract::anniversary_hud::witnesses,
        contract::anniversary_hud::relatives,contract::anniversary_hud::pointers};
    const char* failure{};
    if (!VerifyNativeFeatureBindings(bindings.base,bindings.size,generation.load(),set,failure))
    { LOG("CE Anniversary HUD stock fallback: %s; camera retained",failure?failure:"binding verification"); return false; }
    auto* target=reinterpret_cast<void*>(bindings.base+contract::anniversary_hud::native_hud_callback);
    if (MH_CreateHook(target,reinterpret_cast<void*>(&AnniversaryHudCallbackHook),
        &anniversaryHudHook.original)!=MH_OK) return false;
    anniversaryHudHook.target=target;
    if (MH_EnableHook(target)!=MH_OK) { (void)AnniversaryHud_Remove(); return false; }
    anniversaryHudHook.enabled=true; anniversaryHudInstalled=true;
    LOG("CE Anniversary HUD installed: per-eye native callback before exact source copy; independent target/raster restoration, visible result unverified");
    return true;
}

bool AnniversaryHud_InstallNatural() noexcept
{
    if (!kCeAnniversaryUnpreparedHudReplayEnabled)
    { LOG("CE Anniversary unprepared HUD replay disabled after 22cb813 headset failure; camera retained");return false; }
    const NativeContractSet set{contract::anniversary_hud::entries,contract::anniversary_hud::witnesses,
        contract::anniversary_hud::relatives,contract::anniversary_hud::pointers};
    const char* failure{};
    if (!hudTargetBindingsVerified.load()||
        !VerifyNativeFeatureBindings(bindings.base,bindings.size,generation.load(),set,failure))
    { LOG("CE Anniversary HUD stock fallback: %s; camera retained",failure?failure:"cold target proof");return false; }
    auto* target=reinterpret_cast<void*>(bindings.base+contract::anniversary_hud::native_hud_callback);
    if (MH_CreateHook(target,reinterpret_cast<void*>(&AnniversaryHudCallbackHook),&anniversaryHudHook.original)!=MH_OK) return false;
    anniversaryHudHook.target=target;
    if (MH_EnableHook(target)!=MH_OK) { (void)AnniversaryHud_Remove();return false; }
    anniversaryHudHook.enabled=true;anniversaryHudNaturalInstalled=true;
    LOG("CE Anniversary HUD installed: ordinary late native callback, gameplay HUD in both packed eyes; manual callback replay remains disabled");
    return true;
}

struct AnniversaryHudSource
{
    uintptr_t root{},surface{},resource{},context{},backend{};
    ResourceRegistry::Record record;
    int32_t index{};
};
bool AnniversaryHud_ReadSource(AnniversaryHudSource& out) noexcept
{
    AnniversaryHudSource next{}; uintptr_t vtable{},context{};
    if (!Read(bindings.base+0x1b7b11c,next.index)||next.index<0||next.index>1||
        !Read(bindings.base+0x2d62890+next.index*8,next.root)||!next.root||
        !Read(next.root,vtable)||vtable!=bindings.base+0x17fb608||
        !Read(bindings.base+0x2e3bde0,next.backend)||!next.backend||
        !Read(next.backend,vtable)||vtable!=bindings.base+0x17f9d10||
        !Read(next.backend+0xce0,next.context)||!next.context||
        !Read(bindings.base+0x2ea2d30,context)||context!=next.context) return false;
    __try { next.surface=reinterpret_cast<uintptr_t(__fastcall*)(uintptr_t)>(bindings.surfaceSelector)(next.root); }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
    if (!next.surface||!Read(next.surface,vtable)||vtable!=bindings.base+0x17fb608||
        !Read(next.surface+0xe0,next.resource)||!next.resource||
        !resources.Read(next.resource,next.resource,next.record)) return false;
    out=next; return true;
}
void AnniversaryHud_ReplayEyeBody(FrameScope& scope,int eye,bool& cleanupVerified)
{
    if (!anniversaryHudInstalled.load()||!scope.synthetic||!scope.capture||
        eye<0||eye>1||anniversaryHudReplay) return;
    const uint32_t mask=1u<<eye;
    if (scope.hudAttempted&mask) return;
    scope.hudAttempted|=mask;
    uint32_t failure=1;
    AnniversaryHudReplay replay{};
    AnniversaryHudSource source{};
    ReferenceSample sample{};
    uintptr_t callback{},players{},config{},display{},enabledPointer{},stackStorage{};
    uintptr_t previousOutput{};
    uint8_t nativeInitialized{},nativeDisabled{},nativeRendering{},hudAvailable{};
    int16_t playerCount{}; int32_t stereo{},nativeStackDepth{},nativeStackCapacity{};
    std::array<uint8_t,0x48> targetDescriptor{};
    constexpr UINT rasterCapacity=D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    D3D11_VIEWPORT previousViewports[rasterCapacity]{};
    D3D11_RECT previousScissors[rasterCapacity]{};
    UINT previousViewportCount{},previousScissorCount{};
    bool targetRestored=false;
    const auto camera=reinterpret_cast<const SaberCamera*>(scope.renderer+0xf0+eye*sizeof(SaberView));
    // Keep each optional admission refusal attributable in the cold log.
    // Earlier builds collapsed all of these into failure 1, hiding which
    // native state actually prevented HUD replay on the user's machine.
    failure=10;
    if (!(scope.renderFlags&0x10)) goto failed;
    failure=11;
    if (scope.diagnostic.nativeFlags!=1) goto failed;
    failure=AnniversaryEyeTracking(camera,replay.owner.tracking);
    if (failure) goto failed;
    failure=12;
    if (!Read(bindings.base+0x1c33fe0,callback)||callback!=bindings.base+contract::anniversary_hud::native_hud_callback) goto failed;
    failure=13;
    if (!Read(bindings.base+0x2ea2d90,players)||!Read(players+0xb4,playerCount)||playerCount!=1) goto failed;
    failure=14;
    if (!Read(bindings.base+0x2d9bdd1,nativeInitialized)||!nativeInitialized) goto failed;
    failure=15;
    if (!Read(bindings.base+0x2b23700,nativeDisabled)||nativeDisabled) goto failed;
    failure=16;
    if (!Read(bindings.base+0x2d91330,enabledPointer)||!Read(enabledPointer+2,nativeRendering)||!nativeRendering) goto failed;
    failure=17;
    if (!Read(bindings.base+0x2e3b829,hudAvailable)||!hudAvailable) goto failed;
    failure=18;
    if (!Read(bindings.base+0x2e3bdd8,config)||!Read(config+0x238,stereo)||stereo) goto failed;
    failure=19;
    if (!Read(config+0x118,display)||!Read(display+0x10,replay.width)||!Read(display+0x14,replay.height)) goto failed;
    failure=20;
    if (!Read(bindings.base+0x2d9cb34,replay.owner.camera)||!Valid(replay.owner.camera)) goto failed;
    failure=21;
    if (!publishedReference.Read(sample)||sample.revision!=scope.prepared.referenceRevision) goto failed;
    failure=22;
    if (!AnniversaryHud_ReadSource(source)) goto failed;
    failure=2;
    replay.owner.reference=sample.value; replay.owner.referenceRevision=sample.revision;
    replay.owner.rendererEpoch=ceRendererEpoch.load();
    replay.owner.unitsPerMeter=Game_GetWorldScale(); replay.owner.positional=Game_IsPositionalTracking();
    replay.context=reinterpret_cast<ID3D11DeviceContext*>(source.context);
    replay.eyeWidth=source.record.descriptor.Width; replay.eyeHeight=source.record.descriptor.Height;
    if (!HaloCE_RenderContextCurrent(replay.owner)||
        !std::isfinite(replay.owner.unitsPerMeter)||replay.owner.unitsPerMeter<=0||replay.owner.unitsPerMeter>10||
        replay.width!=replay.eyeWidth||replay.height!=2*replay.eyeHeight||
        replay.eyeWidth!=scope.prepared.receipt.pair.cameras[eye].viewportWidth||
        replay.eyeHeight!=scope.prepared.receipt.pair.cameras[eye].viewportHeight||
        replay.owner.camera.viewport.left||replay.owner.camera.viewport.top||
        replay.owner.camera.viewport.right!=replay.width||replay.owner.camera.viewport.bottom!=replay.height||
        !Read(bindings.base+0x2e3d0d0,previousOutput)||
        !Read(source.backend+0x10,nativeStackDepth)||nativeStackDepth<0||
        !Read(source.backend+0x14,nativeStackCapacity)||nativeStackCapacity<2||
        nativeStackCapacity>4096||nativeStackDepth>nativeStackCapacity-2||
        !Read(source.backend+8,stackStorage)||!stackStorage||
        !Read(source.backend+0x18,targetDescriptor)||
        !HaloCEHudLayout_CopyState(replay.context,&previousViewportCount,previousViewports,
            &previousScissorCount,previousScissors)) goto failed;
    {
        // Both native pushes fit the existing stack: this adapter never grows
        // native storage. The callback borrows kind1/2 and restores them itself.
        // Our outer push/pop restores the pre-HUD backend target and viewport.
        bool pushed=false,borrowed=false;
        cleanupVerified=false;
        __try
        {
            reinterpret_cast<AnniversaryHudFn>(bindings.base+contract::anniversary_hud::native_target_push)();
            pushed=true;
            *reinterpret_cast<uintptr_t*>(bindings.base+0x2e3d0d0)=source.root; borrowed=true;
            anniversaryHudReplay=&replay;
            reinterpret_cast<AnniversaryHudFn>(bindings.base+contract::anniversary_hud::native_hud_preamble)();
        }
        __finally
        {
            anniversaryHudReplay=nullptr;
            uintptr_t outputAfter{};
            const bool outputOwned=borrowed&&Read(bindings.base+0x2e3d0d0,outputAfter)&&outputAfter==source.root;
            if (outputOwned) *reinterpret_cast<uintptr_t*>(bindings.base+0x2e3d0d0)=previousOutput;
            if (pushed)
            {
                uintptr_t backendAfter{},contextAfter{},storageAfter{};
                int32_t depthAfter{};
                std::array<uint8_t,0x48> descriptorAfter{};
                const bool popOwned=Read(bindings.base+0x2e3bde0,backendAfter)&&
                    backendAfter==source.backend&&Read(source.backend+0xce0,contextAfter)&&
                    contextAfter==source.context&&Read(source.backend+8,storageAfter)&&
                    storageAfter==stackStorage&&Read(source.backend+0x10,depthAfter)&&
                    depthAfter==nativeStackDepth+1;
                const auto popped=popOwned?reinterpret_cast<uint8_t(__fastcall*)()>(
                    bindings.base+contract::anniversary_hud::native_target_pop)():0;
                targetRestored=outputOwned&&popped&&Read(source.backend+0x10,depthAfter)&&
                    depthAfter==nativeStackDepth&&Read(source.backend+0x18,descriptorAfter)&&
                    descriptorAfter==targetDescriptor;
                if (targetRestored)
                {
                    // The native pop restores target dimensions, not an earlier
                    // custom viewport. Restore its separately observed numeric
                    // raster only after the descriptor and stack match again.
                    replay.context->RSSetViewports(previousViewportCount,previousViewports);
                    replay.context->RSSetScissorRects(previousScissorCount,previousScissors);
                }
            }
            cleanupVerified=targetRestored&&replay.rasterRestored;
        }
    }
    if (!cleanupVerified)
    {
        // Unknown target/raster ownership invalidates only this frame. Keep
        // the native hooks and XR lifecycle available for the next frame.
        scope.capture=false;
        scope.diagnostic.failure=FrameFailure::IncompletePair;
        failure=5;
        goto failed;
    }
    failure=3;
    {
        AnniversaryHudSource after{};
        if (!replay.entered||!replay.returned||!replay.rasterRestored||!targetRestored||
            !HaloCE_RenderContextCurrent(replay.owner)||!AnniversaryHud_ReadSource(after)||
            after.root!=source.root||after.surface!=source.surface||after.resource!=source.resource||
            after.context!=source.context||after.record.revision!=source.record.revision) goto failed;
    }
    anniversaryHudFailure.store(0,std::memory_order_relaxed);
    anniversaryHudDraws.fetch_add(1,std::memory_order_relaxed); return;
failed:
    anniversaryHudFailure.store(failure,std::memory_order_relaxed);
    anniversaryHudFallbacks.fetch_add(1,std::memory_order_relaxed);
}
void AnniversaryHud_ReplayEye(FrameScope& scope,int eye)
{
    bool cleanupVerified=true;
    __try { AnniversaryHud_ReplayEyeBody(scope,eye,cleanupVerified); }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        // Native optional HUD callbacks can fault after taking ownership.
        // Their finally blocks run before this handler; retain the world pair
        // only when those blocks proved target, stack and raster restoration.
        if (!cleanupVerified)
        {
            scope.capture=false;
            scope.diagnostic.failure=FrameFailure::IncompletePair;
        }
        anniversaryHudFailure.store(cleanupVerified?4u:5u,std::memory_order_relaxed);
        anniversaryHudFallbacks.fetch_add(1,std::memory_order_relaxed);
    }
}
