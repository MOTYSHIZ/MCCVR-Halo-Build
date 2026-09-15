// Execute the actual extracted Anniversary mesh-particle and sprite-particle
// vertex shaders. Proprietary fixtures remain external; no game is launched.
#include "../src/common/haloce_first_person_logic.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <cstdio>
#include <fstream>
#include <vector>
using Microsoft::WRL::ComPtr;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"CE particle shader failed at %d: %s\n",__LINE__,#x); return 1; } } while(false)
struct ParticleVertex
{
    float position[4]{.2f,.1f,.3f,1};
    uint32_t indices[4]{0,1,0,0};
    float tangent[4]{0,1,0,0},color[4]{1,1,1,1},parameters[4]{};
    float uv[2]{0,0},life[2]{0,.5f};
};
int main(int argc,char** argv)
{
    CHECK(argc==3);
    const bool sprite=std::strcmp(argv[2],"sprite")==0;
    CHECK(sprite||std::strcmp(argv[2],"mesh")==0);
    std::ifstream file(argv[1],std::ios::binary);
    std::vector<char> shader((std::istreambuf_iterator<char>(file)),{});
    CHECK(shader.size()>32);
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    CHECK(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)));
    ComPtr<ID3D11VertexShader> vs;
    CHECK(SUCCEEDED(device->CreateVertexShader(shader.data(),shader.size(),nullptr,&vs)));
    const char* geometry=R"(
        struct V { float4 position:SV_Position; };
        [maxvertexcount(1)] void main(point V input[1],inout PointStream<V> stream) { stream.Append(input[0]); }
    )";
    ComPtr<ID3DBlob> code,errors;
    CHECK(SUCCEEDED(D3DCompile(geometry,std::strlen(geometry),nullptr,nullptr,nullptr,"main","gs_5_0",0,0,&code,&errors)));
    D3D11_SO_DECLARATION_ENTRY declaration{0,"SV_Position",0,0,4,0};const UINT outStride=16;
    ComPtr<ID3D11GeometryShader> gs;
    CHECK(SUCCEEDED(device->CreateGeometryShaderWithStreamOutput(code->GetBufferPointer(),code->GetBufferSize(),&declaration,1,&outStride,1,D3D11_SO_NO_RASTERIZED_STREAM,nullptr,&gs)));
    const D3D11_INPUT_ELEMENT_DESC inputs[]={
        {"POSITION",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,offsetof(ParticleVertex,position),D3D11_INPUT_PER_VERTEX_DATA,0},
        {"BLENDINDICES",0,DXGI_FORMAT_R32G32B32A32_UINT,0,offsetof(ParticleVertex,indices),D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TANGENT",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,offsetof(ParticleVertex,tangent),D3D11_INPUT_PER_VERTEX_DATA,0},
        {"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,offsetof(ParticleVertex,color),D3D11_INPUT_PER_VERTEX_DATA,0},
        {"COLOR",1,DXGI_FORMAT_R32G32B32A32_FLOAT,0,offsetof(ParticleVertex,parameters),D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,offsetof(ParticleVertex,uv),D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",1,DXGI_FORMAT_R32G32_FLOAT,0,offsetof(ParticleVertex,life),D3D11_INPUT_PER_VERTEX_DATA,0}};
    ComPtr<ID3D11InputLayout> layout;
    CHECK(SUCCEEDED(device->CreateInputLayout(inputs,UINT(std::size(inputs)),shader.data(),shader.size(),&layout)));
    auto buffer=[&](UINT size,UINT flags,ComPtr<ID3D11Buffer>& result)
    {
        D3D11_BUFFER_DESC desc{};desc.ByteWidth=size;desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=flags;
        return SUCCEEDED(device->CreateBuffer(&desc,nullptr,&result));
    };
    ComPtr<ID3D11Buffer> vertices,output,readback,commonBuffer,globalsBuffer,particlesBuffer;
    CHECK(buffer(sizeof(ParticleVertex),D3D11_BIND_VERTEX_BUFFER,vertices));
    CHECK(buffer(16,D3D11_BIND_STREAM_OUTPUT,output));
    CHECK(buffer(1440,D3D11_BIND_CONSTANT_BUFFER,commonBuffer));
    CHECK(buffer(64,D3D11_BIND_CONSTANT_BUFFER,globalsBuffer));
    CHECK(buffer(201*16,D3D11_BIND_CONSTANT_BUFFER,particlesBuffer));
    D3D11_BUFFER_DESC desc{};output->GetDesc(&desc);desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    CHECK(SUCCEEDED(device->CreateBuffer(&desc,nullptr,&readback)));
    const UINT stride=sizeof(ParticleVertex),zero=0;ID3D11Buffer* vertex=vertices.Get();
    context->IASetVertexBuffers(0,1,&vertex,&stride,&zero);context->IASetInputLayout(layout.Get());
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
    context->VSSetShader(vs.Get(),nullptr,0);context->GSSetShader(gs.Get(),nullptr,0);
    ID3D11Buffer* buffers[8]{commonBuffer.Get(),globalsBuffer.Get(),nullptr,nullptr,nullptr,nullptr,nullptr,particlesBuffer.Get()};
    context->VSSetConstantBuffers(0,8,buffers);
    unsigned draws=0;
    for (float scale:{.3f,1.0f,3.0f}) for (unsigned eye=0;eye<2;++eye)
        for (unsigned emitter:{0u,4u,8u}) for (bool firstPerson:{false,true}) for (bool corrected:{false,true})
    {
        ParticleVertex input{};input.indices[2]=emitter;
        float particles[804]{},common[360]{},globals[16]{};
        const float world[16]{1.2f,0,eye?.18f:-.21f,0, 0,1.4f,.09f,0, 0,0,.7f,.2f, 0,0,1,0};
        const float fixed[16]{2,0,0,0, 0,2.1f,0,0, 0,0,.8f,.2f, 0,0,1,0};
        std::memcpy(particles,world,sizeof(world));std::memcpy(particles+16,fixed,sizeof(fixed));
        std::memcpy(common+156,world,sizeof(world));std::memcpy(common+172,fixed,sizeof(fixed));
        // Same authored muzzle point in a translated/rotated/scaled carrier;
        // sprite emitters already contain the native camera-relative transform.
        const float yaw=.4f+float(emitter)*.1f,c=std::cos(yaw)*scale,s=std::sin(yaw)*scale;
        const float translation[3]{float(emitter)*.11f-.3f,.2f,2.0f};
        const float transform[12]{c,-s,0,translation[0], s,c,0,translation[1], 0,0,scale,translation[2]};
        std::memcpy(common+224,transform,sizeof(transform));
        const size_t base=(21+emitter*20)*4;
        std::memcpy(particles+base+12,transform,sizeof(transform));
        for (size_t row=0;row<3;++row) particles[base+row*4+row]=1;
        particles[base+40]=firstPerson?1.0f:0.0f;
        particles[base+13*4]=1.0f/64;particles[base+13*4+1]=1;particles[base+13*4+2]=1;
        particles[base+14*4+2]=particles[base+14*4+3]=1;
        for (size_t row:{16u,18u}) for (unsigned lane=0;lane<4;++lane) particles[base+row*4+lane]=float(lane);
        for (size_t row:{17u,19u}) for (unsigned lane=0;lane<4;++lane) particles[base+row*4+lane]=1;
        const auto nativeParticles=std::to_array(particles);
        unsigned changed{};
        if (corrected) CHECK(halo_ce::SelectSaberTrackedParticleProjection(particles,201,changed));
        CHECK(!corrected||changed==unsigned(firstPerson));
        // Model-particle shader uses the same selector with EMITTER_OFFSET.
        const int offset=int(emitter*20);std::memcpy(globals+12,&offset,sizeof(offset));
        context->UpdateSubresource(vertices.Get(),0,nullptr,&input,0,0);
        context->UpdateSubresource(commonBuffer.Get(),0,nullptr,common,0,0);
        context->UpdateSubresource(globalsBuffer.Get(),0,nullptr,globals,0,0);
        context->UpdateSubresource(particlesBuffer.Get(),0,nullptr,particles,0,0);
        ID3D11Buffer* stream=output.Get();context->SOSetTargets(1,&stream,&zero);context->Draw(1,0);context->SOSetTargets(0,nullptr,nullptr);
        context->CopyResource(readback.Get(),output.Get());D3D11_MAPPED_SUBRESOURCE mapped{};
        CHECK(SUCCEEDED(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&mapped)));
        const auto* actual=static_cast<const float*>(mapped.pData);
        float point[4]{0,0,0,1};
        for (unsigned row=0;row<3;++row) for (unsigned lane=0;lane<4;++lane) point[row]+=transform[row*4+lane]*input.position[lane];
        const auto* projection=firstPerson&&!corrected?fixed:world;
        for (unsigned row=0;row<4;++row)
        {
            float expected=0;for (unsigned lane=0;lane<4;++lane) expected+=projection[row*4+lane]*point[lane];
            if (!std::isfinite(actual[row])||std::fabs(actual[row]-expected)>.003f)
                std::fprintf(stderr,"%s scale=%g eye=%u emitter=%u FP=%d corrected=%d row=%u actual=%g expected=%g\n",argv[2],scale,eye,emitter,firstPerson,corrected,row,actual[row],expected);
            CHECK(std::isfinite(actual[row])&&std::fabs(actual[row]-expected)<.003f);
        }
        context->Unmap(readback.Get(),0);++draws;
        for (size_t index=0;index<std::size(particles);++index)
            CHECK(particles[index]==(corrected&&index==base+40?0.0f:nativeParticles[index]));
    }
    std::printf("PASS native CE %s particles: %u WARP draws, both eyes, three scales, mixed emitter ownership and stock-vs-tracked lens\n",argv[2],draws);
    return 0;
}
