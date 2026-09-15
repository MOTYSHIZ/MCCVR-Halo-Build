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
    MissingOutput,ResizedOutput,ChangedTick,ChangedClock,ModeSwitch,Recenter,ChangedSource };
Fault fault{};
bool floatArgumentsIntact{true},sourceRestored{true};
RenderContext observedContexts[2]{};
bool observedContextValid[2]{};
uintptr_t alternativeClock{};
constexpr uint32_t leftColor=0xff123456,rightColor=0xffabcdef;

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
        Paint(scope->eye?rightColor:leftColor);
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
void __fastcall NativeBlit(const halo_ce::Rectangle*) { ++nativeBlits; }
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
    if (FAILED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,&feature,1,
        D3D11_SDK_VERSION,&device,nullptr,&context))) return 1;
    testContext=context.Get();
    testDesc.Width=32; testDesc.Height=16; testDesc.MipLevels=1; testDesc.ArraySize=1;
    testDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM; testDesc.SampleDesc.Count=1;
    testDesc.Usage=D3D11_USAGE_DEFAULT; testDesc.BindFlags=D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> source;
    if (FAILED(device->CreateTexture2D(&testDesc,nullptr,&source))) return 1;
    testSource=source.Get();
    ComPtr<ID3D11RenderTargetView> rtv;
    if (FAILED(device->CreateRenderTargetView(source.Get(),nullptr,&rtv))) return 1;
    std::vector<uint8_t> mapped(contract::imageSize);
    bindings.base=reinterpret_cast<uintptr_t>(mapped.data()); bindings.generation=3;
    bindings.surfaceSelector=reinterpret_cast<uintptr_t>(&SelectSource);
    std::array<uint8_t,0x100> wrapper{};
    std::array<uint8_t,0xd00> backend{};
    std::array<uint8_t,0xc0> players{};
    std::array<uint8_t,0x40> nativeClock{},otherClock{};
    auto put=[](void* address,const auto& value) { std::memcpy(address,&value,sizeof(value)); };
    const uintptr_t wrapperAddress=reinterpret_cast<uintptr_t>(wrapper.data());
    const uintptr_t backendAddress=reinterpret_cast<uintptr_t>(backend.data());
    const uintptr_t playersAddress=reinterpret_cast<uintptr_t>(players.data());
    const uintptr_t clockAddress=reinterpret_cast<uintptr_t>(nativeClock.data());
    alternativeClock=reinterpret_cast<uintptr_t>(otherClock.data());
    const uintptr_t rtvAddress=reinterpret_cast<uintptr_t>(rtv.Get());
    const uintptr_t viewsAddress=reinterpret_cast<uintptr_t>(&rtvAddress);
    put(wrapper.data(),bindings.base+0x17fb608);
    put(wrapper.data()+0xe0,testSource); put(wrapper.data()+0xe8,viewsAddress);
    put(mapped.data()+0x2e3b910,wrapperAddress); put(mapped.data()+0x1b85e78,rtvAddress);
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
    HaloCE_RecordTextureCreated(testSource,testDesc);
    check(cache.Prepare(device.Get(),context.Get(),testDesc,3,1),"cold eye storage prepares");
    Tracking tracking{}; tracking.generation=3; tracking.spaceEpoch=7;
    tracking.headPosition={0.1f,1.6f,0.2f};
    tracking.eyes[0].offset={-0.032f,0,0}; tracking.eyes[1].offset={0.032f,0,0};
    for (auto& eye:tracking.eyes) { eye.fov[0]=-0.9f; eye.fov[1]=0.8f; eye.fov[2]=0.85f; eye.fov[3]=-0.95f; }
    uint64_t serial=0;
    auto run=[&](Fault selected) {
        fault=selected; ++serial; tracking.serial=serial;
        *reinterpret_cast<int32_t*>(mapped.data()+0x1b7aa84)=0;
        put(mapped.data()+0x2e9fd68,clockAddress); put(nativeClock.data()+0xc,int32_t{100});
        CeObserveRendererMode(); recenter=true;
        HaloCE_PublishTracking(tracking,true);
        ClassicGameRenderBody(0.125f,0.75f);
        check(!classicFrameScope&&!preparationBusy.test(),"scope and busy lease retire after every frame");
    };
    EyeCache::Completed pair{};
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
            "owned per-eye pixels remain distinct after the reused source is overwritten");
        HaloCE_ReleasePair(pair.borrowId); pair={};
    }
    *reinterpret_cast<int32_t*>(mapped.data()+0x1b7aa84)=1;
    check(!HaloCE_AcquirePair(context.Get(),serial,7,pair),
        "graphics mode change before submission immediately revokes the Classic pair");
    for (Fault selected:{Fault::MissingView,Fault::ForeignCamera,Fault::ChangedCamera,
        Fault::DuplicateWindow,Fault::MissingOutput,Fault::ResizedOutput,Fault::ChangedTick,
        Fault::ChangedClock,Fault::ModeSwitch,Fault::Recenter,Fault::ChangedSource})
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
    classicInstalled=false;
    cache.Reset();
    return failures?1:0;
}
