// H3 behavior: an independently magnified world view follows the gun's aim,
// while the two headset views remain wide. ODST uses its own verified compact,
// derived and nested-camera layout and native rebuild/upload functions.
std::atomic<uint32_t> g_odstScopeFaultGeneration{0};
std::atomic<uint64_t> g_odstScopeRendered{0},g_odstScopeRefused{0},g_odstScopeFaults{0};

void RenderOdstScopeBody(void* view)
{
    const auto generation=g_odstRuntimeGeneration.load(std::memory_order_acquire);
    if(!view||!generation||g_odstScopeFaultGeneration.load()==generation||
        !g_odstCamera.armed.load()||g_odstCamera.teardownRequested.load()||
        !g_odstCamera.buildViewport||!g_odstCamera.buildMatrices||
        !g_odstCamera.prepareView||!g_odstCamera.originalRenderView||
        !VR_ScopeShouldRenderThisFrame())return;
    int32_t scene=-1,shot=-1;
    if(ReadOdstCinematicControl(scene,shot)!=CinematicControlState::PlayerControlled)return;
    const auto& layout=kOdstCameraProfile.layout;
    auto* bytes=static_cast<unsigned char*>(view);
    auto* camera=bytes+layout.rootCurrentCompact;
    float basis[9]{},seat[3]{},scale=1;
    const float direction[3]{g_aimFwdX.load(),g_aimFwdY.load(),g_aimFwdZ.load()};
    ScopeCameraPose pose{};float aspect=4.f/3.f;
    if(!ControllerWorldPoseEx(false,basis,seat,scale)||
        !ComputeScopeCameraPose(basis,reinterpret_cast<const float*>(camera+layout.compactPosition),direction,pose)||
        !VR_GetScopeRenderAspect(aspect))return;
    const auto tangents=ComputeScopeProjectionTangents(VR_GetScopeZoom(),aspect);
    OdstHalo3FovMatch fov{};
    if(!ComputeOdstHalo3FovMatch(atanf(tangents.horizontal),atanf(tangents.vertical),fov))return;
    constexpr size_t capacity=0xC0;
    const uintptr_t offsets[]{layout.rootCurrentCompact,layout.rootCurrentDerived,
        layout.rootSecondaryCompact,layout.rootSecondaryDerived,layout.nestedCurrentCompact,
        layout.nestedCurrentDerived,layout.nestedSecondaryCompact,layout.nestedSecondaryDerived};
    const size_t sizes[]{layout.compactSize,layout.derivedSize,layout.compactSize,layout.derivedSize,
        layout.compactSize,layout.derivedSize,layout.compactSize,layout.derivedSize};
    alignas(16) unsigned char saved[8][capacity]{};
    for(int i=0;i<8;++i){if(sizes[i]>capacity)return;std::memcpy(saved[i],bytes+offsets[i],sizes[i]);}
    if(!VR_BeginScopeRaster()){g_odstScopeRefused.fetch_add(1);return;}
    const FpInterpolationContext contexts[]{g_fpInterpolationContexts[0],g_fpInterpolationContexts[1]};
    const bool previousScope=g_scopeRenderActive.exchange(true,std::memory_order_acq_rel);
    __try
    {
        std::memcpy(camera+layout.compactPosition,pose.position,12);
        std::memcpy(camera+layout.compactForward,pose.forward,12);
        std::memcpy(camera+layout.compactUp,pose.up,12);
        *reinterpret_cast<float*>(camera+layout.verticalFov)=fov.compactVerticalInput;
        *reinterpret_cast<float*>(camera+layout.referenceFov)=fov.compactReferenceInput;
        alignas(16) unsigned char temporary[0x40]{};
        g_odstCamera.buildViewport(camera,temporary);
        g_odstCamera.buildMatrices(camera,temporary,bytes+layout.rootCurrentDerived,0.f);
        auto* projection=reinterpret_cast<float*>(bytes+layout.rootCurrentDerived+layout.projectionMatrix);
        projection[0]=fov.projectionX;projection[5]=fov.projectionY;
        for(int i=2;i<8;i+=2)
        {
            std::memcpy(bytes+offsets[i],camera,layout.compactSize);
            std::memcpy(bytes+offsets[i+1],bytes+layout.rootCurrentDerived,layout.derivedSize);
        }
        if(g_odstCamera.fpCameraUpload)
            g_odstCamera.fpCameraUpload(bytes+layout.rootSecondaryCompact,bytes+layout.rootSecondaryDerived);
        g_odstCamera.prepareView(view,0);
        g_odstCamera.originalRenderView(view);
        VR_CaptureScope();g_odstScopeRendered.fetch_add(1,std::memory_order_relaxed);
    }
    __finally
    {
        for(int i=0;i<8;++i)std::memcpy(bytes+offsets[i],saved[i],sizes[i]);
        g_fpInterpolationContexts[0]=contexts[0];g_fpInterpolationContexts[1]=contexts[1];
        g_scopeRenderActive.store(previousScope,std::memory_order_release);
        VR_EndScopeRaster();
    }
}
void RenderOdstScope(void* view)
{
    __try {RenderOdstScopeBody(view);}
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        g_odstScopeFaultGeneration.store(g_odstRuntimeGeneration.load(),std::memory_order_release);
        g_odstScopeFaults.fetch_add(1,std::memory_order_relaxed);
        // An optional third-view failure never retries the native draw or
        // changes camera-core ownership. Its next request stays native.
    }
}
void ReportOdstScope()
{
    const auto faults=g_odstScopeFaults.exchange(0);
    if(faults)LOG("ODST scope StockFallback: optional third view faulted %llu times; camera core retained",faults);
}
