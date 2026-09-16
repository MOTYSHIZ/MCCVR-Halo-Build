#include "weapon_accessory_renderer.h"
#include "../common/weapon_magazines.generated.h"
#include "../common/weapon_generic_magazine.h"
#include <d3dcompiler.h>
#include <cstring>
#include <iterator>

namespace
{
constexpr char shader[]=R"(
cbuffer Transform : register(b0) { float4 objectQ,objectP,eyeQ,eyeP,tangents,color; };
float3 rotate(float4 q,float3 p) { return p+2*cross(q.xyz,cross(q.xyz,p)+q.w*p); }
struct V { float3 p:POSITION; float3 n:NORMAL; };
struct O { float4 p:SV_Position; float3 n:NORMAL; };
O vs_main(V v) {
    O o;
    float3 p=rotate(eyeQ,rotate(objectQ,v.p)+objectP.xyz-eyeP.xyz);
    float z=-p.z;
    o.p=float4((2*p.x-(tangents.y+tangents.x)*z)/(tangents.y-tangents.x),
        (2*p.y-(tangents.w+tangents.z)*z)/(tangents.w-tangents.z),
        (20.0*z-0.02*20.0)/(20.0-0.02),z);
    o.n=rotate(objectQ,v.n);return o;
}
float4 ps_main(O i):SV_Target {
    float light=0.35+0.65*abs(dot(normalize(i.n),normalize(float3(-0.3,0.8,0.5))));
    return float4(color.xyz*light,1);
}
)";
}
void WeaponAccessoryRenderer::Reset() noexcept
{
    if(context_) context_->ClearState();
    depth_.Reset();depthState_.Reset();raster_.Reset();constants_.Reset();vertices_.Reset();genericVertices_.Reset();
    layout_.Reset();ps_.Reset();vs_.Reset();context_.Reset();device_.Reset();
    width_=height_=0;prepared_=S_FALSE;
}
HRESULT WeaponAccessoryRenderer::Prepare(ID3D11Device* device,unsigned width,unsigned height)
{
    if(!device||!width||!height||width>16384||height>16384) return E_INVALIDARG;
    if(device_.Get()!=device) { Reset();device_=device; }
    if(FAILED(prepared_)) return prepared_; // no per-frame shader compilation retries
    if(prepared_==S_FALSE)
    {
        HRESULT hr=device->CreateDeferredContext(0,&context_);
        Ptr<ID3DBlob> vs,ps,error;
        if(SUCCEEDED(hr)) hr=D3DCompile(shader,sizeof(shader)-1,nullptr,nullptr,nullptr,
            "vs_main","vs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&vs,&error);
        error.Reset();
        if(SUCCEEDED(hr)) hr=D3DCompile(shader,sizeof(shader)-1,nullptr,nullptr,nullptr,
            "ps_main","ps_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&ps,&error);
        if(SUCCEEDED(hr)) hr=device->CreateVertexShader(vs->GetBufferPointer(),vs->GetBufferSize(),nullptr,&vs_);
        if(SUCCEEDED(hr)) hr=device->CreatePixelShader(ps->GetBufferPointer(),ps->GetBufferSize(),nullptr,&ps_);
        const D3D11_INPUT_ELEMENT_DESC elements[]{
            {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
            {"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0}};
        if(SUCCEEDED(hr)) hr=device->CreateInputLayout(elements,2,vs->GetBufferPointer(),vs->GetBufferSize(),&layout_);
        D3D11_BUFFER_DESC buffer{};buffer.ByteWidth=sizeof(weapon_model::kVertices);
        buffer.Usage=D3D11_USAGE_IMMUTABLE;buffer.BindFlags=D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA data{weapon_model::kVertices};
        if(SUCCEEDED(hr)) hr=device->CreateBuffer(&buffer,&data,&vertices_);
        buffer.ByteWidth=sizeof(weapon_model::kGenericVertices);data.pSysMem=weapon_model::kGenericVertices;
        if(SUCCEEDED(hr)) hr=device->CreateBuffer(&buffer,&data,&genericVertices_);
        buffer.ByteWidth=sizeof(weapon_accessory::Constants);buffer.Usage=D3D11_USAGE_DYNAMIC;
        buffer.BindFlags=D3D11_BIND_CONSTANT_BUFFER;buffer.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
        if(SUCCEEDED(hr)) hr=device->CreateBuffer(&buffer,nullptr,&constants_);
        D3D11_RASTERIZER_DESC raster{};raster.FillMode=D3D11_FILL_SOLID;
        raster.CullMode=D3D11_CULL_NONE;raster.DepthClipEnable=TRUE;
        if(SUCCEEDED(hr)) hr=device->CreateRasterizerState(&raster,&raster_);
        D3D11_DEPTH_STENCIL_DESC depth{};depth.DepthEnable=TRUE;
        depth.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ALL;depth.DepthFunc=D3D11_COMPARISON_LESS;
        if(SUCCEEDED(hr)) hr=device->CreateDepthStencilState(&depth,&depthState_);
        prepared_=hr;
        if(FAILED(hr)) return hr;
    }
    if(width_!=width||height_!=height||!depth_)
    {
        depth_.Reset();width_=height_=0;
        D3D11_TEXTURE2D_DESC desc{};desc.Width=width;desc.Height=height;
        desc.ArraySize=desc.MipLevels=1;desc.SampleDesc.Count=1;
        desc.Format=DXGI_FORMAT_D32_FLOAT;desc.BindFlags=D3D11_BIND_DEPTH_STENCIL;
        Ptr<ID3D11Texture2D> texture;
        HRESULT hr=device->CreateTexture2D(&desc,nullptr,&texture);
        if(SUCCEEDED(hr)) hr=device->CreateDepthStencilView(texture.Get(),nullptr,&depth_);
        if(FAILED(hr)) { prepared_=hr;return hr; }
        width_=width;height_=height;
    }
    return S_OK;
}
HRESULT WeaponAccessoryRenderer::Draw(ID3D11Device* device,ID3D11DeviceContext* immediate,
    ID3D11RenderTargetView* target,unsigned width,unsigned height,const D3D11_VIEWPORT& viewport,
    const weapon_accessory::Presentation& item,const weapon_accessory::Constants& constants,ID3D11CommandList** staged)
{
    if(staged) *staged=nullptr;
    const auto* model=item.model;
    const bool generic=model==&weapon_model::kGenericReloadModel;
    const size_t available=generic?std::size(weapon_model::kGenericVertices):std::size(weapon_model::kVertices);
    if(!immediate||!target||!model||!model->vertexCount||
        model->firstVertex>available||
        model->vertexCount>available-model->firstVertex||
        model->vertexCount%3) return E_INVALIDARG;
    HRESULT hr=Prepare(device,width,height);
    if(FAILED(hr)) return hr;
    D3D11_MAPPED_SUBRESOURCE mapped{};
    hr=context_->Map(constants_.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped);
    if(FAILED(hr)) return hr;
    std::memcpy(mapped.pData,&constants,sizeof(constants));context_->Unmap(constants_.Get(),0);
    context_->OMSetRenderTargets(1,&target,depth_.Get());
    context_->ClearDepthStencilView(depth_.Get(),D3D11_CLEAR_DEPTH,1,0);
    context_->OMSetDepthStencilState(depthState_.Get(),0);
    context_->OMSetBlendState(nullptr,nullptr,0xFFFFFFFF);
    context_->RSSetState(raster_.Get());context_->RSSetViewports(1,&viewport);
    context_->IASetInputLayout(layout_.Get());
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    const UINT stride=sizeof(weapon_model::Vertex),offset=0;
    ID3D11Buffer* vertex=generic?genericVertices_.Get():vertices_.Get();ID3D11Buffer* cb=constants_.Get();
    context_->IASetVertexBuffers(0,1,&vertex,&stride,&offset);
    context_->VSSetConstantBuffers(0,1,&cb);
    context_->PSSetConstantBuffers(0,1,&cb);
    context_->VSSetShader(vs_.Get(),nullptr,0);context_->PSSetShader(ps_.Get(),nullptr,0);
    context_->Draw(model->vertexCount,model->firstVertex);
    Ptr<ID3D11CommandList> commands;
    hr=context_->FinishCommandList(FALSE,&commands);
    if(SUCCEEDED(hr))
    {
        if(staged) *staged=commands.Detach();
        else immediate->ExecuteCommandList(commands.Get(),TRUE);
    }
    else context_->ClearState();
    return hr;
}
