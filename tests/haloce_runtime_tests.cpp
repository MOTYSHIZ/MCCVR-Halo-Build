// Runs the production CE adapter against native-call fixtures and real WARP
// textures. It never maps executable game code, installs hooks or opens MCC.
#include "../src/dll/haloce_stereo_core.cpp"
#include <wrl/client.h>
#include <vector>
#include <cstdio>

using Microsoft::WRL::ComPtr;
void ConfigureCeHudLayoutRuntimeFixture(uint32_t generation,bool enabled,bool installed=true);
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
bool WaitForNativeDetourQuiescence(const void* const*,const void* const*,size_t count,
    const std::atomic<uint32_t>& value) { return count<=8&&!value.load(); }

namespace
{
int32_t sceneRequestedRefresh{};
const float* scenePrimary{};
const float* sceneSecondary{};
void __fastcall NativeSceneCamera(uintptr_t scene,int32_t refresh,
    const float* primary,const float* secondary)
{
    sceneRequestedRefresh=refresh; scenePrimary=primary; sceneSecondary=secondary;
    // Model just the documented native previous/current camera-count flag.
    // Actual native static-cache reconstruction is executed by the pinned
    // test_ce_scene_refresh_native.py verifier.
    auto* flags=reinterpret_cast<uint32_t*>(scene+0x110);
    *flags=(*flags&~0x1000u)|(primary&&secondary?0x1000u:0u);
}
ID3D11DeviceContext* testContext{};
ID3D11Texture2D* testSource{};
ID3D11Texture2D* testDestination{};
D3D11_TEXTURE2D_DESC testDesc{};
bool omitRight{};
bool invalidBox{};
bool omitDepth{},omitShading{},foreignUploadCamera{},changedUploadCamera{},changedUploadPlayer{};
bool aliasDepthResource{},aliasDepthView{},wrongBoundDepth{},changeDepthAtScene{},omitDepthDraw{};
bool recreateDepthAtScene{},auxiliaryDepthOverwrite{};
bool recyclePreparedSource{};
bool inspectPrimaryDraw{},primaryDrawCorrect{true};
unsigned primaryDrawStages{},primaryDrawOutputs{};
bool nestedMaterialProbe{},nestedMaterialFault{},nestedMaterialRejected{true};
int depthEye{};
uintptr_t depthRoot{},depthBackend{};
std::array<std::array<uint8_t,0x118>,2> depthSurfaces{};
ID3D11DepthStencilView* depthViews[2]{};
ID3D11Resource* depthTextures[2]{};
uintptr_t hudRoot{};
uintptr_t __fastcall NativeDepthSelect(uintptr_t root)
{ return root==hudRoot?root:root==depthRoot?reinterpret_cast<uintptr_t>(depthSurfaces[depthEye].data()):0; }
void SelectDepth(int eye)
{
    depthEye=eye;
    const auto resource=reinterpret_cast<uintptr_t>(depthTextures[aliasDepthResource?0:eye]);
    const auto view=reinterpret_cast<uintptr_t>(depthViews[aliasDepthView?0:eye]);
    std::memcpy(depthSurfaces[eye].data()+0xe0,&resource,8);
    std::memcpy(depthSurfaces[eye].data()+0x108,&view,8);
}
void __fastcall NativeDepthDraw(uintptr_t,uintptr_t,uintptr_t,int32_t eye)
{
    if (inspectPrimaryDraw)
    {
        Tracking owner{};
        primaryDrawCorrect&=HaloCE_GetAnniversaryPrimaryEyeTracking(owner)==(eye>=0&&eye<2);
    }
    if (eye==2) eye=0;
    SelectDepth(eye);
    const auto view=reinterpret_cast<uintptr_t>(depthViews[wrongBoundDepth?1-eye:aliasDepthView?0:eye]);
    std::memcpy(reinterpret_cast<void*>(depthBackend+0xd20),&view,8);
    testContext->OMSetRenderTargets(0,nullptr,reinterpret_cast<ID3D11DepthStencilView*>(view));
    testContext->ClearDepthStencilView(reinterpret_cast<ID3D11DepthStencilView*>(view),
        D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL,eye?.8f:.2f,static_cast<UINT8>(eye+1));
}
unsigned nativeCopies{};
uintptr_t rendererAddress{};
constexpr uint32_t leftColor=0xff123456,rightColor=0xffabcdef;
void Paint(ID3D11Texture2D* texture,uint32_t color);
constexpr uint32_t hudColor=0xff34ab78;
unsigned hudCallbacks{};
bool hudRasterCorrect{true};
enum class HudFault { None,CallbackException,PreambleException,UnownedStack };
HudFault hudFault{};
void ObserveRaster(const D3D11_VIEWPORT& viewport,const D3D11_RECT& scissor)
{
    D3D11_VIEWPORT transformed{}; D3D11_RECT clipped{};
    const bool mappedView=HaloCEHudLayout_PrepareViewports(testContext,1,&viewport,&transformed);
    const bool mappedScissor=HaloCEHudLayout_PrepareScissors(testContext,1,&scissor,&clipped);
    testContext->RSSetViewports(1,mappedView?&transformed:&viewport);
    testContext->RSSetScissorRects(1,mappedScissor?&clipped:&scissor);
}
void __fastcall NativeHudPush()
{
    auto* depth=reinterpret_cast<int32_t*>(depthBackend+0x10);
    uintptr_t storage{};Read(depthBackend+8,storage);
    std::memcpy(reinterpret_cast<void*>(storage+0x48*(*depth)++),reinterpret_cast<void*>(depthBackend+0x18),0x48);
}
uint8_t __fastcall NativeHudPop()
{
    auto* depth=reinterpret_cast<int32_t*>(depthBackend+0x10);
    uintptr_t storage{};Read(depthBackend+8,storage);
    if (*depth<=0) return 0;
    std::memcpy(reinterpret_cast<void*>(depthBackend+0x18),reinterpret_cast<void*>(storage+0x48*--(*depth)),0x48);
    return 1;
}
void __fastcall NativeHudCallback()
{
    ++hudCallbacks;
    D3D11_VIEWPORT view{};D3D11_RECT rect{};UINT views=1,rects=1;
    testContext->RSGetViewports(&views,&view);testContext->RSGetScissorRects(&rects,&rect);
    hudRasterCorrect&=views==1&&rects==1&&view.Width==testDesc.Width&&view.Height==testDesc.Height&&
        rect.right==LONG(testDesc.Width)&&rect.bottom==LONG(testDesc.Height);
    if (hudFault==HudFault::UnownedStack)
        ++*reinterpret_cast<int32_t*>(depthBackend+0x10);
    if (hudFault==HudFault::CallbackException||hudFault==HudFault::UnownedStack)
        RaiseException(0xe042ce01,0,0,nullptr);
    // Native callback/D3D drawing are explicit fixtures. The actual adapter
    // admission, full/half raster mapping, copy and restoration are exercised.
    Paint(testSource,hudColor);
    ObserveRaster({0,0,float(testDesc.Width),float(testDesc.Height*2),0,1},
        {0,0,LONG(testDesc.Width),LONG(testDesc.Height*2)});
}
void __fastcall NativeHudPreamble()
{
    NativeHudPush();NativeHudPop();
    if (hudFault==HudFault::PreambleException) RaiseException(0xe042ce02,0,0,nullptr);
    AnniversaryHudCallbackBody();
}
bool InstallFixtureJump(uintptr_t at,uintptr_t destination)
{
    DWORD previous{};
    if (!VirtualProtect(reinterpret_cast<void*>(at),12,PAGE_EXECUTE_READWRITE,&previous)) return false;
    // mov rax, imm64; jmp rax. This is private fixture memory, never a game module.
    const uint8_t entry[]{0x48,0xb8}; const uint8_t tail[]{0xff,0xe0};
    std::memcpy(reinterpret_cast<void*>(at),entry,2);
    std::memcpy(reinterpret_cast<void*>(at+2),&destination,8);
    std::memcpy(reinterpret_cast<void*>(at+10),tail,2);
    return FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(at),12)!=0;
}
using MixedAppendFn=uintptr_t(__fastcall*)(uintptr_t,const SaberCamera*,uint32_t,int32_t,
    float,float,uint32_t,float,uint16_t,uint16_t,uint64_t,uint8_t,float,float,uint8_t,uint8_t);
bool appendArgumentsIntact{};
uintptr_t __fastcall MixedAppend(uintptr_t list,const SaberCamera* source,uint32_t flags,int32_t index,
    float a5,float a6,uint32_t a7,float a8,uint16_t a9,uint16_t a10,
    uint64_t a11,uint8_t a12,float a13,float a14,uint8_t a15,uint8_t a16)
{
    appendArgumentsIntact=list==0x1122334455667788ull&&source==reinterpret_cast<const SaberCamera*>(0x12340)&&
        flags==0x110b&&index==-1&&a5==1.25f&&a6==-2.5f&&a7==0xdeadbeef&&a8==.125f&&
        a9==0x1234&&a10==0xabcd&&a11==0xfedcba9876543210ull&&a12==0xff&&
        a13==-12.5f&&a14==14.75f&&a15==2&&a16==3;
    return 0xabcdfedc12345678ull;
}
void Paint(ID3D11Texture2D* texture,uint32_t color)
{
    std::vector<uint32_t> pixels(testDesc.Width*testDesc.Height,color);
    testContext->UpdateSubresource(texture,0,nullptr,pixels.data(),testDesc.Width*4,0);
}
void STDMETHODCALLTYPE NativeCopy(ID3D11DeviceContext* context,ID3D11Resource* destination,
    UINT sub,UINT x,UINT y,UINT z,ID3D11Resource* source,UINT sourceSub,const D3D11_BOX* box)
{
    ++nativeCopies;
    context->CopySubresourceRegion(destination,sub,x,y,z,source,sourceSub,box);
}
uintptr_t __fastcall NativeTransfer(uintptr_t,SurfaceTransfer* request)
{
    D3D11_BOX box{0,0,0,testDesc.Width,testDesc.Height,1};
    if (invalidBox) ++box.right;
    CopyBody(testContext,testDestination,0,0,request->destinationY,0,testSource,0,&box,bindings.base+0x204da0);
    return 0x123401;
}
void __fastcall NativeOutput(int eye)
{
    if (inspectPrimaryDraw)
    {
        Tracking owner{};
        primaryDrawCorrect&=!HaloCE_GetAnniversaryPrimaryEyeTracking(owner);
        ++primaryDrawOutputs;
    }
    SurfaceTransfer request{0x111,0x222,0,0,0,0,0,static_cast<int>(eye*testDesc.Height),0,0,
        static_cast<int>(testDesc.Width),static_cast<int>(testDesc.Height)};
    TransferBody(0,&request,bindings.base+0x45e376);
}
void __fastcall NativeCameraUpload(uintptr_t,uintptr_t,const SaberCamera* camera)
{
    if (inspectPrimaryDraw)
    {
        Tracking owner{};
        primaryDrawCorrect&=!HaloCE_GetAnniversaryPrimaryEyeTracking(owner);
    }
    const uintptr_t selected=reinterpret_cast<uintptr_t>(camera);
    std::memcpy(reinterpret_cast<void*>(rendererAddress+0xbe98),&selected,sizeof(selected));
}
void ConsumeCamera(int eye,uintptr_t caller)
{
    SelectDepth(changeDepthAtScene&&caller==0x456a86?1-eye:eye);
    auto* camera=reinterpret_cast<SaberCamera*>(rendererAddress+0xf0+eye*sizeof(SaberView));
    const SaberCamera saved=*camera;
    SaberCamera foreign=*camera;
    if (changedUploadCamera&&eye==1) camera->pose.matrix[12]+=100;
    if (changedUploadPlayer&&eye==1)
    {
        const int32_t otherPlayer=1;
        std::memcpy(reinterpret_cast<uint8_t*>(camera)+0x220,&otherPlayer,4);
    }
    CameraUploadBody(0,0,foreignUploadCamera?&foreign:camera,bindings.base+caller);
    if (inspectPrimaryDraw)
    {
        Tracking owner{};
        primaryDrawCorrect&=HaloCE_GetAnniversaryPrimaryEyeTracking(owner)&&
            owner.serial==frameScope->prepared.receipt.tracking.serial;
        primaryDrawStages|=caller==0x4562bf?1u:caller==0x456a86?2u:4u;
    }
    *camera=saved;
}
Prepared MakePrepared(uint64_t serial,uintptr_t list,PreparationOrigin origin);
void __fastcall NativeFrame(uintptr_t,uint32_t)
{
    if (nestedMaterialProbe)
    {
        Tracking owner{};
        nestedMaterialRejected&=!HaloCE_GetAnniversaryPrimaryEyeTracking(owner);
        if (nestedMaterialFault) RaiseException(0xe042ce03,0,0,nullptr);
        return;
    }
    if (recyclePreparedSource&&frameScope)
    {
        // The native copied-list worker can start preparing the next frame
        // after this render frame has frozen its already-copied receipt.
        // Recycle only its private source, leaving active renderer bytes alone.
        const auto& receipt=frameScope->prepared.receipt;
        MakePrepared(receipt.tracking.serial+1,receipt.ticket.sourceList,receipt.ticket.origin);
    }
    if (!omitDepth) for (int eye=0;eye<2;++eye)
    {
        ConsumeCamera(eye,0x4562bf);
        if (!omitDepthDraw) DepthMeshBody(0,0,0,eye,bindings.base+0x456329);
    }
    if (recreateDepthAtScene)
    {
        D3D11_TEXTURE2D_DESC d{};static_cast<ID3D11Texture2D*>(depthTextures[0])->GetDesc(&d);
        HaloCE_RecordTextureCreated(static_cast<ID3D11Texture2D*>(depthTextures[0]),d);
    }
    if (auxiliaryDepthOverwrite) DepthMeshBody(0,0,0,2,bindings.base+0x456329);
    for (int eye=0;eye<(omitRight?1:2);++eye)
    {
        ConsumeCamera(eye,0x456a86);
        if (!omitShading) ConsumeCamera(eye,0x457c07);
        Paint(testSource,eye?rightColor:leftColor); OutputBody(eye);
    }
}
bool InvokeNestedMaterialFault()
{
    __try { FrameBody(0,0); }
    __except(EXCEPTION_EXECUTE_HANDLER) { return true; }
    return false;
}
uintptr_t __fastcall NativeReset(uintptr_t list)
{ *reinterpret_cast<SaberViewPair*>(list)={}; return 0xfedcba9876543210ull; }
uint8_t observedBuilderSecondary{};
bool observedBuilderTracking{};
uintptr_t __fastcall NativeBuilder(uintptr_t,SaberViewPair* list,uint8_t secondary,float*)
{
    observedBuilderSecondary=secondary;
    observedBuilderTracking=buildScope!=nullptr;
    *list={};
    return 0xceba1234;
}
void __fastcall NativePrepare(uintptr_t job)
{ std::memcpy(reinterpret_cast<void*>(rendererAddress+0xb0),reinterpret_cast<void*>(job+0x70),sizeof(SaberViewPair)); }
bool Pixels(ID3D11Device* device,ID3D11Texture2D* texture,uint32_t expected)
{
    auto d=testDesc; d.Usage=D3D11_USAGE_STAGING; d.BindFlags=0; d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging;
    if (FAILED(device->CreateTexture2D(&d,nullptr,&staging))) return false;
    testContext->CopyResource(staging.Get(),texture);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(testContext->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped))) return false;
    bool good=true;
    for (UINT y=0;y<d.Height;++y)
    {
        const auto* row=reinterpret_cast<const uint32_t*>(static_cast<const uint8_t*>(mapped.pData)+y*mapped.RowPitch);
        for (UINT x=0;x<d.Width;++x) good&=row[x]==expected;
    }
    testContext->Unmap(staging.Get(),0); return good;
}
Prepared MakePrepared(uint64_t serial,uintptr_t list,PreparationOrigin origin)
{
    auto& native=*reinterpret_cast<SaberViewPair*>(list);
    native={}; native.flags=1; native.count=2;
    Tracking tracking{}; tracking.serial=serial; tracking.generation=3; tracking.spaceEpoch=7;
    tracking.headPosition={.1f,1.6f,.2f};
    tracking.eyes[0].offset={-.032f,0,0}; tracking.eyes[1].offset={.032f,0,0};
    Camera stockCamera{};
    stockCamera.position={10,20,30}; stockCamera.forward={1,0,0}; stockCamera.up={0,0,1};
    stockCamera.verticalFov=1.2f; stockCamera.nearPlane=.01f; stockCamera.farPlane=1000;
    stockCamera.viewport=stockCamera.window={0,0,32,32};
    StagedViewPair pair{};
    for (int eye=0;eye<2;++eye)
    {
        native.views[eye].flags=eye?0x20b:0x10b; native.views[eye].viewIndex=eye;
        auto& camera=native.views[eye].camera;
        // Reproduce the test log's full-desktop camera / half-height eye source.
        camera.viewportWidth=static_cast<float>(testDesc.Width); camera.viewportHeight=static_cast<float>(2*testDesc.Height);
        BuildSaberPose(stockCamera,{},0,camera.pose);
        camera.verticalFovDegrees=70; camera.horizontalFovDegrees=100;
        camera.nearPlane=.03f; camera.farPlane=3000;
        tracking.eyes[eye].fov[0]=-.9f; tracking.eyes[eye].fov[1]=.8f;
        tracking.eyes[eye].fov[2]=.85f; tracking.eyes[eye].fov[3]=-.95f;
    }
    SaberViewPair raster{};
    const Reference originReference{tracking.headPosition,tracking.headOrientation,7,3};
    const bool staged=SelectNativeEyeRaster(native,testDesc.Width,testDesc.Height,raster)&&
        StageNativeViewPair(raster,tracking,originReference,Game_GetWorldScale(),true,
            [](SaberCamera&) { return true; },pair)==PairStageResult::Staged;
    if (staged) for (int eye=0;eye<2;++eye) native.views[eye].camera=pair.cameras[eye];
    const auto ticket=handoff.Begin(origin,list,3);
    Prepared result{list,3,true,false,{}};
    result.referenceRevision=referenceRevision.load();
    result.valid=staged&&handoff.Publish(ticket,tracking,pair,native)&&handoff.Read(origin,list,native,3,7,result.receipt);
    return result;
}
}
int main()
{
    int failures=0;
    const auto check=[&](bool value,const char* why) { if (!value) { ++failures; std::fprintf(stderr,"CE runtime: %s\n",why); } };
    hooks[Append].original=reinterpret_cast<void*>(&MixedAppend);
    const auto forwarded=reinterpret_cast<MixedAppendFn>(&AppendHook)(0x1122334455667788ull,
        reinterpret_cast<const SaberCamera*>(0x12340),0x110b,-1,1.25f,-2.5f,0xdeadbeef,.125f,
        0x1234,0xabcd,0xfedcba9876543210ull,0xff,-12.5f,14.75f,2,3);
    check(appendArgumentsIntact&&forwarded==0xabcdfedc12345678ull,
        "production append detour preserves register arguments, mixed-width stack slots and return value");
    ComPtr<ID3D11Device> device; ComPtr<ID3D11DeviceContext> context;
    const D3D_FEATURE_LEVEL feature=D3D_FEATURE_LEVEL_11_0;
    if (FAILED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,&feature,1,D3D11_SDK_VERSION,&device,nullptr,&context))) return 1;
    testContext=context.Get();
    testDesc.Width=32; testDesc.Height=16; testDesc.MipLevels=1; testDesc.ArraySize=1;
    testDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM; testDesc.SampleDesc.Count=1;
    testDesc.Usage=D3D11_USAGE_DEFAULT; testDesc.BindFlags=D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> source,destination;
    if (FAILED(device->CreateTexture2D(&testDesc,nullptr,&source))||FAILED(device->CreateTexture2D(&testDesc,nullptr,&destination))) return 1;
    testSource=source.Get(); testDestination=destination.Get();
    std::array<uint8_t,0x100> sourceWrapper{},destinationWrapper{};
    std::memcpy(sourceWrapper.data()+0xe0,&testSource,sizeof(testSource));
    std::memcpy(destinationWrapper.data()+0xe0,&testDestination,sizeof(testDestination));
    RecordResource(reinterpret_cast<uintptr_t>(sourceWrapper.data()));
    RecordResource(reinterpret_cast<uintptr_t>(destinationWrapper.data()));
    std::vector<uint8_t> mapped(contract::imageSize),renderer(0xc000),job(0xc000),nativeBackend(0xd28),nativeConfig(0x330);
    bindings.base=reinterpret_cast<uintptr_t>(mapped.data()); bindings.generation=3;
    bindings.surfaceSelector=reinterpret_cast<uintptr_t>(&NativeDepthSelect);
    depthRoot=reinterpret_cast<uintptr_t>(depthSurfaces[0].data());
    depthBackend=reinterpret_cast<uintptr_t>(nativeBackend.data());
    const uintptr_t configAddress=reinterpret_cast<uintptr_t>(nativeConfig.data());
    const uintptr_t backendVtable=bindings.base+0x17f9d10,textureVtable=bindings.base+0x17fb608;
    std::memcpy(mapped.data()+0x2e3bdd8,&configAddress,8);
    std::memcpy(mapped.data()+0x2e3bde0,&depthBackend,8);
    std::memcpy(mapped.data()+0x2ea2d30,&testContext,8);
    std::memcpy(nativeConfig.data()+0x318,&depthRoot,8);
    std::memcpy(nativeBackend.data(),&backendVtable,8);
    std::memcpy(nativeBackend.data()+0xce0,&testContext,8);
    std::memcpy(nativeBackend.data()+0x48,&depthRoot,8);
    ComPtr<ID3D11Texture2D> depthTexture[2];ComPtr<ID3D11DepthStencilView> depthView[2];
    auto depthDesc=testDesc;depthDesc.Format=DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.BindFlags=D3D11_BIND_DEPTH_STENCIL;
    for (int eye=0;eye<2;++eye)
    {
        if (FAILED(device->CreateTexture2D(&depthDesc,nullptr,&depthTexture[eye]))||
            FAILED(device->CreateDepthStencilView(depthTexture[eye].Get(),nullptr,&depthView[eye]))) return 1;
        depthTextures[eye]=depthTexture[eye].Get();depthViews[eye]=depthView[eye].Get();
        std::memcpy(depthSurfaces[eye].data(),&textureVtable,8);
        const uint32_t flags=1u<<9;std::memcpy(depthSurfaces[eye].data()+0x88,&flags,4);
        SelectDepth(eye);HaloCE_RecordTextureCreated(depthTexture[eye].Get(),depthDesc);
    }
    *reinterpret_cast<int32_t*>(mapped.data()+0x1b7aa84)=1;
    CeObserveRendererMode();
    rendererAddress=reinterpret_cast<uintptr_t>(renderer.data());
    std::memcpy(mapped.data()+0x1bea9e0,&rendererAddress,sizeof(rendererAddress));
    installed=true; active=true; retiring=false; armed=true; generation=3; recenter=false; trackingEnabled=true;
    hudTargetBindingsVerified=true;
    check(HaloCE_HudTargetBindingsVerified(bindings.base,bindings.size,3),
        "cold HUD target proof is available only through the current retained core");
    check(!HaloCE_HudTargetBindingsVerified(bindings.base+1,bindings.size,3)&&
        !HaloCE_HudTargetBindingsVerified(bindings.base,bindings.size-1,3)&&
        !HaloCE_HudTargetBindingsVerified(bindings.base,bindings.size,4),
        "HUD target proof cannot cross module, size or generation boundaries");
    retiring=true;
    check(!HaloCE_HudTargetBindingsVerified(bindings.base,bindings.size,3),
        "core retirement immediately revokes the borrowed HUD target proof");
    retiring=false;hudTargetBindingsVerified=false;
    check(!HaloCE_HudTargetBindingsVerified(bindings.base,bindings.size,3)&&HaloCE_Armed(),
        "failed optional target proof leaves the working camera core armed");
    {
        std::array<uint8_t,0x118> nativeScene{};
        const uintptr_t scene=reinterpret_cast<uintptr_t>(nativeScene.data());
        const uintptr_t sceneVtable=bindings.base+0x1819658;
        std::memcpy(nativeScene.data(),&sceneVtable,8);
        auto* sceneFlags=reinterpret_cast<uint32_t*>(nativeScene.data()+0x110);
        *sceneFlags=0xa8000042u;
        const auto savedScope=jobScope;
        jobScope={reinterpret_cast<uintptr_t>(job.data()),rendererAddress+0xb0,true};
        hooks[SceneCamera].original=reinterpret_cast<void*>(&NativeSceneCamera);
        const auto primary=reinterpret_cast<const float*>(jobScope.activeList+0x70);
        const auto secondary=reinterpret_cast<const float*>(jobScope.activeList+0x438);
        sceneVisibilityRefreshes=0;
        SceneCameraHook(scene,0,primary,secondary);
        check(sceneRequestedRefresh==1&&sceneVisibilityRefreshes==1&&
            scenePrimary==primary&&sceneSecondary==secondary&&*sceneFlags==0xa8001042u,
            "two-eye entry requests native static-scene refresh and preserves camera pointers/other flags");
        SceneCameraHook(scene,0,primary,secondary);
        check(sceneRequestedRefresh==0&&sceneVisibilityRefreshes==1,
            "steady stereo does not rebuild the static scene every frame");
        SceneCameraHook(scene,7,primary,secondary);
        check(sceneRequestedRefresh==7&&sceneVisibilityRefreshes==1,
            "authored native refresh requests remain unchanged");
        armed=false; retiring=true;
        SceneCameraHook(scene,0,primary,nullptr);
        check(sceneRequestedRefresh==1&&sceneVisibilityRefreshes==2&&*sceneFlags==0xa8000042u,
            "stock return during retirement refreshes mono masks without depending on XR arming");
        armed=true; retiring=false;
        const auto copiedPrimary=reinterpret_cast<const float*>(jobScope.job+0xe0);
        const auto copiedSecondary=reinterpret_cast<const float*>(jobScope.job+0x4a8);
        SceneCameraHook(scene,0,copiedPrimary,copiedSecondary);
        check(sceneRequestedRefresh==1&&sceneVisibilityRefreshes==3,
            "copied worker preparation requests the same native transition refresh");
        jobScope.owned=false;
        SceneCameraHook(scene,0,copiedPrimary,nullptr);
        check(sceneRequestedRefresh==0&&sceneVisibilityRefreshes==3,
            "unowned callbacks preserve stock refresh choice");
        jobScope.owned=true;
        SceneCameraHook(scene,0,primary+1,secondary);
        check(sceneRequestedRefresh==0&&sceneVisibilityRefreshes==3,
            "foreign primary-position pointers cannot grant transition ownership");
        *sceneFlags&=~0x1000u;
        SceneCameraHook(scene,0,primary,secondary+1);
        check(sceneRequestedRefresh==0&&sceneVisibilityRefreshes==3,
            "foreign secondary-position pointers cannot grant transition ownership");
        *sceneFlags&=~0x1000u;
        nativeScene[0]^=0x08;
        SceneCameraHook(scene,0,primary,secondary);
        check(sceneRequestedRefresh==0&&sceneVisibilityRefreshes==3,
            "another native scene class cannot inherit the verified layout");
        nativeScene[0]^=0x08;
        *sceneFlags&=~0x1000u;
        SceneCameraHook(scene,0,primary,secondary);
        check(sceneRequestedRefresh==1&&sceneVisibilityRefreshes==4,
            "the next owned transition recovers without stale adapter bookkeeping");
        jobScope=savedScope;
    }
    preparedLists[0].Publish({}); preparedLists[1].Publish({}); renderReady.Publish({});
    hooks[Copy].original=reinterpret_cast<void*>(&NativeCopy);
    hooks[Transfer].original=reinterpret_cast<void*>(&NativeTransfer);
    hooks[Output].original=reinterpret_cast<void*>(&NativeOutput);
    hooks[Frame].original=reinterpret_cast<void*>(&NativeFrame);
    hooks[CameraUpload].original=reinterpret_cast<void*>(&NativeCameraUpload);
    hooks[DepthMesh].original=reinterpret_cast<void*>(&NativeDepthDraw);
    hooks[Prepare].original=reinterpret_cast<void*>(&NativePrepare);
    hooks[ResetList].original=reinterpret_cast<void*>(&NativeReset);
    auto bootstrap=MakePrepared(99,rendererAddress+0xb0,PreparationOrigin::ActiveList);
    bootstrap.valid=false; // native two-view bootstrap has no compatible cache yet
    preparedLists[0].Publish(bootstrap); renderReady.Publish(bootstrap);
    FrameBody(0,0);
    HaloCE_PresentResources(device.Get(),context.Get());
    Wanted selectedRaster{};
    check(allocated.Read(selectedRaster)&&selectedRaster.generation==3&&
        selectedRaster.descriptor.Width==testDesc.Width&&selectedRaster.descriptor.Height==testDesc.Height,
        "native bootstrap copies discover the actual raster and cold Present prepares compatible eye caches");
    nativeCopies=0; previewFolded=0;
    const auto publish=[&](uint64_t serial) {
        const auto p=MakePrepared(serial,rendererAddress+0xb0,PreparationOrigin::ActiveList);
        check(p.valid,"fixture uses the production receipt ledger"); preparedLists[0].Publish(p); renderReady.Publish(p);
    };
    publish(100); FrameBody(0,0);
    FrameDiagnostic initialDepth{};frameDiagnostic.Read(initialDepth);
    if (initialDepth.failure!=FrameFailure::None)
        std::fprintf(stderr,"initial frame failure=%u depth=%u mask=%u\n",unsigned(initialDepth.failure),initialDepth.depthFailure,initialDepth.depthMask);
    EyeCache::Completed pair{};
    check(HaloCE_AcquirePair(context.Get(),101,7,pair),"native frame/output/transfer scopes produce a submitted pair with its older prepared pose");
    if (pair.borrowId)
    {
        Paint(testSource,0xff000000);
        check(Pixels(device.Get(),pair.eyes[0],leftColor)&&Pixels(device.Get(),pair.eyes[1],rightColor),"production capture preserves both eyes after source recycling");
        check(Pixels(device.Get(),testDestination,rightColor),"undersized packed destination gets a bounded last-eye desktop preview");
        check(pair.tracking.serial==100&&pair.tracking.headPosition.y==1.6f,"exact preparation pose survives native copy handoff");
        HaloCE_ReleasePair(pair.borrowId); pair={};
    }
    check(previewFolded.load()==1&&nativeCopies==2,"second-eye packing guard ran exactly once");
    omitRight=true; publish(102); FrameBody(0,0);
    check(!HaloCE_AcquirePair(context.Get(),102,7,pair),"partial native frame never submits");
    omitRight=false; publish(103); FrameBody(0,0);
    check(HaloCE_AcquirePair(context.Get(),103,7,pair),"frame after a partial failure recovers");
    if (pair.borrowId) { HaloCE_ReleasePair(pair.borrowId); pair={}; }
    publish(104); invalidBox=true; const auto before=nativeCopies; FrameBody(0,0); invalidBox=false;
    check(nativeCopies==before&&!HaloCE_AcquirePair(context.Get(),104,7,pair),"invalid native rectangle performs no unsafe GPU operation and submits no pair");
    publish(105); RevokeWrappedResource(reinterpret_cast<uintptr_t>(sourceWrapper.data())); FrameBody(0,0);
    check(!HaloCE_AcquirePair(context.Get(),105,7,pair),"released resource identity cannot lend a stale descriptor");
    RecordResource(reinterpret_cast<uintptr_t>(sourceWrapper.data()));
    publish(106); FrameBody(0,0);
    testTitle=GameTitle::Halo3;
    check(!HaloCE_AcquirePair(context.Get(),106,7,pair),"foreign title cannot borrow CE images");
    testTitle=GameTitle::HaloCE;
    check(!HaloCE_AcquirePair(context.Get(),106,8,pair),"new tracking space cannot borrow old images");
    const auto jobAddress=reinterpret_cast<uintptr_t>(job.data());
    *reinterpret_cast<int*>(job.data()+0xbe58)=1; *reinterpret_cast<int*>(job.data()+0xbe5c)=1;
    const auto copied=MakePrepared(107,jobAddress+0x70,PreparationOrigin::CopiedList);
    reinterpret_cast<SaberViewPair*>(jobAddress+0x70)->count=4;
    preparedLists[1].Publish(copied); PrepareBody(jobAddress);
    Prepared frozen{};
    check(renderReady.Read(frozen)&&frozen.valid&&frozen.receipt.tracking.serial==107&&
        frozen.sourceList==jobAddress+0x70,"copied-list receipt survives native auxiliary culling views");
    FrameBody(0,0);
    check(HaloCE_AcquirePair(context.Get(),107,7,pair),"copied native preparation reaches the real GPU pair");
    if (pair.borrowId) { HaloCE_ReleasePair(pair.borrowId); pair={}; }
    publish(108); FrameBody(0,0);
    HaloCE_Recenter();
    recenter=false; // the next builder has consumed the request
    check(!HaloCE_AcquirePair(context.Get(),108,7,pair),"recenter revokes a completed pair even in the same XR space");
    FrameBody(0,0);
    check(!HaloCE_AcquirePair(context.Get(),109,7,pair),"queued preparation from before recenter cannot regain admission");
    publish(110); FrameBody(0,0);
    check(HaloCE_AcquirePair(context.Get(),110,7,pair),"new reference recovers on the next prepared frame");
    if (pair.borrowId) { HaloCE_ReleasePair(pair.borrowId); pair={}; }
    publish(111);
    Prepared resized{}; renderReady.Read(resized);
    resized.receipt.pair.cameras[0].viewportWidth+=1;
    reinterpret_cast<SaberViewPair*>(rendererAddress+0xb0)->views[0].camera.viewportWidth+=1;
    renderReady.Publish(resized); FrameBody(0,0);
    check(!HaloCE_AcquirePair(context.Get(),111,7,pair),"source dimensions must match the raster whose FOV will be submitted");
    publish(112); FrameBody(0,0);
    CompletedFrame aged{}; completedFrame.Read(aged); aged.capturedAtMs=GetTickCount64()-300; completedFrame.Publish(aged);
    check(!HaloCE_AcquirePair(context.Get(),112,7,pair),"elapsed time rejects old pixels even when the XR serial stops");
    publish(113);
    reinterpret_cast<SaberViewPair*>(rendererAddress+0xb0)->count=3;
    FrameBody(0,0);
    check(HaloCE_AcquirePair(context.Get(),113,7,pair),
        "native auxiliary culling views must not black out both primary eye images");
    if (pair.borrowId) { HaloCE_ReleasePair(pair.borrowId); pair={}; }
    FrameDiagnostic diagnostic{};
    check(frameDiagnostic.Read(diagnostic)&&diagnostic.failure==FrameFailure::None&&
        diagnostic.nativeCount==3&&diagnostic.eyeMask==3&&diagnostic.cameraHeight==testDesc.Height&&
        diagnostic.sourceHeight==testDesc.Height,"worker diagnostics report matched rasters and both actual GPU copies");
    publish(114); omitRight=true; FrameBody(0,0); omitRight=false;
    check(frameDiagnostic.Read(diagnostic)&&diagnostic.failure==FrameFailure::IncompletePair&&diagnostic.eyeMask==1,
        "missing eye is distinguished from camera receipt and raster rejection");
    publish(115); reinterpret_cast<SaberViewPair*>(rendererAddress+0xb0)->views[1].camera.pose.matrix[12]+=100;
    FrameBody(0,0);
    check(frameDiagnostic.Read(diagnostic)&&diagnostic.failure==FrameFailure::CameraChanged&&
        diagnostic.cameraDifference>=sizeof(SaberCamera)&&diagnostic.eyeMask==0,
        "a displaced right camera is rejected and identified before any GPU capture");
    publish(116); omitDepth=true; FrameBody(0,0); omitDepth=false;
    check(!HaloCE_AcquirePair(context.Get(),116,7,pair)&&frameDiagnostic.Read(diagnostic)&&
        diagnostic.failure==FrameFailure::DepthResource&&diagnostic.depthFailure==5,
        "prepared cameras and two GPU colors cannot prove a missing native depth-camera consumption");
    publish(117); omitShading=true; FrameBody(0,0); omitShading=false;
    check(!HaloCE_AcquirePair(context.Get(),117,7,pair)&&frameDiagnostic.Read(diagnostic)&&
        diagnostic.consumerFailure==5,"missing native shading upload rejects the current pair only");
    publish(118); foreignUploadCamera=true; FrameBody(0,0); foreignUploadCamera=false;
    check(!HaloCE_AcquirePair(context.Get(),118,7,pair)&&frameDiagnostic.Read(diagnostic)&&
        diagnostic.consumerFailure==1,"identical bytes at an unrelated camera address cannot claim a native primary eye");
    publish(119); changedUploadCamera=true; FrameBody(0,0); changedUploadCamera=false;
    check(!HaloCE_AcquirePair(context.Get(),119,7,pair)&&frameDiagnostic.Read(diagnostic)&&
        diagnostic.consumerFailure==2,"camera changes after frame entry are rejected at the actual render consumer");
    publish(120); changedUploadPlayer=true; FrameBody(0,0); changedUploadPlayer=false;
    check(!HaloCE_AcquirePair(context.Get(),120,7,pair)&&frameDiagnostic.Read(diagnostic)&&
        diagnostic.consumerFailure==3,"camera source-player changes outside the pose prefix cannot claim the right eye");
    publish(121); FrameBody(0,0);
    check(HaloCE_AcquirePair(context.Get(),121,7,pair)&&frameDiagnostic.Read(diagnostic)&&
        diagnostic.consumedDepth==3&&diagnostic.consumedScene==3&&diagnostic.consumedShading==3&&
        diagnostic.consumedCamera[0]==rendererAddress+0xf0&&
        diagnostic.consumedCamera[1]==rendererAddress+0xf0+sizeof(SaberView)&&
        diagnostic.sourceResource[0]==reinterpret_cast<uintptr_t>(testSource)&&
        diagnostic.sourceResource[1]==reinterpret_cast<uintptr_t>(testSource)&&
        diagnostic.copyContext[0]==reinterpret_cast<uintptr_t>(testContext),
        "a new frame recovers and reports actual consumed cameras and recycled per-eye source identity");
    if (pair.borrowId) { HaloCE_ReleasePair(pair.borrowId); pair={}; }
    aliasDepthResource=true;publish(122);FrameBody(0,0);aliasDepthResource=false;
    check(!HaloCE_AcquirePair(context.Get(),122,7,pair)&&frameDiagnostic.Read(diagnostic)&&
        diagnostic.depthFailure==4&&diagnostic.depthResource[0]==diagnostic.depthResource[1]&&
        diagnostic.depthView[0]!=diagnostic.depthView[1],
        "different DSV identities cannot admit two eyes using the same depth texture");
    aliasDepthView=true;publish(123);FrameBody(0,0);aliasDepthView=false;
    check(!HaloCE_AcquirePair(context.Get(),123,7,pair)&&frameDiagnostic.Read(diagnostic)&&
        diagnostic.depthFailure==4,"aliased depth views reject a manufactured pair");
    wrongBoundDepth=true;publish(124);FrameBody(0,0);wrongBoundDepth=false;
    check(!HaloCE_AcquirePair(context.Get(),124,7,pair)&&frameDiagnostic.Read(diagnostic)&&
        diagnostic.depthFailure==1,"native depth draw must bind the selected native depth view");
    changeDepthAtScene=true;publish(125);FrameBody(0,0);changeDepthAtScene=false;
    check(!HaloCE_AcquirePair(context.Get(),125,7,pair)&&frameDiagnostic.Read(diagnostic)&&
        diagnostic.depthFailure==5,"scene cannot silently select the opposite eye depth");
    omitDepthDraw=true;publish(126);FrameBody(0,0);omitDepthDraw=false;
    check(!HaloCE_AcquirePair(context.Get(),126,7,pair)&&frameDiagnostic.Read(diagnostic)&&
        diagnostic.depthFailure==5,"camera uploads alone do not establish completed native depth draws");
    publish(127);FrameBody(0,0);
    check(HaloCE_AcquirePair(context.Get(),127,7,pair)&&frameDiagnostic.Read(diagnostic)&&
        diagnostic.depthMask==3&&diagnostic.depthFailure==0,
        "independent current depth restores frame submission after each rejected pair");
    if (pair.borrowId) { HaloCE_ReleasePair(pair.borrowId); pair={}; }
    recreateDepthAtScene=true;publish(128);FrameBody(0,0);recreateDepthAtScene=false;
    check(!HaloCE_AcquirePair(context.Get(),128,7,pair)&&frameDiagnostic.Read(diagnostic)&&
        diagnostic.depthFailure==5,"reused texture pointers cannot borrow depth from an earlier resource lifetime");
    auxiliaryDepthOverwrite=true;publish(129);FrameBody(0,0);auxiliaryDepthOverwrite=false;
    check(!HaloCE_AcquirePair(context.Get(),129,7,pair)&&frameDiagnostic.Read(diagnostic)&&
        diagnostic.depthFailure==6,"an auxiliary depth draw cannot overwrite a completed primary depth");
    publish(130);FrameBody(0,0);
    check(HaloCE_AcquirePair(context.Get(),130,7,pair),"new depth lifetime and clean native frame recover without disarming");
    if (pair.borrowId) { HaloCE_ReleasePair(pair.borrowId); pair={}; }
    {
        FrameScope post{};
        post.synthetic=post.capture=true;post.renderer=rendererAddress;
        post.prepared=MakePrepared(131,rendererAddress+0xb0,PreparationOrigin::ActiveList);
        post.diagnostic.consumedDepth=post.diagnostic.consumedScene=post.diagnostic.consumedShading=3;
        auto current=post.prepared.receipt.tracking;current.serial=133;current.motionBlur=true;
        trackingSnapshot.Publish(current);trackingAtMs.store(GetTickCount64());
        frameScope=&post;
        Tracking borrowed{};
        for (int eye=0;eye<2;++eye)
        {
            auto* camera=reinterpret_cast<SaberCamera*>(rendererAddress+0xf0+eye*sizeof(SaberView));
            post.lastSceneEye=eye;
            post.primaryDrawEye=eye;post.primaryDrawStage=3;
            post.diagnostic.consumedCamera[eye]=reinterpret_cast<uintptr_t>(camera);
            NativeCameraUpload(0,0,camera);
            check(HaloCE_GetAnniversaryEyeTracking(camera,borrowed)&&borrowed.serial==131&&!borrowed.motionBlur,
                "post effects borrow each current primary's frozen preparation settings rather than newer XR input");
            check(HaloCE_GetAnniversaryPrimaryEyeTracking(borrowed)&&borrowed.serial==131,
                "material lens uses the same frozen primary camera settings");
            auto foreign=*camera;
            check(!HaloCE_GetAnniversaryEyeTracking(&foreign,borrowed)&&!borrowed.serial,
                "identical copied camera bytes cannot lend post-effect ownership");
            const auto saved=*camera;camera->pose.matrix[12]+=1;
            check(!HaloCE_GetAnniversaryEyeTracking(camera,borrowed),"post effects reject camera mutation after shading");
            check(!HaloCE_GetAnniversaryPrimaryEyeTracking(borrowed),"material lens rejects changed native camera bytes");
            *camera=saved;
        }
        auto* camera=reinterpret_cast<SaberCamera*>(rendererAddress+0xf0+sizeof(SaberView));
        const auto selected=reinterpret_cast<SaberCamera*>(rendererAddress+0xf0);
        NativeCameraUpload(0,0,selected);
        check(!HaloCE_GetAnniversaryEyeTracking(camera,borrowed),"a later selected camera revokes current-scene post ownership");
        check(!HaloCE_GetAnniversaryPrimaryEyeTracking(borrowed),"material lens rejects a changed native selected camera");
        NativeCameraUpload(0,0,camera);
        post.diagnostic.consumedShading=1;
        check(!HaloCE_GetAnniversaryEyeTracking(camera,borrowed),"post effects require completed shading consumption");
        check(!HaloCE_GetAnniversaryPrimaryEyeTracking(borrowed),"shading material scope requires its own consumed camera receipt");
        post.diagnostic.consumedShading=3;
        post.capture=false;
        check(!HaloCE_GetAnniversaryEyeTracking(camera,borrowed),"dropped pairs cannot lend post-effect ownership");
        check(!HaloCE_GetAnniversaryPrimaryEyeTracking(borrowed),"dropped pairs cannot lend material lens ownership");
        post.capture=true;current.serial=140;trackingSnapshot.Publish(current);
        check(!HaloCE_GetAnniversaryEyeTracking(camera,borrowed),"post effects reject an old preparation beyond the XR serial window");
        check(!HaloCE_GetAnniversaryPrimaryEyeTracking(borrowed),"material lens rejects stale tracking serials");
        current.serial=133;trackingSnapshot.Publish(current);trackingAtMs.store(GetTickCount64()-300);
        check(!HaloCE_GetAnniversaryEyeTracking(camera,borrowed),"post effects reject expired XR publication");
        check(!HaloCE_GetAnniversaryPrimaryEyeTracking(borrowed),"material lens rejects expired tracking publication");
        trackingAtMs.store(GetTickCount64());current.spaceEpoch=8;trackingSnapshot.Publish(current);
        check(!HaloCE_GetAnniversaryEyeTracking(camera,borrowed),"post effects reject a different XR reference space");
        check(!HaloCE_GetAnniversaryPrimaryEyeTracking(borrowed),"material lens rejects changed XR space");
        current.spaceEpoch=7;trackingSnapshot.Publish(current);
        ++post.prepared.referenceRevision;
        check(!HaloCE_GetAnniversaryEyeTracking(camera,borrowed),"post effects reject a changed tracking reference");
        check(!HaloCE_GetAnniversaryPrimaryEyeTracking(borrowed),"material lens rejects changed reference revision");
        --post.prepared.referenceRevision;
        *reinterpret_cast<int32_t*>(mapped.data()+0x1b7aa84)=0;
        check(!HaloCE_GetAnniversaryEyeTracking(camera,borrowed),"Original renderer cannot borrow an Anniversary post scope");
        check(!HaloCE_GetAnniversaryPrimaryEyeTracking(borrowed),"Original renderer cannot borrow Anniversary material ownership");
        *reinterpret_cast<int32_t*>(mapped.data()+0x1b7aa84)=1;
        check(HaloCE_GetAnniversaryEyeTracking(camera,borrowed),"current post-effect ownership recovers after rejected observations");
        nestedMaterialProbe=true;
        FrameBody(0,0);
        check(nestedMaterialRejected&&!anniversaryMaterialMasked&&HaloCE_GetAnniversaryPrimaryEyeTracking(borrowed),
            "nested frames cannot borrow an outer material eye before their first camera upload");
        nestedMaterialFault=true;
        check(InvokeNestedMaterialFault()&&nestedMaterialRejected&&!anniversaryMaterialMasked&&
            HaloCE_GetAnniversaryPrimaryEyeTracking(borrowed),
            "native nested-frame unwind restores outer material scope without lending it to the nested frame");
        nestedMaterialProbe=nestedMaterialFault=false;
        ++testGeneration;
        check(!HaloCE_GetAnniversaryPrimaryEyeTracking(borrowed),"material lens rejects a retired title generation");
        --testGeneration;
        post.diagnostic.nativeCount=3;
        CameraUploadBody(0,0,camera,bindings.base+0x1234);
        check(!HaloCE_GetAnniversaryPrimaryEyeTracking(borrowed),
            "unknown native camera uploads revoke the preceding primary material scope");
        CameraUploadBody(0,0,camera,bindings.base+0x4562bf);
        check(HaloCE_GetAnniversaryPrimaryEyeTracking(borrowed),
            "a verified depth upload admits material projection before scene or shading");
        auto* auxiliary=reinterpret_cast<SaberCamera*>(rendererAddress+0xf0+2*sizeof(SaberView));
        CameraUploadBody(0,0,auxiliary,bindings.base+0x4562bf);
        check(!HaloCE_GetAnniversaryPrimaryEyeTracking(borrowed),
            "auxiliary and reflection cameras cannot lend material projection ownership");
        frameScope=nullptr;
        check(!HaloCE_GetAnniversaryEyeTracking(camera,borrowed),"post-effect ownership ends with the native frame scope");
        check(!HaloCE_GetAnniversaryPrimaryEyeTracking(borrowed),"material ownership ends with the native frame scope");
    }
    {
        publish(132);Prepared prepared{};renderReady.Read(prepared);
        HaloCE_PublishTracking(prepared.receipt.tracking,true);recenter=false;
        inspectPrimaryDraw=true;FrameBody(0,0);inspectPrimaryDraw=false;
        Tracking ended{};
        check(primaryDrawCorrect&&primaryDrawStages==7&&primaryDrawOutputs==2&&
            !HaloCE_GetAnniversaryPrimaryEyeTracking(ended),
            "both eyes own material projection at depth/scene/shading, never during upload/output or after frame");
        check(HaloCE_AcquirePair(context.Get(),132,7,pair),"material stage observations preserve both world captures");
        if (pair.borrowId) { HaloCE_ReleasePair(pair.borrowId);pair={}; }
    }
    {
        // Cover the complete frame -> per-eye replay -> native callback ->
        // actual GPU capture chain. be2140f lost the render flags before this
        // boundary, which all earlier standalone layout/target tests missed.
        std::array<uint8_t,0xb8> players{};
        std::array<uint8_t,3> rendering{{0,0,1}};
        std::array<uint8_t,0x18> display{};
        std::array<uint8_t,4*0x48> targetStack{};
        const uintptr_t playersAddress=reinterpret_cast<uintptr_t>(players.data());
        const uintptr_t renderingAddress=reinterpret_cast<uintptr_t>(rendering.data());
        const uintptr_t displayAddress=reinterpret_cast<uintptr_t>(display.data());
        const uintptr_t storageAddress=reinterpret_cast<uintptr_t>(targetStack.data());
        const uintptr_t callbackAddress=bindings.base+contract::anniversary_hud::native_hud_callback;
        const int16_t playerCount=1;const int32_t depth=1,capacity=4;
        const UINT height=testDesc.Height*2;
        hudRoot=reinterpret_cast<uintptr_t>(sourceWrapper.data());
        std::memcpy(sourceWrapper.data(),&textureVtable,8);
        std::memcpy(mapped.data()+0x2d62890,&hudRoot,8);
        std::memcpy(mapped.data()+0x1c33fe0,&callbackAddress,8);
        std::memcpy(mapped.data()+0x2ea2d90,&playersAddress,8);
        std::memcpy(players.data()+0xb4,&playerCount,2);
        mapped[0x2d9bdd1]=mapped[0x2e3b829]=1;
        std::memcpy(mapped.data()+0x2d91330,&renderingAddress,8);
        std::memcpy(nativeConfig.data()+0x118,&displayAddress,8);
        std::memcpy(display.data()+0x10,&testDesc.Width,4);
        std::memcpy(display.data()+0x14,&height,4);
        std::memcpy(nativeBackend.data()+8,&storageAddress,8);
        std::memcpy(nativeBackend.data()+0x10,&depth,4);
        std::memcpy(nativeBackend.data()+0x14,&capacity,4);
        Camera camera{};camera.position={10,20,30};camera.forward={1,0,0};camera.up={0,0,1};
        camera.verticalFov=1;camera.nearPlane=.01f;camera.farPlane=1000;
        camera.window=camera.viewport={0,0,static_cast<int16_t>(height),static_cast<int16_t>(testDesc.Width)};
        std::memcpy(mapped.data()+0x2d9cb34,&camera,sizeof(camera));
        check(InstallFixtureJump(bindings.base+contract::anniversary_hud::native_target_push,
                reinterpret_cast<uintptr_t>(&NativeHudPush))&&
            InstallFixtureJump(bindings.base+contract::anniversary_hud::native_target_pop,
                reinterpret_cast<uintptr_t>(&NativeHudPop))&&
            InstallFixtureJump(bindings.base+contract::anniversary_hud::native_hud_preamble,
                reinterpret_cast<uintptr_t>(&NativeHudPreamble)),"private native HUD fixture entry points");
        anniversaryHudHook.original=reinterpret_cast<void*>(&NativeHudCallback);
        anniversaryHudInstalled=true;ConfigureCeHudLayoutRuntimeFixture(3,true);
        const D3D11_VIEWPORT priorViewport{2,3,20,10,.2f,.8f};
        const D3D11_RECT priorScissor{2,3,22,13};
        auto hudFrame=[&](uint64_t serial,uint32_t flags,bool expectedHud,
            bool expectedPair=true,unsigned expectedCallbacks=UINT_MAX,bool expectedClean=true) {
            if (recyclePreparedSource)
            {
                const auto next=MakePrepared(serial,jobAddress+0x70,PreparationOrigin::CopiedList);
                preparedLists[1].Publish(next);PrepareBody(jobAddress);
            }
            else publish(serial);
            Prepared prepared{};renderReady.Read(prepared);
            HaloCE_PublishTracking(prepared.receipt.tracking,true);recenter=false;
            publishedReference.Publish({{prepared.receipt.tracking.headPosition,{},7,3},referenceRevision.load()});
            ObserveRaster(priorViewport,priorScissor);
            const auto beforeHud=hudCallbacks;
            FrameBody(0,flags);
            check(hudCallbacks-beforeHud==(expectedCallbacks==UINT_MAX?(expectedHud?2u:0u):expectedCallbacks),
                "native HUD-enable bit and guards reach the expected output callbacks");
            check(HaloCE_AcquirePair(context.Get(),serial,7,pair)==expectedPair,
                "HUD cleanup ownership determines whether the current world pair can be submitted");
            if (pair.borrowId)
            {
                check(Pixels(device.Get(),pair.eyes[0],expectedHud?hudColor:leftColor)&&
                    Pixels(device.Get(),pair.eyes[1],expectedHud?hudColor:rightColor),
                    "HUD callback pixels reach the two production eye captures only when admitted");
                HaloCE_ReleasePair(pair.borrowId);pair={};
            }
            if (expectedClean)
            {
                D3D11_VIEWPORT after{};D3D11_RECT rect{};UINT n=1,m=1;
                testContext->RSGetViewports(&n,&after);testContext->RSGetScissorRects(&m,&rect);
                check(n==1&&m==1&&!std::memcmp(&after,&priorViewport,sizeof(after))&&
                    !std::memcmp(&rect,&priorScissor,sizeof(rect)),"complete replay restores exact pre-HUD GPU raster");
            }
            int32_t afterDepth{};uintptr_t afterOutput{};
            Read(depthBackend+0x10,afterDepth);Read(bindings.base+0x2e3d0d0,afterOutput);
            check((!expectedClean||afterDepth==1)&&afterOutput==0,
                "HUD restores borrowed output and every still-owned native target stack");
            check(!anniversaryHudReplay&&HaloCE_Armed(),
                "optional HUD work clears its thread scope and preserves the CE core");
        };
        hudFrame(134,0,false);
        check(anniversaryHudFailure.load()==10,
            "native HUD frame-bit refusal is distinguished from camera and source failures");
        hudFrame(135,0x10,true);
        check(anniversaryHudFailure.load()==0,
            "recovered native HUD clears its old rejection reason");
        const int32_t noCapacity=2;std::memcpy(nativeBackend.data()+0x14,&noCapacity,4);
        hudFrame(136,0x10,false);
        std::memcpy(nativeBackend.data()+0x14,&capacity,4);
        hudFrame(137,0x10,true);
        hudFault=HudFault::CallbackException;
        hudFrame(138,0x10,false,true,2);
        check(anniversaryHudFailure.load()==4,"a restored callback exception is a feature-only fallback");
        hudFault=HudFault::None;hudFrame(139,0x10,true);
        hudFault=HudFault::PreambleException;
        hudFrame(140,0x10,false);
        check(anniversaryHudFailure.load()==4,"a restored preamble exception cannot escape the optional feature");
        hudFault=HudFault::None;hudFrame(141,0x10,true);
        hudFault=HudFault::UnownedStack;
        hudFrame(142,0x10,false,false,1,false);
        check(anniversaryHudFailure.load()==5,"unverifiable cleanup drops only the affected frame");
        // Model the native owner restoring its own fresh stack before the
        // next frame; the adapter must not pop an unowned stack entry.
        int32_t preservedDepth{};Read(depthBackend+0x10,preservedDepth);
        check(preservedDepth==3,"unowned stack entries are preserved instead of guessed away");
        std::memcpy(nativeBackend.data()+0x10,&depth,4);
        hudFault=HudFault::None;hudFrame(143,0x10,true);
        // Numeric observation is available before the optional layout hook
        // installs. Rejected replay has not mutated its raster, so it must
        // preserve both world captures and refrain from calling native HUD.
        ConfigureCeHudLayoutRuntimeFixture(3,true,false);
        hudFrame(144,0x10,false);
        check(anniversaryHudFailure.load()==3,
            "uninstalled optional layout is a feature-only admission fallback");
        ConfigureCeHudLayoutRuntimeFixture(3,true);
        hudFrame(145,0x10,true);
        Camera invalidCamera{};
        std::memcpy(mapped.data()+0x2d9cb34,&invalidCamera,sizeof(invalidCamera));
        hudFrame(146,0x10,false);
        check(anniversaryHudFailure.load()==20,
            "missing stock HUD camera leaves world eyes intact with a precise reason");
        std::memcpy(mapped.data()+0x2d9cb34,&camera,sizeof(camera));
        hudFrame(147,0x10,true);
        check(hudRasterCorrect&&anniversaryHudDraws.load()==14,
            "both-eye HUD raster and capture recover after rejected and faulted optional callbacks");
        recyclePreparedSource=true;
        hudFrame(148,0x10,true);
        Prepared renderedReceipt{};renderReady.Read(renderedReceipt);
        check(!handoff.Current(renderedReceipt.receipt.ticket)&&anniversaryHudFailure.load()==0,
            "recycled preparation source cannot revoke the frozen in-flight HUD eye receipt");
        recyclePreparedSource=false;
        anniversaryHudInstalled=false;anniversaryHudHook={};hudRoot=0;
        ConfigureCeHudLayoutRuntimeFixture(3,false);
    }
    {
        // The Anniversary native builder must publish its untouched center
        // for controls/shot consumers outside render callbacks. Earlier WIP
        // only published this receipt from Classic, leaving Anniversary inert.
        Tracking tracking{}; tracking.generation=3; tracking.spaceEpoch=7; tracking.serial=140;
        tracking.headPosition={.4f,1.7f,-.3f};
        HaloCE_PublishTracking(tracking,true); recenter=false;
        const Reference frozen{{0,1.6f,0},{},7,3};
        Camera native{}; native.position={10,20,30}; native.forward={1,0,0}; native.up={0,0,1};
        native.verticalFov=1; native.viewport=native.window={0,0,32,32};
        native.nearPlane=.01f; native.farPlane=1000;
        const Vec3 offset{17,-9,23}; const float bias=2.5f;
        std::array<uint8_t,0x138> nativeScene{};
        const uintptr_t sceneAddress=reinterpret_cast<uintptr_t>(nativeScene.data());
        std::memcpy(mapped.data()+0x2e3c418,&sceneAddress,sizeof(sceneAddress));
        std::memcpy(mapped.data()+0x2b05118,&offset,sizeof(offset));
        std::memcpy(mapped.data()+0x2e3b838,&bias,sizeof(bias));
        SaberCamera saber{}; BuildSaberPose(native,offset,bias,saber.pose);
        saber.viewportWidth=32; saber.viewportHeight=32; saber.verticalFovDegrees=57.2957795f;
        saber.nearPlane=.03f; saber.farPlane=3000;
        const auto revision=referenceRevision.load();
        gameplayCamera.Publish({}); RenderContext received{};
        check(!HaloCE_GetGameplayContext(received),"Anniversary controls require an actual stock-camera publication");
        gameplayBridgeVerified=false;
        PublishAnniversaryGameplayContext(saber,tracking,frozen,revision,Game_GetWorldScale(),true);
        check(!HaloCE_GetGameplayContext(received),"unverified native bridge stays stock for controls only");
        gameplayBridgeVerified=true;
        PublishAnniversaryGameplayContext(saber,tracking,frozen,revision,Game_GetWorldScale(),true);
        check(HaloCE_GetGameplayContext(received)&&std::fabs(received.camera.position.x-10)<.0001f&&
            std::fabs(received.camera.position.y-20)<.0001f&&std::fabs(received.camera.position.z-30)<.0001f&&
            received.tracking.serial==140&&received.referenceRevision==revision,
            "Anniversary stock-camera receipt reaches nonrender controls without tracked-eye offsets");
        tracking.serial=141; tracking.headPosition.x=.7f; HaloCE_PublishTracking(tracking,true);
        check(HaloCE_GetGameplayContext(received)&&received.tracking.serial==141&&
            received.tracking.headPosition.x==.7f&&std::fabs(received.camera.position.x-10)<.0001f,
            "controls refresh XR input while preserving the native center and matching reference");
        mapped[0x2e3b826]=1;
        PublishAnniversaryGameplayContext(saber,tracking,frozen,revision,Game_GetWorldScale(),true);
        check(!HaloCE_GetGameplayContext(received),"native free camera immediately revokes prior gameplay publication");
        mapped[0x2e3b826]=0; nativeScene[0x130]=1;
        PublishAnniversaryGameplayContext(saber,tracking,frozen,revision,Game_GetWorldScale(),true);
        check(!HaloCE_GetGameplayContext(received),"external native scene camera cannot be inverted as a gameplay center");
        nativeScene[0x130]=0;
        PublishAnniversaryGameplayContext(saber,tracking,frozen,revision,Game_GetWorldScale(),true);
        check(HaloCE_GetGameplayContext(received),"ordinary native camera publication recovers after external mode");
        HaloCE_Recenter();
        check(!HaloCE_GetGameplayContext(received),"recenter revokes the nonrender Anniversary control receipt");
        recenter=false;
        PublishAnniversaryGameplayContext(saber,tracking,frozen,revision,Game_GetWorldScale(),true);
        check(!HaloCE_GetGameplayContext(received),"old builder revision cannot republish after recenter");
        PublishAnniversaryGameplayContext(saber,tracking,frozen,referenceRevision.load(),Game_GetWorldScale(),true);
        check(HaloCE_GetGameplayContext(received),"new native center recovers Anniversary controls");
        *reinterpret_cast<int32_t*>(mapped.data()+0x1b7aa84)=0;
        check(!HaloCE_GetGameplayContext(received),"graphics-mode switch revokes Anniversary controls before reuse");
        *reinterpret_cast<int32_t*>(mapped.data()+0x1b7aa84)=1; CeObserveRendererMode();
    }
    {
        // An Original-mode preparation may observe the graphics toggle only
        // after it started. Its bookkeeping scope owns no tracking reference.
        // Require the camera guard before even forcing native secondary=1.
        std::array<uint8_t,0x10> clock{};clock[0]=1;
        const int32_t tick=20,count=1;
        std::memcpy(clock.data()+0xc,&tick,4);
        const uintptr_t clockAddress=reinterpret_cast<uintptr_t>(clock.data());
        std::memcpy(mapped.data()+0x2e9fd68,&clockAddress,8);
        Camera center{};center.forward={1,0,0};center.up={0,0,1};
        center.verticalFov=1;center.nearPlane=.01f;center.farPlane=1000;
        center.viewport=center.window={0,0,32,32};
        SaberCamera source{};BuildSaberPose(center,{},0,source.pose);
        source.viewportWidth=source.viewportHeight=32;
        source.verticalFovDegrees=57.2957795f;source.nearPlane=.03f;source.farPlane=3000;
        const uintptr_t sourceAddress=reinterpret_cast<uintptr_t>(&source);
        const uintptr_t arrayAddress=reinterpret_cast<uintptr_t>(&sourceAddress);
        std::memcpy(mapped.data()+0x2b17b98,&count,4);
        std::memcpy(mapped.data()+0x2b17b90,&arrayAddress,8);
        auto tracking=MakePrepared(160,jobAddress+0x70,PreparationOrigin::CopiedList).receipt.tracking;
        HaloCE_PublishTracking(tracking,true);
        const auto previousScope=jobScope;
        const auto previousReference=reference;
        hooks[Builder].original=reinterpret_cast<void*>(&NativeBuilder);
        jobScope={jobAddress,rendererAddress+0xb0,true,false};
        recenter=true;
        check(LiveGame()&&SingleCamera(),"builder camera-ownership fixture reaches native eligibility");
        check(BuilderHook(0,reinterpret_cast<SaberViewPair*>(jobAddress+0x70),0,nullptr)==0xceba1234&&
            observedBuilderSecondary==0&&!observedBuilderTracking&&recenter.load()&&
            !std::memcmp(&reference,&previousReference,sizeof(reference)),
            "Original stock job cannot force stereo or consume recenter after a graphics toggle");
        jobScope.cameraOwned=true;
        check(BuilderHook(0,reinterpret_cast<SaberViewPair*>(jobAddress+0x70),0,nullptr)==0xceba1234&&
            observedBuilderSecondary==1&&observedBuilderTracking&&!recenter.load()&&
            reference.generation==tracking.generation&&reference.spaceEpoch==tracking.spaceEpoch,
            "camera-owned Anniversary job still reaches tracked native construction");
        jobScope=previousScope;
    }
    ResourceRegistry::Record recorded{};
    resources.Forget(reinterpret_cast<uintptr_t>(testSource));
    HaloCE_RecordTextureCreated(testSource,testDesc);
    check(resources.Read(reinterpret_cast<uintptr_t>(testSource),reinterpret_cast<uintptr_t>(testSource),recorded),
        "early creation metadata bootstraps a texture created before native hooks");
    check(!Remove()&&installed.load(),"retirement keeps protective hooks while native synthetic lists remain");
    check(ResetListHook(jobAddress+0x70)==0xfedcba9876543210ull,"native list reset preserves its return register");
    ResetListHook(rendererAddress+0xb0);
    check(!HasSyntheticLists(),"native reset retires synthetic source and active lists");
    check(Remove(),"drained adapter resources retire without touching another title");
    check(!resources.Read(reinterpret_cast<uintptr_t>(testSource),reinterpret_cast<uintptr_t>(testSource),recorded),
        "module retirement invalidates previous pointer identities");
    return failures?1:0;
}
