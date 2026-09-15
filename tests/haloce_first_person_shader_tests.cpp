// Executes a pinned, locally extracted native CE Anniversary vertex shader.
// The proprietary bytecode is an explicit external fixture, never distributed.
// This proves the bone/projection consumer; native draw selection still needs
// the game's renderer and headset validation.
#include "../src/common/haloce_first_person_logic.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <cstdio>
#include <fstream>
#include <vector>

using Microsoft::WRL::ComPtr;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"Native CE shader check failed at %d: %s\n",__LINE__,#x); return 1; } } while(false)
namespace
{
struct Vertex
{
    int32_t position[4]{};
    float weights[4]{1,0,0,0};
    uint32_t indices[4]{};
    float tangent[4]{1,.5f,.5f,1},bitangent[4]{.5f,1,.5f,1};
    float color[4]{1,1,1,1},uv[2]{},uv1[2]{};
};
struct Output { float clip[4],world[4]; };
bool Near(float a,float b)
{ return std::isfinite(a)&&std::isfinite(b)&&std::fabs(a-b)<0.002f+std::fabs(b)*0.000002f; }
}
int main(int argc,char** argv)
{
    CHECK(argc==3||argc==4);
    const bool zfill=argc==4&&std::strcmp(argv[3],"zfill")==0;
    const bool sfx=argc==4&&std::strcmp(argv[3],"sfx")==0;
    CHECK(argc==3||zfill||sfx);
    std::ifstream shaderFile(argv[1],std::ios::binary);
    std::vector<char> shader((std::istreambuf_iterator<char>(shaderFile)),{});
    CHECK(shader.size()>32);
    std::ifstream fixture(argv[2],std::ios::binary);
    uint32_t count{};
    CHECK(fixture.read(reinterpret_cast<char*>(&count),sizeof(count))&&count>=2&&count<=64);
    std::vector<halo_ce::SaberBoneMatrix> bones(count);
    CHECK(fixture.read(reinterpret_cast<char*>(bones.data()),count*sizeof(bones[0])));
    float skin[800]{};
    CHECK(fixture.read(reinterpret_cast<char*>(skin),count*48));

    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    CHECK(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,
        D3D11_SDK_VERSION,&device,nullptr,&context)));
    ComPtr<ID3D11VertexShader> vertexShader;
    CHECK(SUCCEEDED(device->CreateVertexShader(shader.data(),shader.size(),nullptr,&vertexShader)));
    const char* geometrySource=R"(
        struct V {
            float4 position:SV_Position; float4 uv0:TEXCOORD0; float4 uv1:TEXCOORD1;
            float4 color:COLOR0; float4 normal:TEXCOORD3; float4 tangent:TEXCOORD4;
            float4 bitangent:TEXCOORD5; float4 direction:TEXCOORD6; float4 world:TEXCOORD7;
            float4 lighting:TEXCOORD2; float4 color1:COLOR1;
        };
        struct O { float4 position:SV_Position; float4 world:TEXCOORD7; };
        [maxvertexcount(1)] void main(point V input[1],inout PointStream<O> stream)
        { O output; output.position=input[0].position; output.world=input[0].world; stream.Append(output); }
    )";
    if (zfill) geometrySource=R"(
        struct V { float4 position:SV_Position; float3 uv:TEXCOORD0; };
        struct O { float4 position:SV_Position; float4 world:TEXCOORD7; };
        [maxvertexcount(1)] void main(point V input[1],inout PointStream<O> stream)
        { O output;output.position=input[0].position;output.world=0;stream.Append(output); }
    )";
    if (sfx) geometrySource=R"(
        struct V { float4 position:SV_Position; float4 uv0:TEXCOORD0;float4 color:COLOR0;
            float4 uv1:TEXCOORD1;float4 uv2:TEXCOORD2;float4 uv3:TEXCOORD3;float2 uv4:TEXCOORD4; };
        struct O { float4 position:SV_Position; float4 world:TEXCOORD7; };
        [maxvertexcount(1)] void main(point V input[1],inout PointStream<O> stream)
        { O output;output.position=input[0].position;output.world=0;stream.Append(output); }
    )";
    ComPtr<ID3DBlob> geometryCode,errors;
    CHECK(SUCCEEDED(D3DCompile(geometrySource,std::strlen(geometrySource),nullptr,nullptr,nullptr,
        "main","gs_5_0",0,0,&geometryCode,&errors)));
    const D3D11_SO_DECLARATION_ENTRY declaration[]={
        {0,"SV_Position",0,0,4,0},{0,"TEXCOORD",7,0,4,0}};
    const UINT outputStride=sizeof(Output);
    ComPtr<ID3D11GeometryShader> geometryShader;
    CHECK(SUCCEEDED(device->CreateGeometryShaderWithStreamOutput(
        geometryCode->GetBufferPointer(),geometryCode->GetBufferSize(),declaration,2,
        &outputStride,1,D3D11_SO_NO_RASTERIZED_STREAM,nullptr,&geometryShader)));
    const D3D11_INPUT_ELEMENT_DESC elements[]={
        {"POSITION",0,DXGI_FORMAT_R32G32B32A32_SINT,0,offsetof(Vertex,position),D3D11_INPUT_PER_VERTEX_DATA,0},
        {"BLENDWEIGHT",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,offsetof(Vertex,weights),D3D11_INPUT_PER_VERTEX_DATA,0},
        {"BLENDINDICES",0,DXGI_FORMAT_R32G32B32A32_UINT,0,offsetof(Vertex,indices),D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TANGENT",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,offsetof(Vertex,tangent),D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TANGENT",1,DXGI_FORMAT_R32G32B32A32_FLOAT,0,offsetof(Vertex,bitangent),D3D11_INPUT_PER_VERTEX_DATA,0},
        {"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,offsetof(Vertex,color),D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,offsetof(Vertex,uv),D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",1,DXGI_FORMAT_R32G32_FLOAT,0,offsetof(Vertex,uv1),D3D11_INPUT_PER_VERTEX_DATA,0},
        {"NORMAL",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,offsetof(Vertex,tangent),D3D11_INPUT_PER_VERTEX_DATA,0}};
    ComPtr<ID3D11InputLayout> layout;
    CHECK(SUCCEEDED(device->CreateInputLayout(elements,UINT(std::size(elements)),shader.data(),shader.size(),&layout)));
    std::vector<Vertex> vertices(count*4);
    for (uint32_t bone=0;bone<count;++bone)
        for (uint32_t point=0;point<4;++point)
        {
            auto& vertex=vertices[bone*4+point];
            for (auto& index:vertex.indices) index=bone;
            if (point) vertex.position[point-1]=32767;
        }
    auto makeBuffer=[&](UINT size,UINT bind,const void* source,ComPtr<ID3D11Buffer>& out)
    {
        D3D11_BUFFER_DESC description{};
        description.ByteWidth=size;description.Usage=D3D11_USAGE_DEFAULT;description.BindFlags=bind;
        D3D11_SUBRESOURCE_DATA initial{};initial.pSysMem=source;
        return SUCCEEDED(device->CreateBuffer(&description,source?&initial:nullptr,&out));
    };
    ComPtr<ID3D11Buffer> vertexBuffer,outputBuffer,readback,commonBuffer,skinBuffer,materialBuffer;
    CHECK(makeBuffer(UINT(vertices.size()*sizeof(Vertex)),D3D11_BIND_VERTEX_BUFFER,vertices.data(),vertexBuffer));
    CHECK(makeBuffer(UINT(vertices.size()*sizeof(Output)),D3D11_BIND_STREAM_OUTPUT,nullptr,outputBuffer));
    CHECK(makeBuffer(sizeof(skin),D3D11_BIND_CONSTANT_BUFFER,skin,skinBuffer));
    CHECK(makeBuffer(1440,D3D11_BIND_CONSTANT_BUFFER,nullptr,commonBuffer));
    CHECK(makeBuffer(384,D3D11_BIND_CONSTANT_BUFFER,nullptr,materialBuffer));
    D3D11_BUFFER_DESC readDescription{};outputBuffer->GetDesc(&readDescription);
    readDescription.Usage=D3D11_USAGE_STAGING;readDescription.BindFlags=0;
    readDescription.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    CHECK(SUCCEEDED(device->CreateBuffer(&readDescription,nullptr,&readback)));
    const UINT stride=sizeof(Vertex),offset=0;
    ID3D11Buffer* rawVertex=vertexBuffer.Get();
    context->IASetVertexBuffers(0,1,&rawVertex,&stride,&offset);
    context->IASetInputLayout(layout.Get());context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
    context->VSSetShader(vertexShader.Get(),nullptr,0);context->GSSetShader(geometryShader.Get(),nullptr,0);
    ID3D11Buffer* constantBuffers[8]{commonBuffer.Get(),nullptr,nullptr,nullptr,skinBuffer.Get(),nullptr,nullptr,materialBuffer.Get()};
    context->VSSetConstantBuffers(0,8,constantBuffers);
    unsigned draws=0;
    for (unsigned location=0;location<2;++location)
        for (unsigned eye=0;eye<2;++eye)
            for (unsigned corrected=0;corrected<2;++corrected)
            {
                float common[360]{},material[96]{};
                const float camera[3]{location?125.0f:0,location?-73.0f:0,location?18.0f:0};
                std::memcpy(common+200,camera,sizeof(camera));
                common[208]=common[209]=common[210]=1;
                // The two matrices deliberately differ: camera-relative world
                // projection carries asymmetry per eye; stock FP is fixed.
                const float world[16]{1.2f,0,eye?.18f:-.21f,0, 0,1.4f,.09f,0,
                    0,0,.7f,.2f, 0,0,1,0};
                const float fixed[16]{2.0f,0,0,0, 0,2.1f,0,0, 0,0,.8f,.2f, 0,0,1,0};
                std::memcpy(common+156,world,sizeof(world));std::memcpy(common+172,fixed,sizeof(fixed));
                common[246]=float(count); // all bone indices belong to first native range
                float selector[4]{1,1,1,1};
                if (corrected) CHECK(halo_ce::SelectSaberTrackedProjection(0x10000000u,selector));
                std::memcpy(material+(zfill?8:sfx?28:92),selector,sizeof(selector));
                context->UpdateSubresource(commonBuffer.Get(),0,nullptr,common,0,0);
                context->UpdateSubresource(materialBuffer.Get(),0,nullptr,material,0,0);
                ID3D11Buffer* stream=outputBuffer.Get();
                context->SOSetTargets(1,&stream,&offset);context->Draw(UINT(vertices.size()),0);
                context->SOSetTargets(0,nullptr,nullptr);
                context->CopyResource(readback.Get(),outputBuffer.Get());
                D3D11_MAPPED_SUBRESOURCE mapped{};
                CHECK(SUCCEEDED(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&mapped)));
                const auto* actual=static_cast<const Output*>(mapped.pData);
                for (uint32_t bone=0;bone<count;++bone)
                    for (uint32_t point=0;point<4;++point)
                    {
                        float expected[4]{},relative[4]{};
                        for (unsigned axis=0;axis<3;++axis)
                        {
                            expected[axis]=bones[bone].value[12+axis]+(point?bones[bone].value[(point-1)*4+axis]:0);
                            relative[axis]=expected[axis]-camera[axis];
                            if (!zfill&&!sfx&&!Near(actual[bone*4+point].world[axis],expected[axis]))
                                std::fprintf(stderr,"bone=%u point=%u axis=%u world=%g expected=%g\n",bone,point,axis,
                                    actual[bone*4+point].world[axis],expected[axis]);
                            if (!zfill&&!sfx) CHECK(Near(actual[bone*4+point].world[axis],expected[axis]));
                        }
                        relative[3]=1;
                        const float* lens=corrected?world:fixed;
                        for (unsigned row=0;row<4;++row)
                        {
                            float value=0;for (unsigned axis=0;axis<4;++axis) value+=lens[row*4+axis]*relative[axis];
                            if (!Near(actual[bone*4+point].clip[row],value))
                                std::fprintf(stderr,"%s bone=%u point=%u row=%u clip=%g expected=%g\n",zfill?"ZFILL":sfx?"SFX":"GLT",
                                    bone,point,row,actual[bone*4+point].clip[row],value);
                            CHECK(Near(actual[bone*4+point].clip[row],value));
                        }
                    }
                context->Unmap(readback.Get(),0);++draws;
            }
    std::printf("PASS native CE %s skinned shader: %u bones, %u vertices per draw, %u WARP draws; scaled transforms and per-eye projection verified\n",
        zfill?"ZFILL":sfx?"SFX":"GLT",count,UINT(vertices.size()),draws);
    return 0;
}
