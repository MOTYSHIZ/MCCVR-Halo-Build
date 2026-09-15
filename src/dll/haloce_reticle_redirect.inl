// Included by vr.cpp and its focused production-body fixture.
// Only CE private crosshair transactions admit these exact native state changes.
static bool CeReticleCaptureActive(ID3D11DeviceContext* context)
{
    const auto& saved=g_reticleCaptureState;
    return saved.active&&saved.cePrivateRaster&&!saved.ceRestoring&&context==g_context;
}
void VR_CeInvalidateAuthoredReticleState(ID3D11DeviceContext* context)
{
    if (CeReticleCaptureActive(context)) g_reticleCaptureState.ceStateValid=false;
}
bool VR_CeRedirectAuthoredReticleTargets(ID3D11DeviceContext* context,UINT count,
    ID3D11RenderTargetView* const* input,ID3D11DepthStencilView* depth,UINT& outputCount,
    ID3D11RenderTargetView** output,ID3D11DepthStencilView*& outputDepth)
{
    if (!CeReticleCaptureActive(context)) return false;
    auto& saved=g_reticleCaptureState;
    auto* capture=saved.publishesAuthored?g_authoredReticleRtv:g_authoredReticleDiscardRtv;
    // Our own private bind is already complete. Native rebinds must match
    // every entry attachment; unfamiliar targets retain their native bind.
    if (count==1&&input&&input[0]==capture&&!depth) return false;
    bool matches=count==saved.ceTarget.count&&count>0&&count<=4&&input&&output&&
        depth==saved.ceTarget.dsv&&capture;
    for (UINT i=0;matches&&i<count;++i) matches=input[i]==saved.ceTarget.rtvs[i];
    if (!matches) { saved.ceStateValid=false; return false; }
    output[0]=capture;outputCount=1;outputDepth=nullptr;
    return true;
}
bool VR_CeRedirectAuthoredReticleViewports(ID3D11DeviceContext* context,UINT count,
    const D3D11_VIEWPORT* input,D3D11_VIEWPORT& output)
{
    if (!CeReticleCaptureActive(context)) return false;
    auto& saved=g_reticleCaptureState;
    if (count==1&&input&&std::memcmp(input,&saved.captureViewport,sizeof(*input))==0) return false;
    if (count!=saved.viewportCount||!input||count!=1||
        std::memcmp(input,saved.viewports,sizeof(*input)*count)!=0)
    { saved.ceStateValid=false;return false; }
    output=saved.captureViewport;return true;
}
bool VR_CeRedirectAuthoredReticleScissors(ID3D11DeviceContext* context,UINT count,
    const D3D11_RECT* input,D3D11_RECT& output)
{
    if (!CeReticleCaptureActive(context)) return false;
    auto& saved=g_reticleCaptureState;
    if (count==1&&input&&std::memcmp(input,&saved.captureScissor,sizeof(*input))==0) return false;
    if (count!=saved.scissorCount||count>D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE||
        (count&&!input)||(count&&std::memcmp(input,saved.scissors,sizeof(*input)*count)!=0))
    { saved.ceStateValid=false;return false; }
    output=saved.captureScissor;return true;
}
