#include "../src/dll/weapon_accessory_renderer.h"
#include "../src/common/weapon_magazines.generated.h"
#include "../src/common/weapon_model_observation.h"
#include <cstdio>
#include <limits>
#include <vector>
#include <string>
#include <fstream>

using Microsoft::WRL::ComPtr;
using namespace weapon_interaction;
unsigned checks{},failures{};
void Check(bool value,const char* text)
{ ++checks;if(!value){++failures;std::printf("FAIL: %s\n",text);} }
int main(int argc,char** argv)
{
    weapon_model::Observations observations;
    unsigned visible[7]{},needles[7]{};
    for(const auto& model:weapon_model::kModels)
    {
        const unsigned title=static_cast<unsigned>(model.title);
        Check(weapon_model::Find(model.title,model.identity)==&model,"unique exact title/model lookup");
        if(model.title==GameTitle::Halo2)
        {
            unsigned matches=0;
            for(const auto& other:weapon_model::kModels) if(other.title==model.title&&other.nodeCount==model.nodeCount)
            {
                bool match=true;
                for(unsigned a=0;a<3;++a) match=match&&std::fabs(model.minimum[a]-other.minimum[a])<=.000002f&&
                    std::fabs(model.maximum[a]-other.maximum[a])<=.000002f;
                matches+=match;
            }
            const auto* found=weapon_model::FindHalo2(model.minimum,model.maximum,model.nodeCount);
            Check(matches==1?found==&model:!found,"H2 unique tuples select exactly; ambiguous tuples refuse");
            Check(matches==1||(!model.vertexCount&&!model.needles),"no requested H2 magazine/needle model is ambiguous");
        }
        Check(model.firstVertex<=std::size(weapon_model::kVertices)&&
            model.vertexCount<=std::size(weapon_model::kVertices)-model.firstVertex&&
            model.vertexCount%3==0,"catalogue mesh range is bounded triangles");
        for (unsigned v=0;v<model.vertexCount;++v) {
            const auto& uv=weapon_model::kVertices[model.firstVertex+v].uv;
            Check(std::isfinite(uv[0])&&std::isfinite(uv[1])&&uv[0]>0&&uv[0]<1&&uv[1]>0&&uv[1]<1,
                "authored magazine UVs remain inside the padded surface atlas");
        }
        visible[title]+=model.vertexCount?1:0;needles[title]+=model.needles?1:0;
        for(bool left:{false,true})
        {
            Sample s{};s.title=model.title;s.now=1000;s.generation=2;s.space=4;s.ready=true;
            s.weaponGraph=model.identity;s.head={0,1.6f,0};s.primary={.2f,1.2f,-.5f};s.support={-.2f,1.2f,-.5f};
            Settings c{};c.reload=true;c.leftHanded=left;
            for(bool holsters:{false,true}) for(bool held:{false,true}) {
                auto disabled=c;disabled.reload=false;disabled.holsters=holsters;
                Output output{};output.holdingMagazine=held;
                Check(!weapon_accessory::Build(s,disabled,output,{}).model,
                    "manual reload off hides every authored part, including holster-only and stale held state");
                auto custom=s;custom.weaponGraph=0xF123456789ABCDEF;
                Check(!weapon_accessory::Build(custom,disabled,output,{}).model,
                    "manual reload off hides generic custom-weapon item with either holster state");
            }
            auto p=weapon_accessory::Build(s,c,{},{});
            const bool promethean=model.title==GameTitle::Halo4&&std::strstr(model.name,"forerunner_");
            if (promethean && !model.vertexCount) {
                auto knownOnly=c;knownOnly.genericVisual=false;
                Check(weapon_accessory::Build(s,knownOnly,{},{}).model==&weapon_model::kPrometheanReloadModel,
                    "known Promethean item survives disabling unfamiliar weapon placeholders");
            }
            Check((p.model!=nullptr)==(model.vertexCount!=0||promethean),
                "authored magazines and recognized Promethean reload tokens are visible");
            if(p.model)
            {
                Vec pouch{},holster{};Zones(s,c,pouch,holster);
                Check(Near(p.pose.position,pouch,.0001f),"visible pouch and grab zone coincide in both hands");
                Output held{};held.holdingMagazine=true;
                const Quat q{0,.70710678f,0,.70710678f};
                p=weapon_accessory::Build(s,c,held,q);
                Check(Near(p.pose.position,s.support+Rotate(q,{0,-.015f,-.035f}),.0001f),"carried part follows support hand rotation and position");
                Check(weapon_accessory::Current(p,s.title,2,4,model.identity,1100,true,true),"fresh accessory receipt admitted");
                Check(!weapon_accessory::Current(p,s.title,2,4,model.identity,1151,true,true)&&
                    !weapon_accessory::Current(p,s.title,2,4,model.identity,999,true,true),"future/stale receipt rejected");
                Check(!weapon_accessory::Current(p,s.title,3,4,model.identity,1100,true,true)&&
                    !weapon_accessory::Current(p,s.title,2,5,model.identity,1100,true,true)&&
                    !weapon_accessory::Current(p,s.title,2,4,0,1100,true,true)&&
                    !weapon_accessory::Current(p,s.title,2,4,model.identity,1100,false,true)&&
                    !weapon_accessory::Current(p,s.title,2,4,model.identity,1100,true,false),
                    "generation/recenter/weapon/toggle/gameplay invalidation hides accessory");
            }
            s.ready=false;Check(!weapon_accessory::Build(s,c,{},{}).model,"unavailable tracking never leaves pouch visible");
            s.ready=true;s.dualWield=true;Check(!weapon_accessory::Build(s,c,{},{}).model,"dual inventory keeps native presentation");
        }
        if(model.needles) for(bool left:{false,true}) for(unsigned dt:{8u,11u,14u})
        {
            State state;Settings c{};c.reload=c.needleShake=true;c.leftHanded=left;
            Sample s{};s.title=model.title;s.weaponGraph=model.identity;s.generation=1;s.space=1;s.now=1000;s.ready=true;
            s.head={0,1.6f,0};s.primary={.2f,1.2f,-.5f};s.support={-.2f,1.2f,-.5f};
            state.Update(s,c);s.now+=dt;
            Check(!state.Update(s,c).consumePrimary,"needle detection never requires or consumes grip");
            unsigned requests=0;
            for(int i=1;i<=120;++i) {s.now+=dt;s.primary.y=1.2f+.08f*std::sin(i*dt*.02f);requests+=state.Update(s,c).reloadRequested;}
            Check(requests==1,"all needle models/hands/sample rates reload once per uninterrupted shake without grip");
        }
    }
    for(unsigned t=1;t<=6;++t)
    {
        Check(visible[t]>0&&needles[t]>0,"all six titles have visible parts and needle identities");
        const auto title=static_cast<GameTitle>(t);
        Check(observations.Publish({title,2,123,4,1000}),"model observation publishes without locks or allocation");
        Check(observations.Read(title,2,4,1100)==123,"fresh same-generation model read");
        Check(!observations.Read(title,3,4,1100)&&!observations.Read(title,2,5,1100)&&
            !observations.Read(title,2,4,1151)&&!observations.Read(title,2,4,999),"stale and foreign observations rejected");
        observations.Publish({title,2,0,4,1100});Check(!observations.Read(title,2,4,1101),"unknown model invalidates older known identity");
        Sample sample{};sample.title=title;sample.ready=true;sample.now=1000;sample.space=2;sample.generation=3;
        sample.weaponGraph=0xabcdef1234567890ull;sample.head={0,1.6f,0};
        Settings settings{};settings.reload=true;
        auto generic=weapon_accessory::Build(sample,settings,{},{});
        Check(generic.model==&weapon_model::kGenericReloadModel&&generic.identity==sample.weaponGraph,
            "all-title detected unfamiliar weapons receive automatic generic reload visual");
        Check(!weapon_accessory::Current(generic,title,3,2,sample.weaponGraph+1,1100,true,true),
            "changing between unfamiliar weapons invalidates the old carried item");
        Check(!NeedleWeapon(title,sample.weaponGraph),"unfamiliar geometry never guesses needle ammunition");
        settings.genericVisual=false;Check(!weapon_accessory::Build(sample,settings,{},{}).model,"generic visual separately optional");
        settings.genericVisual=true;sample.weaponGraph=0;
        Check(!weapon_accessory::Build(sample,settings,{},{}).model,"missing native model evidence cannot fabricate held weapon");
    }
    const float low[]{-.1f,-.1f,-.1f},high[]{.25f,.2f,.1f};
    Check(weapon_model::Halo2LiveIdentity(low,high,8,123)!=weapon_model::Halo2LiveIdentity(low,high,8,124),
        "distinct H2 custom tags keep independent identities even with equal bounds");
    const float bad[]{std::numeric_limits<float>::quiet_NaN(),0,0};
    Check(!weapon_model::Halo2LiveIdentity(bad,high,8,123)&&!weapon_model::Halo2LiveIdentity(low,high,65,123),
        "invalid custom model bounds and node counts refuse identity");
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    HRESULT hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context);
    Check(SUCCEEDED(hr),"create actual D3D11 WARP device");if(FAILED(hr)) return 1;
    constexpr unsigned size=192;
    D3D11_TEXTURE2D_DESC desc{};desc.Width=desc.Height=size;desc.ArraySize=desc.MipLevels=1;
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.BindFlags=D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> texture,readback;ComPtr<ID3D11RenderTargetView> target;
    Check(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&texture)),"GPU color target allocation");
    Check(SUCCEEDED(device->CreateRenderTargetView(texture.Get(),nullptr,&target)),"GPU color view allocation");
    desc.BindFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    Check(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&readback)),"GPU readback allocation");
    if(failures) return 1;
    WeaponAccessoryRenderer renderer;
    const D3D11_VIEWPORT viewport{0,0,size,size,0,1},sentinel{3,5,31,27,.2f,.8f};
    auto* targetPtr=target.Get();context->OMSetRenderTargets(1,&targetPtr,nullptr);
    context->RSSetViewports(1,&sentinel);context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
    ComPtr<ID3D11Texture2D> nativeTexture;ComPtr<ID3D11ShaderResourceView> nativeSurface;
    ComPtr<ID3D11SamplerState> nativeSampler;
    auto nativeDesc=desc;nativeDesc.Width=nativeDesc.Height=1;
    nativeDesc.Usage=D3D11_USAGE_IMMUTABLE;nativeDesc.CPUAccessFlags=0;
    nativeDesc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    const uint32_t nativePixel=0xffcc4488;D3D11_SUBRESOURCE_DATA nativeData{&nativePixel,4,0};
    Check(SUCCEEDED(device->CreateTexture2D(&nativeDesc,&nativeData,&nativeTexture))&&
        SUCCEEDED(device->CreateShaderResourceView(nativeTexture.Get(),nullptr,&nativeSurface)),"native surface sentinel");
    D3D11_SAMPLER_DESC nativeSampling{};nativeSampling.Filter=D3D11_FILTER_MIN_MAG_MIP_POINT;
    nativeSampling.AddressU=nativeSampling.AddressV=nativeSampling.AddressW=D3D11_TEXTURE_ADDRESS_WRAP;
    Check(SUCCEEDED(device->CreateSamplerState(&nativeSampling,&nativeSampler)),"native sampler sentinel");
    auto* nativeSrv=nativeSurface.Get();auto* nativeSm=nativeSampler.Get();
    context->PSSetShaderResources(0,1,&nativeSrv);context->PSSetSamplers(0,1,&nativeSm);
    unsigned texturedColor[7]{};
    std::vector<unsigned char> montage;std::ofstream manifest;
    if(argc>1) manifest.open(std::string(argv[1])+".txt");
    unsigned picture=0;double leftCenter=0,rightCenter=0;
    for(size_t modelIndex=0;modelIndex<std::size(weapon_model::kModels)+2;++modelIndex)
    {
        const auto& model=modelIndex==std::size(weapon_model::kModels)+1?weapon_model::kPrometheanReloadModel:
            modelIndex==std::size(weapon_model::kModels)?weapon_model::kGenericReloadModel:
            weapon_model::kModels[modelIndex];
        if(!model.vertexCount) continue;
        weapon_accessory::Presentation p{};p.model=&model;p.pose.position={0,0,-.45f};
        p.pose.orientation={.15f,.25f,0,std::sqrt(1-.15f*.15f-.25f*.25f)};
        for(int eye=0;eye<2;++eye)
        {
            const float clear[]{0,0,0,1};context->ClearRenderTargetView(target.Get(),clear);
            weapon_accessory::Pose eyePose{{eye?.032f:-.032f,0,0},{}};
            weapon_accessory::Constants constants{};
            Check(weapon_accessory::Projection(p,eyePose,-.55f,.55f,-.55f,.55f,constants),"finite stereo projection accepted");
            ComPtr<ID3D11CommandList> staged;
            Check(SUCCEEDED(renderer.Draw(device.Get(),context.Get(),target.Get(),size,size,viewport,p,constants,&staged))&&staged,
                "production renderer stages shaders and mesh draw without touching world target");
            context->CopyResource(readback.Get(),texture.Get());
            D3D11_MAPPED_SUBRESOURCE untouched{};
            hr=context->Map(readback.Get(),0,D3D11_MAP_READ,0,&untouched);
            Check(SUCCEEDED(hr),"read target before optional pair commit");if(FAILED(hr)) return 1;
            bool black=true;
            for(unsigned y=0;y<size;++y) for(unsigned x=0;x<size;++x)
            {
                const auto* pixel=static_cast<const unsigned char*>(untouched.pData)+y*untouched.RowPitch+x*4;
                black=black&&!pixel[0]&&!pixel[1]&&!pixel[2];
            }
            context->Unmap(readback.Get(),0);
            Check(black,"discarding a staged accessory leaves every native world pixel untouched");
            context->ExecuteCommandList(staged.Get(),TRUE);
            ComPtr<ID3D11RenderTargetView> after;context->OMGetRenderTargets(1,&after,nullptr);
            D3D11_VIEWPORT afterVp{};UINT count=1;context->RSGetViewports(&count,&afterVp);
            D3D11_PRIMITIVE_TOPOLOGY topology{};context->IAGetPrimitiveTopology(&topology);
            Check(after.Get()==target.Get()&&afterVp.TopLeftX==sentinel.TopLeftX&&afterVp.Width==sentinel.Width&&
                afterVp.MinDepth==sentinel.MinDepth&&topology==D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP,
                "accessory command list restores native target viewport depth-range and IA state");
            ComPtr<ID3D11ShaderResourceView> afterSurface;ComPtr<ID3D11SamplerState> afterSampler;
            context->PSGetShaderResources(0,1,&afterSurface);context->PSGetSamplers(0,1,&afterSampler);
            Check(afterSurface.Get()==nativeSrv&&afterSampler.Get()==nativeSm,
                "textured accessory restores native pixel surface and sampler bindings");
            context->CopyResource(readback.Get(),texture.Get());
            D3D11_MAPPED_SUBRESOURCE mapped{};hr=context->Map(readback.Get(),0,D3D11_MAP_READ,0,&mapped);
            Check(SUCCEEDED(hr),"map executed GPU image");if(FAILED(hr)) return 1;
            unsigned pixels=0;double sum=0;
            std::vector<unsigned char> rgb(size*size*3);
            for(unsigned y=0;y<size;++y) for(unsigned x=0;x<size;++x)
            {
                const auto* pixel=static_cast<const unsigned char*>(mapped.pData)+y*mapped.RowPitch+x*4;
                if(pixel[0]||pixel[1]||pixel[2]) {++pixels;sum+=x;}
                if(!weapon_model::UsesTokenGeometry(&model) &&
                    (std::abs(int(pixel[0])-int(pixel[1]))>=3 || std::abs(int(pixel[1])-int(pixel[2]))>=3))
                    ++texturedColor[static_cast<unsigned>(model.title)];
                for(unsigned c=0;c<3;++c) rgb[(y*size+x)*3+c]=pixel[c];
            }
            context->Unmap(readback.Get(),0);
            Check(pixels>10,"each title-specific part produces actual visible pixels in both eyes");
            if(eye==0) leftCenter=pixels?sum/pixels:0;else rightCenter=pixels?sum/pixels:0;
            if(argc>1&&eye==0)
            {
                std::ofstream file(std::string(argv[1])+"-"+std::to_string(picture)+".ppm",std::ios::binary);
                file<<"P6\n"<<size<<' '<<size<<"\n255\n";file.write(reinterpret_cast<const char*>(rgb.data()),rgb.size());
                manifest<<picture++<<' '<<static_cast<unsigned>(model.title)<<' '<<model.name<<'\n';
            }
        }
        Check(leftCenter>rightCenter+1,"real two-eye images have correct close-range stereo disparity");
        weapon_accessory::Constants invalid{};
        Check(!weapon_accessory::Projection(p,{},0,0,-.5f,.5f,invalid),"degenerate projection refused before draw");
        p.pose.position.x=std::numeric_limits<float>::quiet_NaN();
        Check(!weapon_accessory::Projection(p,{},-.5f,.5f,-.5f,.5f,invalid),"nonfinite accessory pose rejected");
    }
    renderer.Reset();
    for(unsigned title=1;title<=6;++title)
        Check(texturedColor[title]>10,"every title samples authored weapon colors instead of a flat neutral material");
    std::printf("Weapon accessories: %u checks, %u failures\n",checks,failures);
    return failures?1:0;
}
