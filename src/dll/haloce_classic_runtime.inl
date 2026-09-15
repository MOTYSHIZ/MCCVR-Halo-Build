// Included inside haloce_stereo_core.cpp's private namespace, after LiveGame.
// E-CE-C1..C4, docs/HALOCE-CLASSIC-EVIDENCE-2026-09-15.md.
// Classic owns a separate hook transaction: optional installation failure must
// not remove an installed Anniversary camera or any existing title's hooks.
using ClassicGameRenderFn=void(__fastcall*)(float,float);
using ClassicWindowFn=void(__fastcall*)(Window*);
using ClassicBlitFn=void(__fastcall*)(const halo_ce::Rectangle*);
using ClassicViewFn=void(__fastcall*)(int16_t,const Camera*,const void*,const Camera*,const void*,int16_t,uint8_t);
enum ClassicHookIndex { ClassicGameRender,ClassicPlayerWindow,ClassicFinalBlit,ClassicMainView,ClassicHookCount };
std::array<Hook,ClassicHookCount> classicHooks;
std::atomic<bool> classicInstalled{};
std::atomic<int32_t> ceObservedRenderer{-1};
std::atomic<uint64_t> ceRendererEpoch{1};
std::atomic<uint64_t> classicPairs{},classicDrops{},classicStock{},classicOutputs{},classicSourceMiss{};
enum class ClassicFailure : uint32_t { None,ScopeChanged,WindowCount,PairChanged,
    PairPreparation,CameraState,Consumer,Source,SourceChanged,OutputShape,IncompletePair };
std::atomic<ClassicFailure> classicLastFailure{};
uint32_t classicRejectedGeneration{};

int32_t CeObserveRendererMode() noexcept
{
    int32_t mode=-1;
    if (!Read(bindings.base+0x1b7aa84,mode)) return -1;
    int32_t previous=ceObservedRenderer.load(std::memory_order_acquire);
    if (mode!=previous&&ceObservedRenderer.compare_exchange_strong(previous,mode,
        std::memory_order_acq_rel))
    {
        ceRendererEpoch.fetch_add(1,std::memory_order_acq_rel);
        referenceRevision.fetch_add(1,std::memory_order_acq_rel);
        recenter.store(true,std::memory_order_release);
        completedFrame.Publish({});
    }
    return mode;
}

struct ClassicNativeSource
{
    uintptr_t wrapper{},resolved{},rtv{},resource{},context{};
    ResourceRegistry::Record record;
};
bool ClassicSource(ClassicNativeSource& out) noexcept
{
    ClassicNativeSource value{};
    uintptr_t vtable{},views{},nativeView{},backend{},backendContext{};
    if (!Read(bindings.base+0x2e3b910,value.wrapper)||!value.wrapper||
        !Read(value.wrapper,vtable)||vtable!=bindings.base+0x17fb608||
        !Read(bindings.base+0x1b85e78,nativeView)||!nativeView||
        !Read(bindings.base+0x2ea2d30,value.context)||!value.context||
        !Read(bindings.base+0x2e3bde0,backend)||!backend||
        !Read(backend+0xce0,backendContext)||backendContext!=value.context)
        return false;
    __try
    {
        // Verified native selector is read-only. It chooses the same variants
        // used by texture virtual+D8. Never assume wrapper+E0 is selected.
        value.resolved=reinterpret_cast<uintptr_t(__fastcall*)(uintptr_t)>(
            bindings.surfaceSelector)(value.wrapper);
    }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
    if (!value.resolved||!Read(value.resolved+0xe8,views)||!views||
        !Read(views,value.rtv)||value.rtv!=nativeView||
        !Read(value.resolved+0xe0,value.resource)||!value.resource||
        !resources.Read(value.resource,value.resource,value.record)) return false;
    out=value;
    return true;
}

struct ClassicFrameScope
{
    ClassicViewPair pair;
    Reference reference;
    EyeCache::Key key;
    float scale{};
    bool positional{},prepared{},failed{},capture{};
    int eye{};
    uint32_t generation{},windows[2]{},views[2]{},outputs[2]{};
    uint64_t epoch{},revision{};
    int32_t tick{};
    uintptr_t window{},clock{};
    ClassicNativeSource sources[2];
    ClassicFailure failure{};
    void Fail(ClassicFailure reason) noexcept
    { failed=true; if (failure==ClassicFailure::None) failure=reason; }
};
thread_local ClassicFrameScope* classicFrameScope{};

bool ClassicGetRenderContext(RenderContext& out) noexcept
{
    const auto* scope=classicFrameScope;
    if (!scope||!scope->prepared||scope->failed||scope->eye<0||scope->eye>1||
        !Current()||CeObserveRendererMode()!=0||scope->epoch!=ceRendererEpoch.load()||
        scope->revision!=referenceRevision.load()) return false;
    out={scope->pair.tracking,scope->reference,scope->pair.source.render,
        scope->scale,scope->positional,scope->revision,scope->epoch};
    return true;
}

bool ClassicScopeCurrent(const ClassicFrameScope& scope) noexcept
{
    uintptr_t clock{};
    int32_t tick{};
    return Current()&&classicInstalled.load(std::memory_order_acquire)&&
        armed.load(std::memory_order_acquire)&&trackingEnabled.load(std::memory_order_acquire)&&
        CeObserveRendererMode()==0&&scope.generation==generation.load()&&
        scope.epoch==ceRendererEpoch.load()&&scope.revision==referenceRevision.load()&&
        Read(bindings.base+0x2e9fd68,clock)&&clock==scope.clock&&
        Read(clock+0xc,tick)&&tick==scope.tick;
}

// BBCF30 receives the actual culling and drawing cameras, after fog clipping
// and optional reflection construction. Verify the normal class-1 call owns
// both cameras in our live window; a copied or stale camera cannot count as an
// eye just because a final blit eventually occurs.
void ClassicViewBody(int16_t player,const Camera* render,const void* renderFrustum,
    const Camera* raster,const void* rasterFrustum,int16_t kind,uint8_t reflected,
    uintptr_t caller)
{
    auto* scope=classicFrameScope;
    if (scope&&scope->prepared&&!scope->failed&&caller==bindings.base+0xbbccb2)
    {
        Camera consumedRender{},consumedRaster{},expectedRender=scope->pair.eyes[scope->eye].render;
        const auto& expectedRaster=scope->pair.eyes[scope->eye].raster;
        const bool readable=Read(reinterpret_cast<uintptr_t>(render),consumedRender)&&
            Read(reinterpret_cast<uintptr_t>(raster),consumedRaster);
        // Native fog may lower render far-plane (E-CE-1). It does not change
        // tracked origin, basis, FOV, raster or the raster camera's clip range.
        expectedRender.farPlane=consumedRender.farPlane;
        if (++scope->views[scope->eye]!=1||!ClassicScopeCurrent(*scope)||
            !readable||player!=scope->pair.source.player||kind!=1||
            reinterpret_cast<uintptr_t>(render)!=scope->window+offsetof(Window,render)||
            reinterpret_cast<uintptr_t>(raster)!=scope->window+offsetof(Window,raster)||
            !renderFrustum||!rasterFrustum||renderFrustum==rasterFrustum||
            !Valid(consumedRender)||!Valid(consumedRaster)||
            std::memcmp(&consumedRender,&expectedRender,sizeof(Camera))||
            std::memcmp(&consumedRaster,&expectedRaster,sizeof(Camera)))
            scope->Fail(ClassicFailure::Consumer);
    }
    reinterpret_cast<ClassicViewFn>(classicHooks[ClassicMainView].original)(
        player,render,renderFrustum,raster,rasterFrustum,kind,reflected);
}
void __fastcall ClassicViewHook(int16_t player,const Camera* render,const void* renderFrustum,
    const Camera* raster,const void* rasterFrustum,int16_t kind,uint8_t reflected)
{
    Callback callback;
    ClassicViewBody(player,render,renderFrustum,raster,rasterFrustum,kind,reflected,
        reinterpret_cast<uintptr_t>(_ReturnAddress()));
}

bool ClassicStockFrustum(const Camera& source,std::array<float,99>& frustum) noexcept
{
    if (!Valid(source)) return false;
    float bounds[4]{};
    __try
    {
        reinterpret_cast<void(__fastcall*)(const Camera*,float*)>(bindings.base+
            contract::classic::classic_frustum_bounds)(&source,bounds);
        for (float value:bounds) if (!std::isfinite(value)) return false;
        reinterpret_cast<void(__fastcall*)(const Camera*,const float*,float*,uint8_t)>(bindings.base+
            contract::classic::classic_frustum_build)(&source,bounds,frustum.data(),1);
        for (float value:frustum) if (!std::isfinite(value)) return false;
        return true;
    }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}

void ClassicWindowBody(Window* window,uintptr_t caller)
{
    const auto original=reinterpret_cast<ClassicWindowFn>(classicHooks[ClassicPlayerWindow].original);
    auto* scope=classicFrameScope;
    Window source{};
    const bool eligible=Current()&&classicInstalled.load()&&CeObserveRendererMode()==0&&
        caller==bindings.base+0xbbceeb&&Read(reinterpret_cast<uintptr_t>(window),source)&&
        ValidPrimary(source)&&LiveGame();
    if (eligible)
    {
        const uint64_t now=GetTickCount64(),last=lastCameraMs.exchange(now);
        if (!last||now<last||now-last>=500) firstCameraMs.store(now);
        uint64_t zero=0; firstCameraMs.compare_exchange_strong(zero,now);
    }
    if (!scope||!eligible||scope->failed||!ClassicScopeCurrent(*scope))
    {
        if (scope) scope->Fail(ClassicFailure::ScopeChanged);
        original(window);
        return;
    }
    if (++scope->windows[scope->eye]!=1)
    {
        scope->Fail(ClassicFailure::WindowCount);
        original(window);
        return;
    }
    if (scope->eye==0)
    {
        scope->window=reinterpret_cast<uintptr_t>(window);
        FollowRoomscale(source.render,scope->pair.tracking,scope->reference,
            scope->scale,scope->positional,scope->revision);
        scope->prepared=StageClassicViewPair(source,scope->pair.tracking,scope->reference,
            scope->epoch,scope->scale,scope->positional,scope->pair)==ClassicPairResult::Ready;
        if (scope->prepared)
            PublishGameplayContext({scope->pair.tracking,scope->reference,scope->pair.source.render,
                scope->scale,scope->positional,scope->revision,scope->epoch});
        if (scope->prepared) scope->capture=cache.Begin(scope->pair,scope->key);
    }
    else if (!scope->prepared||scope->window!=reinterpret_cast<uintptr_t>(window)||
        !ClassicPairCurrent(scope->pair,source,scope->generation,
            scope->pair.tracking.spaceEpoch,scope->pair.tracking.serial,scope->epoch))
        scope->Fail(ClassicFailure::PairChanged);
    if (!scope->prepared||!scope->capture||scope->failed)
    {
        scope->Fail(ClassicFailure::PairPreparation);
        original(window);
        return;
    }
    std::array<uint8_t,sizeof(Camera)+0x18c> publishedCameraAndFrustum{};
    if (!Read(bindings.base+0x29af2c4,publishedCameraAndFrustum))
    {
        scope->Fail(ClassicFailure::CameraState);
        original(window);
        return;
    }

    // The two Camera values are plain 0x54-byte native values. Original
    // frustum construction consumes these before visibility/drawing. Restore
    // the source even if the native render raises a structured exception.
    __try
    {
        window->render=scope->pair.eyes[scope->eye].render;
        window->raster=scope->pair.eyes[scope->eye].raster;
        original(window);
    }
    __finally
    {
        window->render=source.render;
        window->raster=source.raster;
        // BBCF30 publishes a second copy for non-window consumers. Restore
        // the complete previous Camera/frustum bytes together. Rebuilding a
        // guessed stock frustum here can lose native fog/reflection state.
        std::memcpy(reinterpret_cast<void*>(bindings.base+0x29af2c4),
            publishedCameraAndFrustum.data(),publishedCameraAndFrustum.size());
    }
}
void __fastcall ClassicWindowHook(Window* window)
{
    Callback callback;
    ClassicWindowBody(window,reinterpret_cast<uintptr_t>(_ReturnAddress()));
}

void ClassicBlitBody(const halo_ce::Rectangle* rectangle,uintptr_t caller)
{
    reinterpret_cast<ClassicBlitFn>(classicHooks[ClassicFinalBlit].original)(rectangle);
    auto* scope=classicFrameScope;
    if (caller!=bindings.base+0xae0ecb||!Current()||!classicInstalled.load()||
        CeObserveRendererMode()!=0) return;
    classicOutputs.fetch_add(1,std::memory_order_relaxed);
    ClassicNativeSource source{};
    if (!ClassicSource(source))
    {
        classicSourceMiss.fetch_add(1,std::memory_order_relaxed);
        if (scope) scope->Fail(ClassicFailure::Source);
        return;
    }
    wanted.Publish({source.record.descriptor,generation.load(),source.context});
    if (!scope||!scope->prepared||!scope->capture||scope->failed||
        scope->windows[scope->eye]!=1||scope->views[scope->eye]!=1||!ClassicScopeCurrent(*scope)) return;
    halo_ce::Rectangle rect{};
    const auto& descriptor=source.record.descriptor;
    const auto& viewport=scope->pair.source.raster.viewport;
    if (scope->eye==1)
    {
        const auto& first=scope->sources[0];
        if (source.wrapper!=first.wrapper||source.resolved!=first.resolved||
            source.rtv!=first.rtv||source.resource!=first.resource||source.context!=first.context||
            source.record.revision!=first.record.revision)
        { scope->Fail(ClassicFailure::SourceChanged); return; }
    }
    scope->sources[scope->eye]=source;
    if (++scope->outputs[scope->eye]!=1||!Read(reinterpret_cast<uintptr_t>(rectangle),rect)||
        rect.top!=0||rect.left!=0||rect.bottom!=descriptor.Height||rect.right!=descriptor.Width||
        viewport.top!=0||viewport.left!=0||viewport.bottom!=rect.bottom||viewport.right!=rect.right||
        !cache.Capture(scope->key,scope->eye,reinterpret_cast<ID3D11DeviceContext*>(source.context),
            reinterpret_cast<ID3D11Resource*>(source.resource),descriptor))
        scope->Fail(ClassicFailure::OutputShape);
}
void __fastcall ClassicBlitHook(const halo_ce::Rectangle* rectangle)
{
    Callback callback;
    ClassicBlitBody(rectangle,reinterpret_cast<uintptr_t>(_ReturnAddress()));
}

void ClassicGameRenderBody(float delta,float interpolation)
{
    const auto original=reinterpret_cast<ClassicGameRenderFn>(classicHooks[ClassicGameRender].original);
    Tracking tracking{};
    uintptr_t players{},clock{};
    int16_t playerCount{};
    int32_t tick{};
    if (classicFrameScope||!Current()||!classicInstalled.load()||CeObserveRendererMode()!=0||
        !armed.load()||!TrackingNow(tracking)||!LiveGame()||
        !Read(bindings.base+0x2ea2d90,players)||!Read(players+0xb4,playerCount)||playerCount!=1||
        !Read(bindings.base+0x2e9fd68,clock)||!Read(clock+0xc,tick))
    {
        original(delta,interpolation);
        classicStock.fetch_add(1,std::memory_order_relaxed);
        return;
    }
    if (preparationBusy.test_and_set(std::memory_order_acquire))
    {
        original(delta,interpolation);
        classicStock.fetch_add(1,std::memory_order_relaxed);
        return;
    }
    ClassicFrameScope scope{};
    completedFrame.Publish({});
    scope.generation=generation.load(); scope.epoch=ceRendererEpoch.load(); scope.tick=tick; scope.clock=clock;
    scope.revision=referenceRevision.load();
    if (recenter.exchange(false)||reference.generation!=scope.generation||
        reference.spaceEpoch!=tracking.spaceEpoch)
        reference={tracking.headPosition,tracking.headOrientation,tracking.spaceEpoch,scope.generation};
    scope.reference=reference;
    if (scope.revision==referenceRevision.load())
        publishedReference.Publish({reference,scope.revision});
    scope.scale=Game_GetWorldScale(); scope.positional=Game_IsPositionalTracking();
    scope.pair.tracking=tracking;
    classicFrameScope=&scope;
    bool returned=false;
    __try
    {
        // AC47C0 is the native render-only path. Each invocation rebuilds its
        // cameras, visibility and output; the game update AB0D10 is not called.
        // Anniversary's full-frame renderer/worker completion is not replayed.
        original(delta,interpolation);
        if (!scope.failed&&scope.prepared&&scope.capture&&scope.outputs[0]==1&&
            ClassicScopeCurrent(scope))
        {
            scope.eye=1;
            original(delta,interpolation);
        }
        returned=true;
    }
    __finally
    {
        if (returned&&!scope.failed&&scope.prepared&&scope.capture&&
            scope.outputs[0]==1&&scope.outputs[1]==1&&ClassicScopeCurrent(scope)&&
            cache.Finish(scope.key))
        {
            completedFrame.Publish({scope.key,scope.revision,GetTickCount64()});
            lastOwnedMs.store(GetTickCount64());
            captured.fetch_add(1,std::memory_order_relaxed);
            built.fetch_add(1,std::memory_order_relaxed);
            classicPairs.fetch_add(1,std::memory_order_relaxed);
            classicLastFailure.store(ClassicFailure::None,std::memory_order_relaxed);
        }
        else
        {
            cache.Drop(scope.key);
            dropped.fetch_add(1,std::memory_order_relaxed);
            classicDrops.fetch_add(1,std::memory_order_relaxed);
            classicLastFailure.store(scope.failure==ClassicFailure::None?
                ClassicFailure::IncompletePair:scope.failure,std::memory_order_relaxed);
        }
        classicFrameScope=nullptr;
        preparationBusy.clear(std::memory_order_release);
    }
}
void __fastcall ClassicGameRenderHook(float delta,float interpolation)
{
    Callback callback;
    ClassicGameRenderBody(delta,interpolation);
}

bool Classic_Remove() noexcept
{
    classicInstalled.store(false,std::memory_order_release);
    for (auto& hook:classicHooks) if (hook.enabled)
    {
        const auto status=MCCVR_DisableHookForRetirement(hook.target);
        if (status!=MH_OK&&status!=MH_ERROR_DISABLED) return false;
        hook.enabled=false;
    }
    if (callbacks.load(std::memory_order_acquire)) return false;
    const void* detours[ClassicHookCount]={reinterpret_cast<void*>(&ClassicGameRenderHook),
        reinterpret_cast<void*>(&ClassicWindowHook),reinterpret_cast<void*>(&ClassicBlitHook),
        reinterpret_cast<void*>(&ClassicViewHook)};
    const void* originals[ClassicHookCount]{};
    bool any=false;
    for (size_t i=0;i<ClassicHookCount;++i)
    { originals[i]=classicHooks[i].original; any|=classicHooks[i].target!=nullptr; }
    if (any&&!WaitForNativeDetourQuiescence(detours,originals,ClassicHookCount,callbacks)) return false;
    for (auto& hook:classicHooks) if (hook.target)
    {
        const auto status=MH_RemoveHook(hook.target);
        if (status!=MH_OK&&status!=MH_ERROR_NOT_CREATED) return false;
        hook={};
    }
    ceObservedRenderer.store(-1,std::memory_order_release);
    return true;
}

bool Classic_Install() noexcept
{
    if (classicInstalled.load()) return true;
    const auto gen=generation.load();
    if (!gen||classicRejectedGeneration==gen) return false;
    const char* failure{};
    const NativeContractSet set{contract::classic::entries,contract::classic::witnesses,
        contract::classic::relatives,contract::classic::pointers};
    if (!VerifyNativeFeatureBindings(bindings.base,bindings.size,gen,set,failure))
    {
        classicRejectedGeneration=gen;
        LOG("CE Classic stock fallback: binding verification %s",failure?failure:"failed");
        return false;
    }
    const uintptr_t addresses[ClassicHookCount]={bindings.base+contract::classic::classic_game_render,
        bindings.base+contract::classic::classic_player_window,bindings.base+contract::classic::classic_final_blit,
        bindings.base+contract::classic::classic_view_render};
    void* detours[ClassicHookCount]={reinterpret_cast<void*>(&ClassicGameRenderHook),
        reinterpret_cast<void*>(&ClassicWindowHook),reinterpret_cast<void*>(&ClassicBlitHook),
        reinterpret_cast<void*>(&ClassicViewHook)};
    for (size_t i=0;i<ClassicHookCount;++i)
    {
        auto& hook=classicHooks[i];
        const auto status=MH_CreateHook(reinterpret_cast<void*>(addresses[i]),detours[i],&hook.original);
        if (status!=MH_OK)
        {
            classicRejectedGeneration=gen;
            LOG("CE Classic stock fallback: create hook %zu status %d",i,status);
            Classic_Remove();
            return false;
        }
        hook.target=reinterpret_cast<void*>(addresses[i]);
    }
    for (auto& hook:classicHooks)
    {
        const auto status=MH_EnableHook(hook.target);
        if (status!=MH_OK)
        {
            classicRejectedGeneration=gen;
            LOG("CE Classic stock fallback: enable hook status %d",status);
            Classic_Remove();
            return false;
        }
        hook.enabled=true;
    }
    classicInstalled.store(true,std::memory_order_release);
    LOG("CE Classic render hooks installed: native render-only eyes, paired native cameras and exact final kind-0 output");
    return true;
}
