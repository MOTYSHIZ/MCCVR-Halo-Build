// Execute the production CE redirection decisions with a real D3D11 context.
// The native descriptor is explicit fixture data; this is not game-pixel proof.
#include "../src/dll/haloce_hud_target.h"
#include <cstdio>
#include <cstring>
static ID3D11DeviceContext* g_context{};
static ID3D11RenderTargetView* g_authoredReticleRtv{},*g_authoredReticleDiscardRtv{};
struct FixtureCaptureState
{
    bool active{},cePrivateRaster{},ceRestoring{},ceStateValid{},publishesAuthored{};
    CeHudTargetSnapshot ceTarget;
    UINT viewportCount{},scissorCount{};
    D3D11_VIEWPORT viewports[16]{},captureViewport{};
    D3D11_RECT scissors[16]{},captureScissor{};
} g_reticleCaptureState;
#include "../src/dll/haloce_reticle_redirect.inl"

int main()
{
    unsigned failures=0;
    const auto check=[&](bool ok,const char* message) {
        if (!ok) { ++failures;std::fprintf(stderr,"CE reticle redirect: %s\n",message); }
    };
    ID3D11Device* device{};D3D_FEATURE_LEVEL feature{};
    if (FAILED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,
        D3D11_SDK_VERSION,&device,&feature,&g_context))) return 2;
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width=desc.Height=64;desc.MipLevels=desc.ArraySize=1;
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;
    desc.BindFlags=D3D11_BIND_RENDER_TARGET;
    ID3D11Texture2D* textures[3]{};ID3D11RenderTargetView* views[3]{};
    for (unsigned i=0;i<3;++i)
        if (FAILED(device->CreateTexture2D(&desc,nullptr,&textures[i]))||
            FAILED(device->CreateRenderTargetView(textures[i],nullptr,&views[i]))) return 2;
    g_authoredReticleRtv=views[1];g_authoredReticleDiscardRtv=views[2];
    auto& saved=g_reticleCaptureState;
    saved.ceTarget.count=1;saved.ceTarget.rtvs[0]=views[0];
    saved.active=saved.cePrivateRaster=saved.ceStateValid=saved.publishesAuthored=true;
    UINT count=99;ID3D11RenderTargetView* out[8]{};ID3D11DepthStencilView* depth{};
    check(VR_CeRedirectAuthoredReticleTargets(g_context,1,views,nullptr,count,out,depth)&&
        count==1&&out[0]==views[1]&&!depth,"exact native target is replaced with private authored target");
    g_context->OMSetRenderTargets(count,out,depth);
    ID3D11RenderTargetView* actual{};g_context->OMGetRenderTargets(1,&actual,nullptr);
    check(actual==views[1],"native rebind interception actually binds the private WARP target");
    if (actual) actual->Release();
    saved.publishesAuthored=false;
    check(VR_CeRedirectAuthoredReticleTargets(g_context,1,views,nullptr,count,out,depth)&&out[0]==views[2],
        "opposite phase selects the separate discard target");
    check(!VR_CeRedirectAuthoredReticleTargets(g_context,1,&views[2],nullptr,count,out,depth)&&saved.ceStateValid,
        "our own private bind passes through without invalidating its scope");
    saved.ceTarget.count=2;saved.ceTarget.rtvs[1]=views[1];
    saved.ceTarget.dsv=reinterpret_cast<ID3D11DepthStencilView*>(uintptr_t(0x12340));
    check(VR_CeRedirectAuthoredReticleTargets(g_context,2,views,saved.ceTarget.dsv,count,out,depth)&&
        count==1&&!depth,"every matching native MRT/depth identity is replaced by one depth-free capture target");
    saved.ceRestoring=true;
    check(!VR_CeRedirectAuthoredReticleTargets(g_context,2,views,saved.ceTarget.dsv,count,out,depth)&&saved.ceStateValid,
        "native restoration cannot be redirected back into capture");
    saved.ceRestoring=false;
    check(!VR_CeRedirectAuthoredReticleTargets(g_context,2,views,nullptr,count,out,depth)&&!saved.ceStateValid,
        "unfamiliar native depth remains native and invalidates only the current art");
    saved.ceStateValid=true;
    check(!VR_CeRedirectAuthoredReticleTargets(nullptr,2,views,nullptr,count,out,depth)&&saved.ceStateValid,
        "another context does not mutate CE capture ownership");
    saved.viewportCount=1;saved.viewports[0]={0,0,1920,1080,0,1};
    saved.captureViewport={-928,-508,1920,1080,0,1};
    D3D11_VIEWPORT viewport{};
    check(VR_CeRedirectAuthoredReticleViewports(g_context,1,saved.viewports,viewport)&&
        std::memcmp(&viewport,&saved.captureViewport,sizeof(viewport))==0,
        "native viewport rebind preserves the exact capture crop");
    check(!VR_CeRedirectAuthoredReticleViewports(g_context,1,&viewport,viewport)&&saved.ceStateValid,
        "private framing is not transformed twice");
    viewport.Width=32;
    check(!VR_CeRedirectAuthoredReticleViewports(g_context,1,&viewport,viewport)&&!saved.ceStateValid,
        "unproven viewport changes cannot publish stretched art");
    saved.ceStateValid=true;saved.scissorCount=1;
    saved.scissors[0]={0,0,1920,1080};saved.captureScissor={0,0,64,64};
    D3D11_RECT scissor{};
    check(VR_CeRedirectAuthoredReticleScissors(g_context,1,saved.scissors,scissor)&&scissor.right==64,
        "exact native scissor follows the private target bounds");
    check(!VR_CeRedirectAuthoredReticleScissors(g_context,17,nullptr,scissor)&&!saved.ceStateValid,
        "oversized raster arrays reject before reading their pointers");
    saved.ceStateValid=true;VR_CeInvalidateAuthoredReticleState(g_context);
    check(!saved.ceStateValid,"command-list or context-state replacement revokes current art");
    saved.cePrivateRaster=false;saved.ceStateValid=true;
    VR_CeInvalidateAuthoredReticleState(g_context);
    check(saved.ceStateValid&&!VR_CeRedirectAuthoredReticleTargets(g_context,1,views,nullptr,count,out,depth),
        "legacy title capture is untouched by every CE state policy");
    g_context->ClearState();
    for (unsigned i=0;i<3;++i) { views[i]->Release();textures[i]->Release(); }
    g_context->Release();device->Release();
    return failures?1:0;
}
