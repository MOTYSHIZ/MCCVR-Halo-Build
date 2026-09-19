#include "../src/dll/haloce_comfort.cpp"
#include <cstdio>
#include <array>
#include <limits>

namespace
{
unsigned failures{},calls{};
GameTitle testTitle=GameTitle::HaloCE;
uint32_t testGeneration=5;
bool admitted=true,raiseNative{};
Tracking testTracking{};
SaberCamera eyeCamera{};
uintptr_t lastEffect{},lastSource{};
const void* lastQuad{};
const SaberCamera* lastCamera{};
float lastStrength{};
std::array<uint8_t,0x110> flareEffect{};
float flareScreen[2]{600,400};
unsigned flareDraws{};
unsigned flareCallbackDepth{};
unsigned flareIndex{};
bool flareThrows{},flareForeignRecord{},flareNested{},flareReadFailure{},flareOwnershipRevoked{};
const float* lastFlareScreen{};
uintptr_t lastFlareRecord{};
uint64_t lastFlareOpaque{};
void Check(bool ok,const char* message)
{ if (!ok) { ++failures;std::fprintf(stderr,"CE comfort: %s\n",message); } }
void __fastcall NativeBlur(uintptr_t effect,const void* quad,uintptr_t source,const SaberCamera* camera,float strength)
{
    ++calls;lastEffect=effect;lastSource=source;lastQuad=quad;lastCamera=camera;lastStrength=strength;
    if (raiseNative) RaiseException(0xe0424242,0,0,nullptr);
}
void Draw(uintptr_t caller=0)
{ BlurDispatch(17,&testTracking,23,&eyeCamera,.375f,caller?caller:moduleBase+0x45107f); }
bool NativeException()
{
    __try { Draw(); }
    __except(GetExceptionCode()==0xe0424242?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH)
    { return true; }
    return false;
}
void __fastcall NativeFlareSprite(uintptr_t,const float* screen,uintptr_t record)
{
    ++flareDraws;lastFlareScreen=screen;lastFlareRecord=record;
    flareCallbackDepth=ce_flare::callbacks.load();
    if (flareThrows) RaiseException(0xe0424243,0,0,nullptr);
}
void __fastcall NativeFlareProjection(uintptr_t effect,uint64_t opaque,const SaberCamera* camera)
{
    lastFlareOpaque=opaque;
    if (flareNested)
    {
        flareNested=false;
        ce_flare::ProjectionDispatch(effect,opaque,camera,ce_flare::base+0x45109c);
        flareNested=true;
        return;
    }
    if (flareOwnershipRevoked) ce_flare::active=false;
    ce_flare::DrawDispatch(effect,flareReadFailure?nullptr:flareScreen,
        effect+(flareIndex?0x98:0x70)+(flareForeignRecord?4:0),
        ce_flare::base+(flareIndex?0x4472eb:0x44718a));
}
void Flare(float depth,uintptr_t caller=0)
{
    const Vec3 light{.1f,.2f,depth};
    std::memcpy(flareEffect.data()+(flareIndex?0x98:0x70),&light,sizeof(light));
    ce_flare::ProjectionDispatch(reinterpret_cast<uintptr_t>(flareEffect.data()),0x1122334455667788,
        &eyeCamera,caller?caller:ce_flare::base+0x45109b);
}
bool FlareException()
{
    __try { Flare(10); }
    __except(GetExceptionCode()==0xe0424243?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH)
    { return true; }
    return false;
}
bool FlareHookException(bool projection)
{
    __try
    {
        if (projection)
            ce_flare::ProjectionHook(reinterpret_cast<uintptr_t>(flareEffect.data()),0x1122334455667788,&eyeCamera);
        else ce_flare::DrawHook(reinterpret_cast<uintptr_t>(flareEffect.data()),flareScreen,
            reinterpret_cast<uintptr_t>(flareEffect.data())+0x70);
    }
    __except(GetExceptionCode()==0xe0424243?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH)
    { return true; }
    return false;
}
}
GameTitle TitleAdapter_GetActiveTitle() { return testTitle; }
uint32_t TitleAdapter_GetGeneration(GameTitle) { return testGeneration; }
bool HaloCE_GetAnniversaryEyeTracking(const halo_ce::SaberCamera* camera,halo_ce::Tracking& out) noexcept
{ out={};if (!admitted||camera!=&eyeCamera) return false;out=testTracking;return true; }
void Logf(const char*,...) {}
bool WaitForNativeDetourQuiescence(const void* const*,const void* const*,size_t,const std::atomic<uint32_t>& count)
{ return !count.load(); }
int main()
{
    for (const auto* entry:{reinterpret_cast<const void*>(&BlurHook),
        reinterpret_cast<const void*>(&ce_flare::ProjectionHook),
        reinterpret_cast<const void*>(&ce_flare::DrawHook),
        reinterpret_cast<const void*>(&ce_flare::ProjectionDispatch),
        reinterpret_cast<const void*>(&ce_flare::DrawDispatch)})
    {
        DWORD64 imageBase{};
        const auto address=reinterpret_cast<DWORD64>(entry);
        const auto* unwind=RtlLookupFunctionEntry(address,&imageBase,nullptr);
        Check(unwind&&address>=imageBase+unwind->BeginAddress&&
            address<imageBase+unwind->EndAddress,
            "every real comfort/flare retirement entry has Release x64 unwind metadata");
    }
    moduleBase=0x180000000;generation=testGeneration;active=installed=true;
    original=&NativeBlur;testTracking.generation=testGeneration;
    Draw();Draw();
    Check(calls==0&&suppressed.load()==2&&!callbacks.load(),"both admitted primary eyes honor motion blur off");
    for (unsigned reason=0;reason<7;++reason)
    {
        switch(reason)
        {
        case 0:testTracking.motionBlur=true;break;
        case 1:admitted=false;break;
        case 2:testTitle=GameTitle::Halo3;break;
        case 3:retiring=true;break;
        case 4:testTracking.generation=6;break;
        case 5:testGeneration=6;break;
        case 6:installed=false;break;
        }
        const auto before=calls;Draw();
        Check(calls==before+1&&lastEffect==17&&lastSource==23&&lastQuad==&testTracking&&
            lastCamera==&eyeCamera&&lastStrength==.375f&&!callbacks.load(),
            "enabled/stock/retired/foreign/stale calls preserve every native argument exactly once");
        testTracking.motionBlur=false;admitted=true;testTitle=GameTitle::HaloCE;
        retiring=false;testTracking.generation=testGeneration=5;installed=true;
    }
    const auto before=calls;Draw(moduleBase+0x451080);
    Check(calls==before+1,"a different native caller is never suppressed");
    testTracking.motionBlur=true;raiseNative=true;
    Check(NativeException()&&!callbacks.load()&&exceptions.load()==1,
        "native exception propagates after callback ownership retires");
    raiseNative=false;testTracking.motionBlur=false;Draw();
    Check(!callbacks.load()&&suppressed.load()==3,"subsequent valid eye recovers after native failure");
    ce_flare::base=moduleBase;ce_flare::generation=5;ce_flare::installed=ce_flare::active=true;
    ce_flare::projection.original=reinterpret_cast<void*>(&NativeFlareProjection);
    ce_flare::draw.original=reinterpret_cast<void*>(&NativeFlareSprite);
    eyeCamera.pose.matrix[10]=1;eyeCamera.nearPlane=.025f;
    eyeCamera.viewportWidth=2912;eyeCamera.viewportHeight=2100;
    for (flareIndex=0;flareIndex<2;++flareIndex)
    {
        for (float depth:{10.0f,.025f,.024f,0.0f,-1.0f})
        {
            const auto before=flareDraws;Flare(depth);
            Check(flareDraws==before+unsigned(depth>=.025f)&&!ce_flare::callbacks.load()&&!ce_flare::scope,
                "each native flare record clips inside/behind its tracked near plane and retires its scope");
        }
    }
    flareIndex=0;
    for (const auto raster: {std::array<float,2>{2912,2100}, {3788,2732}, {2204,2204}, {1000,3000}})
    {
        eyeCamera.viewportWidth=raster[0];eyeCamera.viewportHeight=raster[1];
        for (const auto point: {std::array<float,2>{0,0}, {raster[0],raster[1]},
            {raster[0]*.5f,raster[1]*.5f}, {-raster[0]*.1f,raster[1]*.5f}})
        {
            flareScreen[0]=point[0];flareScreen[1]=point[1];
            const auto before=flareDraws;Flare(10);
            Check(flareDraws==before+1,"in-raster and modest peripheral flares retain the original native draw");
        }
        for (flareIndex=0;flareIndex<2;++flareIndex)
        {
            flareScreen[0]=raster[0]*1.5f;flareScreen[1]=raster[1]*.5f;
            const auto before=flareDraws;const auto rejected=ce_flare::offscreen.load();
            Flare(10);
            Check(flareDraws==before&&ce_flare::offscreen.load()==rejected+1,
                "both native records reject residual offscreen halos despite positive safe depth");
            admitted=false;Flare(10);admitted=true;
            Check(flareDraws==before+1,"the offscreen envelope never suppresses an unowned native callback");
        }
    }
    flareIndex=0;eyeCamera.viewportWidth=2912;eyeCamera.viewportHeight=2100;
    flareScreen[0]=600;flareScreen[1]=400;
    flareScreen[1]=std::numeric_limits<float>::infinity();
    const auto finiteBefore=flareDraws;Flare(10);
    Check(flareDraws==finiteBefore,"nonfinite projected coordinates never reach the sprite draw");
    flareScreen[1]=400;
    const auto provenCamera=eyeCamera;
    for (unsigned reason=0;reason<8;++reason)
    {
        switch(reason)
        {
        case 0:eyeCamera.viewportWidth=0;break;
        case 1:eyeCamera.viewportHeight=-1;break;
        case 2:eyeCamera.viewportWidth=std::numeric_limits<float>::quiet_NaN();break;
        case 3:eyeCamera.viewportHeight=std::numeric_limits<float>::infinity();break;
        case 4:eyeCamera.viewportWidth=16385;break;
        case 5:eyeCamera.viewportHeight=2100.5f;break;
        case 6:eyeCamera.viewportX=1;break;
        case 7:eyeCamera.viewportY=std::numeric_limits<float>::quiet_NaN();break;
        }
        const auto before=flareDraws;const auto unknown=ce_flare::unproven.load();
        flareScreen[0]=100000;Flare(10);
        Check(flareDraws==before+1&&ce_flare::unproven.load()==unknown+1,
            "invalid raster proof leaves a front-facing native flare untouched");
        eyeCamera=provenCamera;flareScreen[0]=600;
    }
    for (unsigned reason=0;reason<6;++reason)
    {
        switch(reason)
        {
        case 0:eyeCamera.pose.matrix[10]=0;break;
        case 1:eyeCamera.pose.matrix[12]=std::numeric_limits<float>::quiet_NaN();break;
        case 2:eyeCamera.nearPlane=0;break;
        case 3:eyeCamera.nearPlane=std::numeric_limits<float>::infinity();break;
        case 4:flareReadFailure=true;break;
        case 5:eyeCamera.pose.matrix[10]=std::numeric_limits<float>::max();break;
        }
        const auto before=flareDraws;
        const auto beforeUnproven=ce_flare::unproven.load();
        Flare(-1);
        Check(flareDraws==before+1&&ce_flare::unproven.load()==beforeUnproven+1&&
            flareCallbackDepth==2&&!ce_flare::callbacks.load()&&!ce_flare::scope,
            "unreadable or invalid native proof preserves the original draw with both callback pins");
        eyeCamera=provenCamera;flareReadFailure=false;
    }
    for (int32_t player:{-1,1,3})
    {
        std::memcpy(reinterpret_cast<uint8_t*>(&eyeCamera)+0x220,&player,sizeof(player));
        const auto before=flareDraws;Flare(-1);
        Check(flareDraws==before+1,"other source-player identities stay entirely native");
    }
    eyeCamera=provenCamera;
    const auto revokedBefore=flareDraws;flareOwnershipRevoked=true;Flare(-1);
    Check(flareDraws==revokedBefore+1&&!ce_flare::callbacks.load()&&!ce_flare::scope,
        "ownership retired between projection and sprite draw stays native");
    flareOwnershipRevoked=false;ce_flare::active=true;
    for (unsigned reason=0;reason<8;++reason)
    {
        switch(reason)
        {
        case 0:admitted=false;break;
        case 1:testTitle=GameTitle::Halo3;break;
        case 2:ce_flare::retiring=true;break;
        case 3:ce_flare::installed=false;break;
        case 4:testGeneration=6;break;
        case 5:testTracking.generation=6;break;
        case 6:flareForeignRecord=true;break;
        case 7:flareNested=true;break;
        }
        const auto before=flareDraws;Flare(-1);
        Check(flareDraws==before+1&&lastFlareScreen==flareScreen&&
            lastFlareOpaque==0x1122334455667788&&!ce_flare::callbacks.load()&&!ce_flare::scope,
            "unowned, stale, nested, retired and foreign flare draws pass through unchanged");
        admitted=true;testTitle=GameTitle::HaloCE;ce_flare::retiring=false;ce_flare::installed=true;
        testTracking.generation=testGeneration=5;flareForeignRecord=flareNested=false;
    }
    const auto callerBefore=flareDraws;Flare(-1,ce_flare::base+0x45109c);
    Check(flareDraws==callerBefore+1,"unverified flare projection caller stays native");
    Flare(10);const auto preserved=flareEffect;
    Flare(10);
    Check(flareEffect==preserved&&lastFlareRecord==reinterpret_cast<uintptr_t>(flareEffect.data())+0x70,
        "valid flare passes its original native record and never mutates shared effect storage");
    flareThrows=true;
    Check(FlareException()&&flareCallbackDepth==2&&!ce_flare::callbacks.load()&&
        !ce_flare::scope&&ce_flare::exceptions.load()==1,
        "native sprite exception propagates after both optional callback scopes retire");
    Check(FlareHookException(true)&&flareCallbackDepth==2&&!ce_flare::callbacks.load()&&
        !ce_flare::scope&&ce_flare::exceptions.load()==2,
        "actual native projection entry owns and releases its counter on exception");
    Check(FlareHookException(false)&&flareCallbackDepth==1&&!ce_flare::callbacks.load()&&!ce_flare::scope,
        "actual native sprite entry owns and releases its counter on exception");
    flareThrows=false;const auto recovery=flareDraws;Flare(10);
    Check(flareDraws==recovery+1,"visible light flares recover after native exceptions");
    testTracking.disableAnniversaryLensFlares=true;
    const auto suppressedStart=ce_flare::userSuppressed.load();
    const auto enabledDraws=flareDraws;
    for(flareIndex=0;flareIndex<2;++flareIndex)Flare(10);
    Check(flareDraws==enabledDraws&&ce_flare::userSuppressed.load()==suppressedStart+2&&
        !ce_flare::callbacks.load()&&!ce_flare::scope,
        "optional setting suppresses both verified flare records and retires callbacks");
    flareIndex=0;
    for(unsigned reason=0;reason<8;++reason)
    {
        switch(reason){case 0:admitted=false;break;case 1:testTitle=GameTitle::Halo3;break;
        case 2:ce_flare::retiring=true;break;case 3:ce_flare::installed=false;break;
        case 4:testGeneration=6;break;case 5:testTracking.generation=6;break;
        case 6:flareForeignRecord=true;break;case 7:flareNested=true;break;}
        const auto before=flareDraws;Flare(10);
        Check(flareDraws==before+1&&!ce_flare::callbacks.load()&&!ce_flare::scope,
            "toggle never suppresses foreign, stale, unowned, nested or retired effects");
        admitted=true;testTitle=GameTitle::HaloCE;ce_flare::retiring=false;ce_flare::installed=true;
        testTracking.generation=testGeneration=5;flareForeignRecord=flareNested=false;
    }
    testTracking.disableAnniversaryLensFlares=false;
    const auto disabledDraws=flareDraws;Flare(10);
    Check(flareDraws==disabledDraws+1,"switching toggle off restores visible native flares immediately");
    ce_flare::callbacks=1;
    Check(!ce_flare::Remove()&&ce_flare::retiring.load()&&!ce_flare::active.load()&&
        !ce_flare::installed.load()&&ce_flare::projection.original&&ce_flare::draw.original,
        "optional retirement retains native dispatch while an admitted callback remains");
    ce_flare::callbacks=0;
    Check(ce_flare::Remove()&&!ce_flare::retiring.load()&&!ce_flare::base,
        "optional retirement recovers after its callback drains without touching the camera core");
    return failures?1:0;
}
