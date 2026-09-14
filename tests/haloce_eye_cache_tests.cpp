#include "haloce_eye_cache.h"
#include <cstdio>
#include <vector>
#include <thread>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;
using namespace halo_ce;
namespace
{
PreparedReceipt Receipt(uint64_t serial,uint32_t width,uint32_t height)
{
    PreparedReceipt r{};
    r.ticket={1,0x12340,3,PreparationOrigin::CopiedList};
    r.tracking.serial=r.pair.serial=serial;
    r.tracking.generation=r.pair.generation=3;
    r.tracking.spaceEpoch=r.pair.spaceEpoch=7;
    r.tracking.headPosition={0.1f,1.6f,0.2f};
    for (int eye=0;eye<2;++eye)
    {
        r.pair.cameras[eye].viewportWidth=static_cast<float>(width);
        r.pair.cameras[eye].viewportHeight=static_cast<float>(height);
        r.pair.covers[eye]={1.8f,1.0f,0.9f};
    }
    return r;
}
bool Pixels(ID3D11Device* device,ID3D11DeviceContext* context,
    ID3D11Texture2D* texture,const D3D11_TEXTURE2D_DESC& source,uint32_t expected)
{
    auto d=source;
    d.BindFlags=0; d.Usage=D3D11_USAGE_STAGING; d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> readback;
    if (FAILED(device->CreateTexture2D(&d,nullptr,&readback))) return false;
    context->CopyResource(readback.Get(),texture);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&mapped))) return false;
    bool ok=true;
    for (UINT y=0;y<d.Height;++y)
    {
        const auto* row=reinterpret_cast<const uint32_t*>(
            static_cast<const uint8_t*>(mapped.pData)+y*mapped.RowPitch);
        for (UINT x=0;x<d.Width;++x) if (row[x]!=expected) ok=false;
    }
    context->Unmap(readback.Get(),0);
    return ok;
}
}
int main()
{
    int failures=0;
    const auto check=[&](bool ok,const char* why) {
        if (!ok) { ++failures; std::fprintf(stderr,"CE GPU cache: %s\n",why); }
    };
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    const D3D_FEATURE_LEVEL level=D3D_FEATURE_LEVEL_11_0;
    const HRESULT hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,
        &level,1,D3D11_SDK_VERSION,&device,nullptr,&context);
    if (FAILED(hr)) { std::fprintf(stderr,"WARP device failed: %08lx\n",hr); return 1; }
    D3D11_TEXTURE2D_DESC d{};
    d.Width=32; d.Height=16; d.MipLevels=1; d.ArraySize=1;
    d.Format=DXGI_FORMAT_R8G8B8A8_UNORM; d.SampleDesc.Count=1;
    d.Usage=D3D11_USAGE_DEFAULT; d.BindFlags=D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> source;
    if (FAILED(device->CreateTexture2D(&d,nullptr,&source))) return 1;
    std::vector<uint32_t> pixels(d.Width*d.Height);
    const auto paint=[&](uint32_t color) {
        std::fill(pixels.begin(),pixels.end(),color);
        context->UpdateSubresource(source.Get(),0,nullptr,pixels.data(),d.Width*4,0);
    };
    EyeCache cache;
    EyeCache::Key key{};
    EyeCache::Completed complete{};
    check(cache.Prepare(device.Get(),context.Get(),d,3,1),"cold cache preparation");
    check(cache.Begin(Receipt(100,d.Width,d.Height),key),"receipt starts an exact frame");
    check(!cache.AcquireCompleted(key,context.Get(),complete),"no unrendered submission");
    paint(0xff112233);
    check(cache.Capture(key,0,context.Get(),source.Get(),d),"capture left from native-lived source");
    check(!cache.AcquireCompleted(key,context.Get(),complete),"no half-pair submission");
    paint(0xffaabbcc);
    check(cache.Capture(key,1,context.Get(),source.Get(),d),"capture right from reused native surface");
    check(!cache.AcquireCompleted(key,context.Get(),complete),"native frame must finish first");
    paint(0xff000000);
    source.Reset();
    check(cache.Finish(key),"completed native frame publishes pair");
    bool borrowed=cache.AcquireCompleted(key,context.Get(),complete);
    const uint64_t firstBorrow=complete.borrowId;
    check(borrowed,"complete pair is available");
    if (borrowed)
    {
        check(complete.tracking.serial==100&&complete.tracking.spaceEpoch==7&&
            complete.tracking.headPosition.y==1.6f,"submission retains preparation tracking");
        check(Pixels(device.Get(),context.Get(),complete.eyes[0],complete.descriptor,0xff112233)&&
            Pixels(device.Get(),context.Get(),complete.eyes[1],complete.descriptor,0xffaabbcc),
            "real GPU copies preserve distinct eyes after native surface recycling and release");
        std::thread cold([&] {
            check(!cache.Reset(),"cold retirement cannot release borrowed textures");
            check(!cache.Prepare(device.Get(),context.Get(),d,3,2),"cold replacement cannot race submission");
        });
        cold.join();
        check(Pixels(device.Get(),context.Get(),complete.eyes[0],complete.descriptor,0xff112233),
            "declined cold replacement leaves borrowed GPU storage intact");
        check(cache.ReleaseCompleted(complete.borrowId),"release exact submission borrow");
    }
    if (FAILED(device->CreateTexture2D(&d,nullptr,&source))) return 1;
    check(!cache.AcquireCompleted(key,context.Get(),complete),"submission is consumed once");
    check(!cache.Begin(Receipt(100,d.Width,d.Height),key),"same tracking frame cannot be replayed");
    check(cache.Begin(Receipt(101,d.Width,d.Height),key),"next frame recovers");
    check(!cache.Capture(key,1,context.Get(),source.Get(),d)&&!cache.Finish(key),
        "out-of-order eye drops only its frame");
    check(cache.Begin(Receipt(102,d.Width,d.Height),key),"recover after order failure");
    check(cache.Capture(key,0,context.Get(),source.Get(),d)&&
        !cache.Capture(key,0,context.Get(),source.Get(),d)&&!cache.Finish(key),"duplicate eye invalidates pair");
    check(cache.Begin(Receipt(103,d.Width,d.Height),key),"start descriptor mismatch case");
    auto changed=d; ++changed.Width;
    check(!cache.Capture(key,0,context.Get(),source.Get(),changed)&&!cache.Finish(key),
        "resize cannot copy using a stale descriptor");
    ComPtr<ID3D11DeviceContext> deferred;
    check(SUCCEEDED(device->CreateDeferredContext(0,&deferred)),"create independent deferred context");
    check(cache.Begin(Receipt(104,d.Width,d.Height),key)&&
        !cache.Capture(key,0,deferred.Get(),source.Get(),d),"wrong/deferred context cannot capture");
    check(cache.Begin(Receipt(105,d.Width,d.Height),key)&&cache.Drop(key)&&!cache.Finish(key),
        "native frame failure drops captured identity");
    const auto oldKey=key;
    check(cache.Reset()&&cache.Prepare(device.Get(),context.Get(),d,3,2),"cold retirement and rebuild");
    check(cache.Begin(Receipt(105,d.Width,d.Height),key)&&key.resourceEpoch!=oldKey.resourceEpoch,
        "new resource epoch prevents stale GPU identity reuse");
    check(!cache.Capture(oldKey,0,context.Get(),source.Get(),d),"old cache callback rejected");
    check(cache.Begin(Receipt(106,d.Width,d.Height),key)&&
        cache.Capture(key,0,context.Get(),source.Get(),d)&&
        cache.Capture(key,1,context.Get(),source.Get(),d)&&cache.Finish(key),"recover after stale callback");
    check(!cache.AcquireCompleted(key,deferred.Get(),complete),"submission must preserve context ordering");
    borrowed=cache.AcquireCompleted(key,context.Get(),complete);
    check(borrowed,"incorrect context does not consume a valid pair");
    if (borrowed)
    {
        check(!cache.ReleaseCompleted(firstBorrow)&&!cache.Reset(),
            "delayed old borrow release cannot free a newer submission");
        check(cache.ReleaseCompleted(complete.borrowId),"new borrow remains releasable");
    }
    changed=d; changed.SampleDesc.Count=2;
    check(!cache.Prepare(device.Get(),context.Get(),changed,3,3),"MSAA needs a separate verified resolve path");
    check(!cache.Begin(Receipt(107,d.Width,d.Height),key),"failed preparation retains no old admission");
    check(!cache.Prepare(device.Get(),deferred.Get(),d,3,3),"deferred context rejected cold");
    check(cache.Prepare(device.Get(),context.Get(),d,3,3),"cold failure recovers");
    check(cache.Reset()&&!cache.Prepare(device.Get(),context.Get(),d,3,3),
        "retirement does not permit reuse of an old resource epoch");
    return failures?1:0;
}
