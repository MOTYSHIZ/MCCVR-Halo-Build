#include "../src/dll/haloce_comfort.cpp"
#include <cstdio>

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
    return failures?1:0;
}
