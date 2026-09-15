// Runs the production CE adapter against native-call fixtures and real WARP
// textures. It never maps executable game code, installs hooks or opens MCC.
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
void Logf(const char*,...) { }
bool WaitForNativeDetourQuiescence(const void* const*,const void* const*,size_t count,
    const std::atomic<uint32_t>& value) { return count<=8&&!value.load(); }

namespace
{
ID3D11DeviceContext* testContext{};
ID3D11Texture2D* testSource{};
ID3D11Texture2D* testDestination{};
D3D11_TEXTURE2D_DESC testDesc{};
bool omitRight{};
bool invalidBox{};
unsigned nativeCopies{};
uintptr_t rendererAddress{};
constexpr uint32_t leftColor=0xff123456,rightColor=0xffabcdef;
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
    SurfaceTransfer request{0x111,0x222,0,0,0,0,0,static_cast<int>(eye*testDesc.Height),0,0,
        static_cast<int>(testDesc.Width),static_cast<int>(testDesc.Height)};
    TransferBody(0,&request,bindings.base+0x45e376);
}
void __fastcall NativeFrame(uintptr_t,uint32_t)
{
    Paint(testSource,leftColor); OutputBody(0);
    if (!omitRight) { Paint(testSource,rightColor); OutputBody(1); }
}
uintptr_t __fastcall NativeReset(uintptr_t list)
{ *reinterpret_cast<SaberViewPair*>(list)={}; return 0xfedcba9876543210ull; }
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
    StagedViewPair pair{}; pair.serial=serial; pair.generation=3; pair.spaceEpoch=7;
    for (int eye=0;eye<2;++eye)
    {
        native.views[eye].flags=eye?0x20b:0x10b; native.views[eye].viewIndex=eye;
        auto& camera=native.views[eye].camera;
        camera.viewportWidth=static_cast<float>(testDesc.Width); camera.viewportHeight=static_cast<float>(testDesc.Height);
        camera.pose.matrix[12]=static_cast<float>(eye); pair.cameras[eye]=camera;
        pair.covers[eye]={1.8f,1.0f,.9f};
    }
    const auto ticket=handoff.Begin(origin,list,3);
    Prepared result{list,3,true,false,{}};
    result.referenceRevision=referenceRevision.load();
    result.valid=handoff.Publish(ticket,tracking,pair,native)&&handoff.Read(origin,list,native,3,7,result.receipt);
    return result;
}
}
int main()
{
    int failures=0;
    const auto check=[&](bool value,const char* why) { if (!value) { ++failures; std::fprintf(stderr,"CE runtime: %s\n",why); } };
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
    std::vector<uint8_t> mapped(contract::imageSize),renderer(0xc000),job(0xc000);
    bindings.base=reinterpret_cast<uintptr_t>(mapped.data()); bindings.generation=3;
    rendererAddress=reinterpret_cast<uintptr_t>(renderer.data());
    std::memcpy(mapped.data()+0x1bea9e0,&rendererAddress,sizeof(rendererAddress));
    installed=true; active=true; retiring=false; armed=true; generation=3; recenter=false; trackingEnabled=true;
    preparedLists[0].Publish({}); preparedLists[1].Publish({}); renderReady.Publish({});
    hooks[Copy].original=reinterpret_cast<void*>(&NativeCopy);
    hooks[Transfer].original=reinterpret_cast<void*>(&NativeTransfer);
    hooks[Output].original=reinterpret_cast<void*>(&NativeOutput);
    hooks[Frame].original=reinterpret_cast<void*>(&NativeFrame);
    hooks[Prepare].original=reinterpret_cast<void*>(&NativePrepare);
    hooks[ResetList].original=reinterpret_cast<void*>(&NativeReset);
    check(cache.Prepare(device.Get(),context.Get(),testDesc,3,1),"preallocate actual GPU caches");
    const auto publish=[&](uint64_t serial) {
        const auto p=MakePrepared(serial,rendererAddress+0xb0,PreparationOrigin::ActiveList);
        check(p.valid,"fixture uses the production receipt ledger"); preparedLists[0].Publish(p); renderReady.Publish(p);
    };
    publish(100); FrameBody(0,0);
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
    preparedLists[1].Publish(copied); PrepareBody(jobAddress);
    Prepared frozen{};
    check(renderReady.Read(frozen)&&frozen.valid&&frozen.receipt.tracking.serial==107&&
        frozen.sourceList==jobAddress+0x70,"production prepare wrapper freezes the explicit copied-list receipt");
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
