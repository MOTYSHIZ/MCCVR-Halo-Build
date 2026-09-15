// Native CE writes only RGB. Feed that exact write-mask descriptor into WARP,
// then execute production Blit and its production shader. Artwork is synthetic;
// this verifies transport/alpha and does not claim native weapon-tag coverage.
#include "../src/dll/vr.h"
#include "../src/dll/d3d_state.h"
#include "../src/dll/title_adapter.h"
#include "../src/common/haloce_reticle_logic.h"
#include "../src/common/vr_blit_shader.h"
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;
namespace
{
unsigned failures{};
ID3D11Device* g_device{};
ID3D11DeviceContext* g_context{};
ID3D11VertexShader* g_blitVs{};
ID3D11PixelShader* g_blitPsLinearize{},*g_blitPsPass{};
ID3D11SamplerState* g_blitSampler{};
ID3D11RasterizerState* g_blitRasterizer{};
ID3D11DepthStencilState* g_blitDepthOff{};
DXGI_FORMAT g_xrFormat=DXGI_FORMAT_R8G8B8A8_UNORM;
GameTitle title=GameTitle::HaloCE;
bool failPipeline{},failView{};
std::vector<ComPtr<ID3D11ShaderResourceView>> sourceViews;
void Check(bool value,const char* why)
{ if (!value) { ++failures;std::fprintf(stderr,"CE reticle pixels: %s\n",why); } }
ComPtr<ID3DBlob> Compile(const char* source,const char* entry,const char* target)
{
    ComPtr<ID3DBlob> code,error;
    const HRESULT hr=D3DCompile(source,std::strlen(source),nullptr,nullptr,nullptr,
        entry,target,0,0,&code,&error);
    Check(SUCCEEDED(hr),entry);
    if (FAILED(hr)&&error) std::fprintf(stderr,"%s\n",static_cast<const char*>(error->GetBufferPointer()));
    return code;
}
bool IsSrgb(DXGI_FORMAT f) { return f==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; }
bool EnsureBlitPipeline()
{
    if (failPipeline) return false;
    if (g_blitVs) return true;
    auto vs=Compile(kVrBlitShader,"vs_main","vs_5_0");
    auto linear=Compile(kVrBlitShader,"ps_linearize","ps_5_0");
    auto pass=Compile(kVrBlitShader,"ps_pass","ps_5_0");
    if (!vs||!linear||!pass) return false;
    if (FAILED(g_device->CreateVertexShader(vs->GetBufferPointer(),vs->GetBufferSize(),nullptr,&g_blitVs))||
        FAILED(g_device->CreatePixelShader(linear->GetBufferPointer(),linear->GetBufferSize(),nullptr,&g_blitPsLinearize))||
        FAILED(g_device->CreatePixelShader(pass->GetBufferPointer(),pass->GetBufferSize(),nullptr,&g_blitPsPass))) return false;
    D3D11_SAMPLER_DESC sampler{};sampler.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;
    D3D11_RASTERIZER_DESC raster{};raster.FillMode=D3D11_FILL_SOLID;raster.CullMode=D3D11_CULL_NONE;
    raster.DepthClipEnable=TRUE;
    D3D11_DEPTH_STENCIL_DESC depth{};
    return SUCCEEDED(g_device->CreateSamplerState(&sampler,&g_blitSampler))&&
        SUCCEEDED(g_device->CreateRasterizerState(&raster,&g_blitRasterizer))&&
        SUCCEEDED(g_device->CreateDepthStencilState(&depth,&g_blitDepthOff));
}
ID3D11ShaderResourceView* AcquireSrcSrv(ID3D11Texture2D* source,const D3D11_TEXTURE2D_DESC&)
{
    if (failView) return nullptr;
    ComPtr<ID3D11ShaderResourceView> view;
    if (FAILED(g_device->CreateShaderResourceView(source,nullptr,&view))) return nullptr;
    sourceViews.push_back(view);return view.Get();
}
}
GameTitle TitleAdapter_GetActiveTitle() { return title; }
#define LOG(...) ((void)0)
#include "../src/dll/vr_blit.inl"
#include "../src/dll/vr_swapchain_rtv.inl"
#undef LOG

namespace
{
std::vector<uint8_t> Pixels(ID3D11Texture2D* texture,UINT subresource=0)
{
    D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);
    desc.Width=std::max(1u,desc.Width>>subresource);desc.Height=std::max(1u,desc.Height>>subresource);
    desc.MipLevels=1;desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;
    desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;
    ComPtr<ID3D11Texture2D> stage;
    Check(SUCCEEDED(g_device->CreateTexture2D(&desc,nullptr,&stage)),"staging texture");
    g_context->CopySubresourceRegion(stage.Get(),0,0,0,0,texture,subresource,nullptr);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    Check(SUCCEEDED(g_context->Map(stage.Get(),0,D3D11_MAP_READ,0,&mapped)),"readback");
    std::vector<uint8_t> result(size_t(desc.Width)*desc.Height*4);
    for (UINT y=0;y<desc.Height;++y)
        std::memcpy(result.data()+size_t(y)*desc.Width*4,
            static_cast<const uint8_t*>(mapped.pData)+size_t(y)*mapped.RowPitch,size_t(desc.Width)*4);
    g_context->Unmap(stage.Get(),0);return result;
}
}
int main(int argc,char** argv)
{
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    if (FAILED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,
        D3D11_SDK_VERSION,&device,nullptr,&context))) return 2;
    g_device=device.Get();g_context=context.Get();
    if (!EnsureBlitPipeline()) return 2;
    D3D11_BLEND_DESC native{};
    native.RenderTarget[0].SrcBlend=native.RenderTarget[0].SrcBlendAlpha=D3D11_BLEND_ONE;
    native.RenderTarget[0].DestBlend=native.RenderTarget[0].DestBlendAlpha=D3D11_BLEND_ZERO;
    native.RenderTarget[0].BlendOp=native.RenderTarget[0].BlendOpAlpha=D3D11_BLEND_OP_ADD;
    native.RenderTarget[0].RenderTargetWriteMask=7;
    if (argc==2)
    {
        std::ifstream input(argv[1],std::ios::binary);
        input.read(reinterpret_cast<char*>(&native),sizeof(native));
        Check(input.gcount()==sizeof(native)&&native.RenderTarget[0].RenderTargetWriteMask==7,
            "pinned native descriptor exported by instruction emulator");
    }
    ComPtr<ID3D11BlendState> blend;
    Check(SUCCEEDED(g_device->CreateBlendState(&native,&blend)),"native CE blend state is valid D3D11");
    D3D11_TEXTURE2D_DESC desc{};desc.Width=desc.Height=512;desc.MipLevels=10;desc.ArraySize=1;
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;
    desc.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;desc.MiscFlags=D3D11_RESOURCE_MISC_GENERATE_MIPS;
    ComPtr<ID3D11Texture2D> capture;
    ComPtr<ID3D11RenderTargetView> captureRtv;
    Check(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&capture))&&
        SUCCEEDED(device->CreateRenderTargetView(capture.Get(),nullptr,&captureRtv)),"capture resources");
    const std::string nativeShader=std::string(kVrBlitShader)+R"(
float4 ps_native(VSOut i) : SV_Target {
    float2 p=i.uv*2-1;
    float coverage=saturate((0.72-length(p))*64)*saturate((length(p)-0.62)*64);
    float3 color=p.x<0?float3(0.12,0.45,0.8):float3(0.85,0.18,0.05);
    return float4(color*coverage,coverage);
})";
    auto code=Compile(nativeShader.c_str(),"ps_native","ps_5_0");
    ComPtr<ID3D11PixelShader> nativePs;
    Check(SUCCEEDED(device->CreatePixelShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&nativePs)),"native fixture pixel shader");
    const float zero[4]{};g_context->ClearRenderTargetView(captureRtv.Get(),zero);
    ID3D11RenderTargetView* target=captureRtv.Get();g_context->OMSetRenderTargets(1,&target,nullptr);
    D3D11_VIEWPORT viewport{0,0,512,512,0,1};g_context->RSSetViewports(1,&viewport);
    g_context->RSSetState(g_blitRasterizer);g_context->OMSetDepthStencilState(g_blitDepthOff,0);
    g_context->OMSetBlendState(blend.Get(),nullptr,0xffffffff);
    g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    g_context->VSSetShader(g_blitVs,nullptr,0);g_context->PSSetShader(nativePs.Get(),nullptr,0);
    g_context->Draw(3,0);g_context->OMSetRenderTargets(0,nullptr,nullptr);
    auto* captureSrv=AcquireSrcSrv(capture.Get(),desc);g_context->GenerateMips(captureSrv);
    const auto source=Pixels(capture.Get());const auto probe=Pixels(capture.Get(),6);
    uint32_t alpha=0,rgb=0;
    for (size_t i=0;i<probe.size();i+=4) { alpha+=probe[i+3];rgb+=probe[i]+probe[i+1]+probe[i+2]; }
    Check(alpha==0&&rgb>2,"actual RGB-only WARP draw produces visible art with zero coverage under the old alpha metric");
    Check(halo_ce::ReticleVisibleInk(alpha,rgb)>2,"CE accepts the native RGB artwork");
    Check(!halo_ce::ReticleVisibleInk(0,0),"fully blank captures remain rejected");
    auto outputDesc=desc;outputDesc.MipLevels=1;outputDesc.MiscFlags=0;
    ComPtr<ID3D11Texture2D> output;ComPtr<ID3D11RenderTargetView> outputRtv;
    Check(SUCCEEDED(device->CreateTexture2D(&outputDesc,nullptr,&output))&&
        SUCCEEDED(device->CreateRenderTargetView(output.Get(),nullptr,&outputRtv)),"output resources");
    // Production fast copy needs matching mip counts, as in its real good-art texture.
    ComPtr<ID3D11Texture2D> good;
    Check(SUCCEEDED(device->CreateTexture2D(&outputDesc,nullptr,&good)),"known-good source");
    g_context->CopySubresourceRegion(good.Get(),0,0,0,0,capture.Get(),0,nullptr);
    Check(Blit(good.Get(),outputDesc,output.Get(),512,512,outputRtv.Get(),false),"old CE path executes");
    Check(Pixels(output.Get())==source,"old fast-copy path preserves invisible zero-alpha art");
    Check(Blit(good.Get(),outputDesc,output.Get(),512,512,outputRtv.Get(),true),"CE authored upload executes production shader repair");
    const auto repaired=Pixels(output.Get());
    unsigned visible=0,antialias=0;
    for (size_t i=0;i<source.size();i+=4)
    {
        const unsigned maximum=std::max({source[i],source[i+1],source[i+2]});
        if (!maximum) { Check(repaired[i+3]==0,"transparent background stays transparent");continue; }
        ++visible;if (maximum<64) ++antialias;
        Check(std::abs(int(repaired[i+3])-int(maximum))<=1,"native color intensity becomes coverage alpha");
        for (unsigned c=0;c<3;++c)
            Check(std::abs(int(repaired[i+c])*int(repaired[i+3])/255-int(source[i+c]))<=2,
                "straight-alpha pixels reconstruct native RGB including antialiased edges");
    }
    Check(visible>1000&&antialias>100,"fixture exercises hollow art and antialiased fragments");
    ComPtr<ID3D11BlendState> restored;g_context->OMGetBlendState(&restored,nullptr,nullptr);
    Check(restored.Get()==blend.Get(),"production shader upload restores native blend state");
    // SteamVR can expose a TYPELESS image for the negotiated SRGB format.
    // Execute production GetRtv's typed retry and Blit's gamma branch together.
    auto xrDesc=outputDesc;xrDesc.Format=DXGI_FORMAT_R8G8B8A8_TYPELESS;
    ComPtr<ID3D11Texture2D> xrOutput;
    Check(SUCCEEDED(device->CreateTexture2D(&xrDesc,nullptr,&xrOutput)),"typeless XR image");
    std::vector<ID3D11Texture2D*> xrImages{xrOutput.Get()};
    std::vector<ID3D11RenderTargetView*> xrViews(1,nullptr);
    g_xrFormat=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    auto* typed=GetRtv(xrImages,xrViews,0,true);
    D3D11_RENDER_TARGET_VIEW_DESC typedDesc{};if (typed) typed->GetDesc(&typedDesc);
    Check(typed&&typedDesc.Format==g_xrFormat,"production typed fallback selects the negotiated SRGB view");
    Check(Blit(good.Get(),outputDesc,xrOutput.Get(),512,512,typed,true),"CE RGB art takes production linearize/SRGB upload");
    const auto srgb=Pixels(xrOutput.Get());
    for (size_t i=0;i<srgb.size();++i)
        Check(std::abs(int(srgb[i])-int(repaired[i]))<=2,"SRGB conversion preserves native color and repaired coverage");
    if (typed) typed->Release();
    g_xrFormat=DXGI_FORMAT_R8G8B8A8_UNORM;
    failPipeline=true;
    Check(!Blit(good.Get(),outputDesc,output.Get(),512,512,outputRtv.Get(),true),"repair failure does not report successful upload");
    failPipeline=false;failView=true;
    Check(!Blit(good.Get(),outputDesc,output.Get(),512,512,outputRtv.Get(),true),"source-view failure remains local to art upload");
    failView=false;
    title=GameTitle::Halo3;
    Check(Blit(good.Get(),outputDesc,output.Get(),512,512,nullptr,true)&&Pixels(output.Get())==source,
        "Halo 3 retains its direct-copy route and exact pixels");
    title=GameTitle::Halo4;
    Check(Blit(good.Get(),outputDesc,output.Get(),512,512,outputRtv.Get(),true)&&Pixels(output.Get())==repaired,
        "Halo 4 retains its existing repaired native-art route");
    title=GameTitle::HaloCE;
    Check(!halo_ce::ReticleUploadNeedsAlphaRepair(true,false,512,512,512,512)&&
        !halo_ce::ReticleUploadNeedsAlphaRepair(true,true,1024,512,512,512)&&
        !halo_ce::ReticleUploadNeedsAlphaRepair(false,true,512,512,512,512),
        "only exact CE authored uploads admit the new repair");
    g_context->ClearState();sourceViews.clear();
    g_blitDepthOff->Release();g_blitRasterizer->Release();g_blitSampler->Release();
    g_blitPsPass->Release();g_blitPsLinearize->Release();g_blitVs->Release();
    if (!failures) std::printf("PASS_CE_NATIVE_RGB_UPLOAD: alpha=%u rgb=%u visible=%u antialias=%u\n",alpha,rgb,visible,antialias);
    return failures?1:0;
}
