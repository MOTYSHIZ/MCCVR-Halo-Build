// Exercise the production Classic scope with native-call fixtures and real
// WARP textures. This is not execution of CE's renderer or headset acceptance.
#include "../src/dll/haloce_stereo_core.cpp"
#include <wrl/client.h>
#include <vector>
#include <cstdio>

using Microsoft::WRL::ComPtr;
static GameTitle testTitle=GameTitle::HaloCE;
static uint32_t testGeneration=3;
GameTitle TitleAdapter_GetActiveTitle() { return testTitle; }
uint32_t TitleAdapter_GetGeneration(GameTitle) { return testGeneration; }
bool TitleAdapter_PublishLifecycle(GameTitle,uint32_t,const TitleRuntimeLifecycle&) { return true; }
bool TitleAdapter_PublishHeartbeat(GameTitle,uint32_t,uint64_t) { return true; }
float Game_GetWorldScale() { return 1.0f/3.048f; }
bool Game_IsPositionalTracking() { return true; }
bool Game_RoomscaleCameraAllowed(GameTitle) { return false; }
bool HaloCEHud_HasCrosshairScope() noexcept { return false; }
void Roomscale_Camera(GameTitle,bool,const float*,const float*,const float*,const float*,float*,float) noexcept {}
void Logf(const char*,...) { }
bool WaitForNativeDetourQuiescence(const void* const*,const void* const*,size_t,
    const std::atomic<uint32_t>& count) { return !count.load(); }

namespace
{
ID3D11DeviceContext* testContext{};
ID3D11Texture2D* testSource{};
D3D11_TEXTURE2D_DESC testDesc{};
Window stockWindow{};
unsigned nativeFrames{},nativeViews{},nativeBlits{};
enum class Fault { None,MissingView,ForeignCamera,ChangedCamera,DuplicateWindow,
    MissingOutput,ResizedOutput,ChangedTick,ChangedClock,ModeSwitch,Recenter,ChangedSource,
    ChangedOwner };
Fault fault{};
bool floatArgumentsIntact{true},sourceRestored{true};
RenderContext observedContexts[2]{};
bool observedContextValid[2]{};
uintptr_t alternativeClock{};
uintptr_t alternativeOwner{};
constexpr uint32_t leftColor=0xff123456,rightColor=0xffabcdef;
constexpr uint32_t intermediateColor=0xff765432;

uintptr_t __fastcall SelectSource(uintptr_t wrapper) { return wrapper; }
void Paint(uint32_t color)
{
    std::vector<uint32_t> pixels(testDesc.Width*testDesc.Height,color);
    testContext->UpdateSubresource(testSource,0,nullptr,pixels.data(),testDesc.Width*4,0);
}
bool Pixels(ID3D11Device* device,ID3D11Texture2D* texture,uint32_t expected)
{
    auto descriptor=testDesc; descriptor.Usage=D3D11_USAGE_STAGING;
    descriptor.BindFlags=0; descriptor.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging;
    if (FAILED(device->CreateTexture2D(&descriptor,nullptr,&staging))) return false;
    testContext->CopyResource(staging.Get(),texture);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(testContext->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped))) return false;
    bool result=true;
    for (UINT y=0;y<descriptor.Height;++y)
    {
        const auto* row=reinterpret_cast<const uint32_t*>(static_cast<const uint8_t*>(mapped.pData)+y*mapped.RowPitch);
        for (UINT x=0;x<descriptor.Width;++x) result&=row[x]==expected;
    }
    testContext->Unmap(staging.Get(),0); return result;
}
void __fastcall NativeView(int16_t,const Camera* render,const void*,const Camera*,const void*,int16_t,uint8_t)
{
    ++nativeViews;
    std::memcpy(reinterpret_cast<void*>(bindings.base+0x29af2c4),render,sizeof(Camera));
    std::memset(reinterpret_cast<void*>(bindings.base+0x29af318),0x5a,0x18c);
    auto* scope=classicFrameScope;
    if (scope&&scope->prepared)
    {
        observedContextValid[scope->eye]=ClassicGetRenderContext(observedContexts[scope->eye]);
        // World rendering precedes final output/postprocessing. Capturing at
        // this boundary would retain unfinished pixels rather than the eye.
        Paint(intermediateColor);
    }
}
void __fastcall NativeWindow(Window* window)
{
    auto* scope=classicFrameScope;
    const bool second=scope&&scope->eye==1;
    if (second&&fault==Fault::MissingView) return;
    std::array<uint8_t,0x18c> renderFrustum{},rasterFrustum{};
    Camera foreign=window->render;
    const Camera* selected=&window->render;
    if (second&&fault==Fault::ForeignCamera) selected=&foreign;
    if (second&&fault==Fault::ChangedCamera) window->render.position.x+=100;
    // The native window clamps render far plane to fog before consumption.
    window->render.farPlane=900;
    ClassicViewBody(window->player,selected,renderFrustum.data(),&window->raster,
        rasterFrustum.data(),1,0,bindings.base+0xbbccb2);
}
void __fastcall NativeBlit(const halo_ce::Rectangle*)
{
    ++nativeBlits;
    const auto* scope=classicFrameScope;
    if (scope&&scope->prepared) Paint(scope->eye?rightColor:leftColor);
}
void __fastcall NativeSaberFrame(uintptr_t,uint32_t) { }
void __fastcall NativeGame(float delta,float interpolation)
{
    ++nativeFrames;
    floatArgumentsIntact&=delta==0.125f&&interpolation==0.75f;
    auto* scope=classicFrameScope;
    auto* window=reinterpret_cast<Window*>(bindings.base+0x2e9fe80);
    *window=stockWindow;
    std::array<uint8_t,sizeof(Camera)+0x18c> before{};
    Read(bindings.base+0x29af2c4,before);
    ClassicWindowBody(window,bindings.base+0xbbceeb);
    sourceRestored&=std::memcmp(window,&stockWindow,sizeof(Window))==0&&
        std::memcmp(before.data(),reinterpret_cast<void*>(bindings.base+0x29af2c4),before.size())==0;
    if (scope&&scope->eye==1&&fault==Fault::DuplicateWindow)
        ClassicWindowBody(window,bindings.base+0xbbceeb);
    halo_ce::Rectangle rectangle{0,0,static_cast<int16_t>(testDesc.Height),static_cast<int16_t>(testDesc.Width)};
    if (scope&&scope->eye==1&&fault==Fault::ResizedOutput) ++rectangle.right;
    if (!(scope&&scope->eye==1&&fault==Fault::MissingOutput))
        ClassicBlitBody(&rectangle,bindings.base+0xae0ecb);
    if (scope&&scope->eye==0)
    {
        uintptr_t nativeClock{}; Read(bindings.base+0x2e9fd68,nativeClock);
        if (fault==Fault::ChangedTick) ++*reinterpret_cast<int32_t*>(nativeClock+0xc);
        if (fault==Fault::ChangedClock) std::memcpy(reinterpret_cast<void*>(bindings.base+0x2e9fd68),&alternativeClock,sizeof(alternativeClock));
        if (fault==Fault::ModeSwitch) *reinterpret_cast<int32_t*>(bindings.base+0x1b7aa84)=1;
        if (fault==Fault::Recenter) HaloCE_Recenter();
        if (fault==Fault::ChangedSource) HaloCE_RecordTextureCreated(testSource,testDesc);
        if (fault==Fault::ChangedOwner) std::memcpy(reinterpret_cast<void*>(bindings.base+0x2e3c090),&alternativeOwner,sizeof(alternativeOwner));
    }
}
}

int main()
{
    int failures=0;
    const auto check=[&](bool ok,const char* message) {
        if (!ok) { std::fprintf(stderr,"CE Classic runtime: %s\n",message); ++failures; }
    };
    ComPtr<ID3D11Device> device; ComPtr<ID3D11DeviceContext> context;
    const D3D_FEATURE_LEVEL feature=D3D_FEATURE_LEVEL_11_0;
    // A real DXGI buffer can predate every CE native hook and bypass the
    // public CreateTexture2D observation. Keep this test window hidden.
    struct HiddenWindow { HWND value{}; ~HiddenWindow() { if (value) DestroyWindow(value); } };
    HiddenWindow outputWindow{CreateWindowExW(0,L"STATIC",L"CE Classic source test",
        WS_OVERLAPPEDWINDOW,0,0,32,16,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr)};
    if (!outputWindow.value) return 1;
    DXGI_SWAP_CHAIN_DESC swapDesc{};
    swapDesc.BufferDesc.Width=32; swapDesc.BufferDesc.Height=16;
    swapDesc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    swapDesc.SampleDesc.Count=1; swapDesc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDesc.BufferCount=1; swapDesc.OutputWindow=outputWindow.value;
    swapDesc.Windowed=TRUE; swapDesc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
    ComPtr<IDXGISwapChain> swapchain;
    if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,&feature,1,
        D3D11_SDK_VERSION,&swapDesc,&swapchain,&device,nullptr,&context))) return 1;
    testContext=context.Get();
    ComPtr<ID3D11Texture2D> source;
    if (FAILED(swapchain->GetBuffer(0,__uuidof(ID3D11Texture2D),reinterpret_cast<void**>(source.GetAddressOf())))) return 1;
    source->GetDesc(&testDesc);
    testSource=source.Get();
    ComPtr<ID3D11RenderTargetView> rtv;
    if (FAILED(device->CreateRenderTargetView(source.Get(),nullptr,&rtv))) return 1;
    std::vector<uint8_t> mapped(contract::imageSize);
    bindings.base=reinterpret_cast<uintptr_t>(mapped.data()); bindings.generation=3;
    bindings.surfaceSelector=reinterpret_cast<uintptr_t>(&SelectSource);
    std::array<uint8_t,0x100> wrapper{};
    std::array<uint8_t,0x100> decoyWrapper{};
    std::array<uint8_t,0x508> owner{},otherOwner{};
    std::array<uint8_t,0xd00> backend{};
    std::array<uint8_t,0xc0> players{};
    std::array<uint8_t,0x40> nativeClock{},otherClock{};
    auto put=[](void* address,const auto& value) { std::memcpy(address,&value,sizeof(value)); };
    const uintptr_t wrapperAddress=reinterpret_cast<uintptr_t>(wrapper.data());
    const uintptr_t ownerAddress=reinterpret_cast<uintptr_t>(owner.data());
    alternativeOwner=reinterpret_cast<uintptr_t>(otherOwner.data());
    const uintptr_t backendAddress=reinterpret_cast<uintptr_t>(backend.data());
    const uintptr_t playersAddress=reinterpret_cast<uintptr_t>(players.data());
    const uintptr_t clockAddress=reinterpret_cast<uintptr_t>(nativeClock.data());
    alternativeClock=reinterpret_cast<uintptr_t>(otherClock.data());
    uintptr_t rtvAddress=reinterpret_cast<uintptr_t>(rtv.Get());
    const uintptr_t viewsAddress=reinterpret_cast<uintptr_t>(&rtvAddress);
    put(wrapper.data(),bindings.base+0x17fb608);
    put(wrapper.data()+0xe0,testSource); put(wrapper.data()+0xe8,viewsAddress);
    // AE410 initializes kind 0 from native output owner+500. Its wrapper
    // array loop starts at kind 1, leaving 2E3B910 null in the actual layout.
    put(mapped.data()+0x2e3c090,ownerAddress); put(owner.data()+0x500,wrapperAddress);
    put(otherOwner.data()+0x500,wrapperAddress); put(mapped.data()+0x1b85e78,rtvAddress);
    put(mapped.data()+0x2ea2d30,testContext); put(mapped.data()+0x2e3bde0,backendAddress);
    put(backend.data()+0xce0,testContext); put(mapped.data()+0x2ea2d90,playersAddress);
    put(players.data()+0xb4,int16_t{1}); put(mapped.data()+0x2e9fd68,clockAddress);
    nativeClock[0]=otherClock[0]=1; put(nativeClock.data()+0xc,int32_t{100}); put(otherClock.data()+0xc,int32_t{100});
    stockWindow.player=0;
    stockWindow.render.position={10,20,30}; stockWindow.render.forward={1,0,0}; stockWindow.render.up={0,0,1};
    stockWindow.render.verticalFov=1.2f; stockWindow.render.nearPlane=0.01f; stockWindow.render.farPlane=1000;
    stockWindow.render.viewport=stockWindow.render.window={0,0,16,32}; stockWindow.raster=stockWindow.render;
    installed=true; active=true; retiring=false; armed=true; generation=3;
    classicInstalled=true; trackingEnabled=true;
    classicHooks[ClassicGameRender].original=reinterpret_cast<void*>(&NativeGame);
    classicHooks[ClassicPlayerWindow].original=reinterpret_cast<void*>(&NativeWindow);
    classicHooks[ClassicFinalBlit].original=reinterpret_cast<void*>(&NativeBlit);
    classicHooks[ClassicMainView].original=reinterpret_cast<void*>(&NativeView);
    hooks[Frame].original=reinterpret_cast<void*>(&NativeSaberFrame);
    ClassicNativeSource selectedSource{};
    check(!ClassicSource(selectedSource)&&classicSourceFailure.load()==ClassicSourceFailure::Descriptor,
        "preexisting DXGI buffer has no fabricated creation or import metadata");
    HaloCE_RecordPresentationTexture(source.Get(),testDesc);
    check(ClassicSource(selectedSource)&&selectedSource.owner==ownerAddress&&
        selectedSource.wrapper==wrapperAddress&&selectedSource.resource==reinterpret_cast<uintptr_t>(testSource),
        "native kind-0 output resolves while wrapper-array element zero is null");
    const auto observedRevision=selectedSource.record.revision;
    HaloCE_RecordPresentationTexture(source.Get(),testDesc);
    check(ClassicSource(selectedSource)&&selectedSource.record.revision==observedRevision,
        "repeated strong GetBuffer observation does not churn the resource revision");
    HaloCE_ForgetPresentationTexture();
    check(!ClassicSource(selectedSource)&&classicSourceFailure.load()==ClassicSourceFailure::Descriptor,
        "resize revocation removes metadata even when COM could reuse the address");
    HaloCE_RecordPresentationTexture(source.Get(),testDesc);
    check(ClassicSource(selectedSource)&&selectedSource.record.revision!=observedRevision,
        "a newly owned post-resize observation receives fresh metadata lifetime");
    put(mapped.data()+0x2e3b910,reinterpret_cast<uintptr_t>(decoyWrapper.data()));
    check(ClassicSource(selectedSource)&&selectedSource.wrapper==wrapperAddress,
        "an unrelated kind-0 array decoy cannot select the completed output");
    put(owner.data()+0x500,uintptr_t{});
    put(mapped.data()+0x2e3b910,wrapperAddress);
    check(!ClassicSource(selectedSource)&&classicSourceFailure.load()==ClassicSourceFailure::Wrapper,
        "missing native output owner never falls back to the wrapper array and records its exact failure");
    put(owner.data()+0x500,wrapperAddress); put(mapped.data()+0x2e3b910,uintptr_t{});
    put(mapped.data()+0x1b85e78,uintptr_t{1});
    check(!ClassicSource(selectedSource)&&classicSourceFailure.load()==ClassicSourceFailure::RtvMismatch,
        "selected output must agree with the native kind-0 RTV cache and records an identity failure");
    put(mapped.data()+0x1b85e78,rtvAddress);
    Tracking tracking{}; tracking.generation=3; tracking.spaceEpoch=7;
    tracking.headPosition={0.1f,1.6f,0.2f};
    tracking.eyes[0].offset={-0.032f,0,0}; tracking.eyes[1].offset={0.032f,0,0};
    for (auto& eye:tracking.eyes) { eye.fov[0]=-0.9f; eye.fov[1]=0.8f; eye.fov[2]=0.85f; eye.fov[3]=-0.95f; }
    uint64_t serial=0;
    auto run=[&](Fault selected) {
        fault=selected; ++serial; tracking.serial=serial;
        *reinterpret_cast<int32_t*>(mapped.data()+0x1b7aa84)=0;
        put(mapped.data()+0x2e9fd68,clockAddress); put(nativeClock.data()+0xc,int32_t{100});
        put(mapped.data()+0x2e3c090,ownerAddress);
        CeObserveRendererMode(); recenter=true;
        HaloCE_PublishTracking(tracking,true);
        ClassicGameRenderBody(0.125f,0.75f);
        check(!classicFrameScope&&!preparationBusy.test(),"scope and busy lease retire after every frame");
    };
    EyeCache::Completed pair{};
    HaloCE_ForgetPresentationTexture();
    run(Fault::None);
    Wanted unavailable{};
    check(!HaloCE_AcquirePair(context.Get(),serial,7,pair)&&classicSourceMiss.load()==1&&
        classicSourceFailure.load()==ClassicSourceFailure::Descriptor&&
        (!wanted.Read(unavailable)||!unavailable.generation),
        "a late CE admission cannot publish or submit a preexisting DXGI buffer with unknown metadata");
    // Production SubmitPreparedFrame observes its already-owned GetBuffer
    // reference here; no texture creation/import callback is manufactured.
    HaloCE_RecordPresentationTexture(source.Get(),testDesc);
    run(Fault::None);
    check(!HaloCE_AcquirePair(context.Get(),serial,7,pair)&&classicLastFailure.load()==ClassicFailure::PairPreparation,
        "first cold Classic frame discovers the source without submitting an unprepared eye pair");
    Wanted discovered{};
    check(wanted.Read(discovered)&&discovered.generation==3&&discovered.descriptor.Width==testDesc.Width&&
        discovered.descriptor.Height==testDesc.Height&&discovered.context==reinterpret_cast<uintptr_t>(testContext)&&
        classicSourceMiss.load()==1,"native output discovery publishes the complete cold cache request after strong presentation observation");
    HaloCE_PresentResources(device.Get(),context.Get());
    nativeFrames=nativeViews=nativeBlits=0;
    sourceRestored=true; // the initial unowned native pass may publish stock state
    run(Fault::None);
    check(nativeFrames==2&&nativeViews==2&&nativeBlits==2,"two independent native render passes each consume a camera and output");
    check(floatArgumentsIntact&&sourceRestored,"float ABI and exact native camera/frustum restoration survive both passes");
    check(observedContextValid[0]&&observedContextValid[1]&&
        std::memcmp(&observedContexts[0],&observedContexts[1],sizeof(RenderContext))==0&&
        std::memcmp(&observedContexts[0].camera,&stockWindow.render,sizeof(Camera))==0,
        "first-person and HUD receive one stock center/tracking/reference context in both eyes");
    FrameBody(0,0);
    check(HaloCE_AcquirePair(context.Get(),serial,7,pair),
        "a later unclaimed Anniversary worker callback preserves the completed Classic pair");
    if (pair.borrowId)
    {
        Paint(0xff000000);
        check(Pixels(device.Get(),pair.eyes[0],leftColor)&&Pixels(device.Get(),pair.eyes[1],rightColor),
            "owned eyes contain final-blit pixels and remain distinct after the reused source is overwritten");
        HaloCE_ReleasePair(pair.borrowId); pair={};
    }
    *reinterpret_cast<int32_t*>(mapped.data()+0x1b7aa84)=1;
    check(!HaloCE_AcquirePair(context.Get(),serial,7,pair),
        "graphics mode change before submission immediately revokes the Classic pair");
    for (Fault selected:{Fault::MissingView,Fault::ForeignCamera,Fault::ChangedCamera,
        Fault::DuplicateWindow,Fault::MissingOutput,Fault::ResizedOutput,Fault::ChangedTick,
        Fault::ChangedClock,Fault::ModeSwitch,Fault::Recenter,Fault::ChangedSource,Fault::ChangedOwner})
    {
        run(selected);
        check(!HaloCE_AcquirePair(context.Get(),serial,7,pair),"invalid native consumer/output/lifetime drops the pair");
        check(armed.load()&&installed.load()&&classicInstalled.load(),"a rejected frame preserves VR core and hook ownership");
        run(Fault::None);
        check(HaloCE_AcquirePair(context.Get(),serial,7,pair),"next good native frame recovers without reinstall");
        if (pair.borrowId) { HaloCE_ReleasePair(pair.borrowId); pair={}; }
    }
    RenderContext gameplay{};
    check(HaloCE_GetGameplayContext(gameplay)&&
        std::memcmp(&gameplay.camera,&stockWindow.render,sizeof(Camera))==0,
        "gameplay-thread context receives the exact saved stock center after the render scope exits");
    const auto savedGameplay=gameplay;
    std::memset(mapped.data()+0x29af2c4,0x7f,sizeof(Camera));
    check(HaloCE_GetGameplayContext(gameplay)&&
        std::memcmp(&gameplay.camera,&savedGameplay.camera,sizeof(Camera))==0,
        "temporary global eye camera changes cannot contaminate gameplay aiming");
    tracking.serial=++serial;
    tracking.controllers.primaryAim={true,{0.3f,1.2f,-0.4f},{}};
    tracking.controllers.support={true,{-0.2f,1.1f,-0.6f},{}};
    HaloCE_PublishTracking(tracking,true);
    check(HaloCE_GetGameplayContext(gameplay)&&gameplay.tracking.serial==serial&&
        gameplay.tracking.controllers.primaryAim.position.x==0.3f&&
        gameplay.tracking.controllers.support.position.z==-0.6f&&
        std::memcmp(&gameplay.camera,&savedGameplay.camera,sizeof(Camera))==0,
        "new tracking serial supplies current controllers while retaining the native stock center");
    const auto recoverGameplay=[&]() {
        run(Fault::None);
        check(HaloCE_GetGameplayContext(gameplay),"fresh Classic frame recovers gameplay context");
    };
    HaloCE_Recenter();
    check(!HaloCE_GetGameplayContext(gameplay),"pending recenter revokes gameplay aiming");
    recenter=false;
    check(!HaloCE_GetGameplayContext(gameplay),"consumed recenter cannot revive the older reference revision");
    recoverGameplay();
    ceRendererEpoch.fetch_add(1);
    check(!HaloCE_GetGameplayContext(gameplay),"renderer epoch change revokes saved gameplay center");
    recoverGameplay();
    testGeneration=4;
    check(!HaloCE_GetGameplayContext(gameplay),"title generation change revokes gameplay center");
    testGeneration=3; recoverGameplay();
    tracking.spaceEpoch=8; tracking.serial=++serial;
    HaloCE_PublishTracking(tracking,true);
    check(!HaloCE_GetGameplayContext(gameplay),"new tracking space cannot reuse the old native reference");
    tracking.spaceEpoch=7; recoverGameplay();
    GameplayCameraSample stale{}; gameplayCamera.Read(stale);
    stale.capturedAtMs=GetTickCount64()-300; gameplayCamera.Publish(stale);
    check(!HaloCE_GetGameplayContext(gameplay),"aged native stock center rejects current controller aiming");
    recoverGameplay();
    gameplayCamera.Read(stale); stale.capturedAtMs=GetTickCount64()+300; gameplayCamera.Publish(stale);
    check(!HaloCE_GetGameplayContext(gameplay),"future captured timestamp rejects gameplay context");
    recoverGameplay();
    HaloCE_ForgetPresentationTexture();
    rtv.Reset(); source.Reset(); testSource=nullptr;
    check(SUCCEEDED(swapchain->ResizeBuffers(1,48,24,DXGI_FORMAT_R8G8B8A8_UNORM,0)),
        "presentation metadata retains no COM references that block real DXGI ResizeBuffers");
    if (SUCCEEDED(swapchain->GetBuffer(0,__uuidof(ID3D11Texture2D),reinterpret_cast<void**>(source.GetAddressOf()))))
    {
        source->GetDesc(&testDesc); testSource=source.Get();
        check(SUCCEEDED(device->CreateRenderTargetView(source.Get(),nullptr,&rtv)),"resized native output view recreates");
        rtvAddress=reinterpret_cast<uintptr_t>(rtv.Get());
        put(wrapper.data()+0xe0,testSource); put(mapped.data()+0x1b85e78,rtvAddress);
        check(!ClassicSource(selectedSource)&&classicSourceFailure.load()==ClassicSourceFailure::Descriptor,
            "real resized buffer is rejected until a new strong presentation observation");
        HaloCE_RecordPresentationTexture(source.Get(),testDesc);
        stockWindow.render.viewport=stockWindow.render.window={0,0,
            static_cast<int16_t>(testDesc.Height),static_cast<int16_t>(testDesc.Width)};
        stockWindow.raster.viewport=stockWindow.raster.window=stockWindow.render.viewport;
        run(Fault::None);
        check(!HaloCE_AcquirePair(context.Get(),serial,7,pair)&&
            classicLastFailure.load()==ClassicFailure::PairPreparation,
            "a differently sized DXGI buffer cannot reuse the previous eye cache");
        check(wanted.Read(discovered)&&discovered.descriptor.Width==48&&
            discovered.descriptor.Height==24,
            "the resized native output publishes replacement cache dimensions");
        HaloCE_PresentResources(device.Get(),context.Get());
        run(Fault::None);
        check(HaloCE_AcquirePair(context.Get(),serial,7,pair),"Classic capture recovers after real DXGI resize and observation");
        if (pair.borrowId)
        {
            Paint(0xff000000);
            check(pair.descriptor.Width==48&&pair.descriptor.Height==24&&
                Pixels(device.Get(),pair.eyes[0],leftColor)&&Pixels(device.Get(),pair.eyes[1],rightColor),
                "replacement caches retain both complete resized eye images independently");
            HaloCE_ReleasePair(pair.borrowId); pair={};
        }
    }
    else check(false,"resized DXGI buffer is available");
    classicInstalled=false;
    HaloCE_ForgetPresentationTexture();
    cache.Reset();
    return failures?1:0;
}
