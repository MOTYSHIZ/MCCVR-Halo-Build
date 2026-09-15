// Production snapshot/restore helper with native-service fixtures and a real
// WARP output merger. Pinned binder instructions are exercised separately by
// tools/re/test_ce_hud_target_native.py, without replacing their arithmetic.
#include "../src/dll/haloce_hud_target.cpp"
#include <cstdio>
#include <vector>

static unsigned failures{},bindCalls{};
static bool bindAllowed=true;
static ID3D11DeviceContext* context{};
static void Check(bool value,const char* text)
{ if (!value) { ++failures;std::fprintf(stderr,"CE HUD target: %s\n",text); } }
template<class T> static void Put(uintptr_t address,T value)
{ std::memcpy(reinterpret_cast<void*>(address),&value,sizeof(value)); }
static uintptr_t __fastcall SelectNativeSurface(uintptr_t wrapper) { return wrapper; }
static bool __fastcall BindNativeTarget(uintptr_t backend,const void* source)
{
    ++bindCalls;
    if (!bindAllowed) return false;
    std::memmove(reinterpret_cast<void*>(backend+0x18),source,0x48);
    const auto descriptor=reinterpret_cast<uintptr_t>(source);
    const auto wrapper=ReadValue<uintptr_t>(descriptor+0x10);
    const auto table=ReadValue<uintptr_t>(wrapper+0xe8);
    auto* rtv=ReadValue<ID3D11RenderTargetView*>(table);
    const auto depth=ReadValue<uintptr_t>(descriptor+0x30);
    auto* dsv=depth?ReadValue<ID3D11DepthStencilView*>(depth+0x108):nullptr;
    Put(backend+0xcf8,uint32_t(1));Put(backend+0xd00,rtv);Put(backend+0xd20,dsv);
    // The actual native binder also updates viewport/scissors; the caller's
    // raster transaction restores the exact pre-capture numeric state later.
    const D3D11_VIEWPORT viewport{0,0,64,32,0,1};const D3D11_RECT scissor{0,0,64,32};
    context->RSSetViewports(1,&viewport);context->RSSetScissorRects(1,&scissor);
    context->OMSetRenderTargets(1,&rtv,dsv);
    return true;
}
static bool ReadSnapshot(uintptr_t base,ID3D11DeviceContext* expected,CeHudTargetSnapshot& out)
{
    return ReadBody(base,expected,out,&SelectNativeSurface);
}
static CeHudTargetRestoreResult RestoreSnapshot(const CeHudTargetSnapshot& saved)
{ return RestoreBody(saved,&SelectNativeSurface,&BindNativeTarget); }
static bool BoundTo(ID3D11RenderTargetView* expected,ID3D11DepthStencilView* expectedDepth)
{
    ID3D11RenderTargetView* actual{};ID3D11DepthStencilView* depth{};
    context->OMGetRenderTargets(1,&actual,&depth);
    const bool result=actual==expected&&depth==expectedDepth;
    if (actual) actual->Release();if (depth) depth->Release();
    return result;
}
int main()
{
    ID3D11Device* device{};D3D_FEATURE_LEVEL feature{};
    if (FAILED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,
        &device,&feature,&context))) return 2;
    ID3D11Texture2D* textures[3]{};ID3D11RenderTargetView* views[3]{};
    D3D11_TEXTURE2D_DESC td{};td.Width=64;td.Height=32;td.ArraySize=1;td.MipLevels=1;
    td.SampleDesc.Count=1;td.Format=DXGI_FORMAT_R8G8B8A8_UNORM;td.BindFlags=D3D11_BIND_RENDER_TARGET;
    for (unsigned i=0;i<3;++i)
        if (FAILED(device->CreateTexture2D(&td,nullptr,&textures[i]))||
            FAILED(device->CreateRenderTargetView(textures[i],nullptr,&views[i]))) return 3;
    ID3D11Texture2D* depthTexture{};ID3D11DepthStencilView* depthView{};
    td.Format=DXGI_FORMAT_D24_UNORM_S8_UINT;td.BindFlags=D3D11_BIND_DEPTH_STENCIL;
    if (FAILED(device->CreateTexture2D(&td,nullptr,&depthTexture))||
        FAILED(device->CreateDepthStencilView(depthTexture,nullptr,&depthView))) return 4;
    std::vector<uint8_t> image(halo_ce::contract::imageSize);
    const uintptr_t base=reinterpret_cast<uintptr_t>(image.data());
    std::vector<uint8_t> storage(0x10000);
    const uintptr_t backend=reinterpret_cast<uintptr_t>(storage.data());
    const uintptr_t color=backend+0x2000,alternate=backend+0x3000,depth=backend+0x4000;
    Put(base+0x2e3bde0,backend);Put(base+0x2ea2d30,context);
    Put(backend,base+0x17f9d10);Put(backend+0xce0,context);
    for (unsigned i=0;i<2;++i)
    {
        const uintptr_t wrapper=i?alternate:color;
        Put(wrapper,base+0x17fb608);Put(wrapper+0x88,uint32_t(1u<<8));
        Put(wrapper+0x1a,int8_t(1));Put(wrapper+0xe0,textures[i]);
        Put(wrapper+0xe8,wrapper+0x200);Put(wrapper+0xf8,wrapper+0x200);
        Put(wrapper+0xf0,int32_t(1));Put(wrapper+0x200,views[i]);
    }
    Put(depth,base+0x17fb608);Put(depth+0x88,uint32_t(1u<<9));
    Put(depth+0xe0,depthTexture);Put(depth+0x108,depthView);Put(depth+0x110,depthView);
    Put(backend+0x18+0x10,color);Put(backend+0x18+0x30,depth);
    Put(backend+0x18+0x38,uint32_t(64));Put(backend+0x18+0x3c,uint32_t(32));
    BindNativeTarget(backend,reinterpret_cast<const void*>(backend+0x18));
    CeHudTargetSnapshot saved{};
    Check(ReadSnapshot(base,context,saved)&&saved.count==1&&saved.rtvs[0]==views[0]&&saved.dsv==depthView,
        "snapshot matches native descriptor-owned color and depth views");
    context->OMSetRenderTargets(1,&views[2],nullptr);
    Check(BoundTo(views[2],nullptr),"private target fixture replaces actual D3D outputs");
    Check(RestoreSnapshot(saved)==CeHudTargetRestoreResult::Unchanged&&BoundTo(views[0],depthView),
        "native replay restores real WARP RTV and DSV without adapter queries or references");
    Check(RestoreSnapshot(saved)==CeHudTargetRestoreResult::Unchanged,
        "repeated native restore still binds identical intent");
    Put(backend+0x18+0x10,alternate);Put(backend+0xd00,views[1]);
    context->OMSetRenderTargets(1,&views[2],nullptr);
    Check(RestoreSnapshot(saved)==CeHudTargetRestoreResult::Changed&&BoundTo(views[1],depthView),
        "native target change restores latest intent and rejects prior capture identity");
    Check(ReadSnapshot(base,context,saved),"new native target can seed another transaction");
    const auto snapshotCopy=saved;
    Put(backend+0xd00,views[2]);
    Check(!ReadSnapshot(base,context,saved)&&saved.descriptor==snapshotCopy.descriptor&&saved.rtvs[0]==snapshotCopy.rtvs[0],
        "cached output unrelated to descriptor is rejected without partial snapshot publication");
    Put(backend+0xd00,views[1]);
    const auto before=bindCalls;
    Put(base+0x2e3bde0,uintptr_t(0));
    Check(RestoreSnapshot(saved)==CeHudTargetRestoreResult::Unavailable&&bindCalls==before,
        "retired native backend is never rebound");
    Put(base+0x2e3bde0,backend);
    Put(backend+0xcf8,uint32_t(5));
    Check(!ReadSnapshot(base,context,saved),"native output count is bounded before reading attachments");
    Put(backend+0xcf8,uint32_t(1));
    Put(backend+0x18+0x40,int16_t(1));
    Check(!ReadSnapshot(base,context,saved),"out-of-range native mip cannot index a view table");
    Put(backend+0x18+0x40,int16_t(0));
    Check(ReadSnapshot(base,context,saved),"valid native state recovers after rejection");
    bindAllowed=false;
    Check(RestoreSnapshot(saved)==CeHudTargetRestoreResult::Unavailable,
        "native bind failure cannot be reported as restored capture");
    bindAllowed=true;
    Check(RestoreSnapshot(saved)==CeHudTargetRestoreResult::Unchanged,
        "subsequent native restore recovers without owning native resources");
    context->OMSetRenderTargets(0,nullptr,nullptr);
    for (unsigned i=0;i<3;++i) { views[i]->Release();textures[i]->Release(); }
    depthView->Release();depthTexture->Release();context->Release();device->Release();
    return failures?1:0;
}
