#pragma once
#include "../common/weapon_accessory_logic.h"
#include <d3d11.h>
#include <wrl/client.h>

// Runs only in the VR compositor, after game-eye resolve. Never called by a
// native render/palette hook. Private deferred state preserves ALL game state.
class WeaponAccessoryRenderer
{
public:
    HRESULT Draw(ID3D11Device* device,ID3D11DeviceContext* immediate,
        ID3D11RenderTargetView* target,unsigned width,unsigned height,
        const D3D11_VIEWPORT& viewport,const weapon_accessory::Presentation& item,
        const weapon_accessory::Constants& constants,ID3D11CommandList** staged=nullptr);
    void Reset() noexcept;
private:
    HRESULT Prepare(ID3D11Device* device,unsigned width,unsigned height);
    template<class T> using Ptr=Microsoft::WRL::ComPtr<T>;
    Ptr<ID3D11Device> device_;
    Ptr<ID3D11DeviceContext> context_;
    Ptr<ID3D11VertexShader> vs_;
    Ptr<ID3D11PixelShader> ps_;
    Ptr<ID3D11InputLayout> layout_;
    Ptr<ID3D11Buffer> vertices_,genericVertices_,constants_;
    Ptr<ID3D11RasterizerState> raster_;
    Ptr<ID3D11DepthStencilState> depthState_;
    Ptr<ID3D11DepthStencilView> depth_;
    Ptr<ID3D11ShaderResourceView> surface_;
    Ptr<ID3D11SamplerState> sampler_;
    unsigned width_{},height_{};
    HRESULT prepared_{S_FALSE};
};
