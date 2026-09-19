#include <Windows.h>
#include <MinHook.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "../src/dll/haloce_native_bindings.h"
using namespace halo_ce;
struct Hook{void* target{},*original{};bool enabled{};};
static Hook muzzleFireHook,muzzleMarkersHook,muzzleQueryHook,muzzleDirectQueryHook;
static std::atomic<bool> installed{true},aimInstalled{true},targetInstalled{true},muzzleInstalled{},muzzleFaulted{};
static std::atomic<uint32_t> callbacks{};
static bool muzzleRetiring{};static uint64_t muzzleInstalledAt{};
static void MuzzleFireHook(){} static void MuzzleMarkersHook(){} static void MuzzleQueryHook(){} static void MuzzleDirectQueryHook(){}
static constexpr uintptr_t base=0x180000000;
static constexpr uintptr_t rvas[]{0xB7A374,0xB3701C,0xB67FA8,0xB68284};
static unsigned checks{},created{},removed{},logged{};
static int failCreate=-1,failEnable=-1,failDisable=-1,failRemove=-1;
static bool verification=true,ingress{},drained{},exists[4]{},enabled[4]{};
static void Check(bool ok,const char* why)
{++checks;if(!ok){std::fprintf(stderr,"CE muzzle lifecycle: %s\n",why);std::exit(1);}}
namespace halo_ce {
bool VerifyNativeFeatureBindings(uintptr_t image,size_t size,uint32_t gen,const NativeContractSet& set,const char*& failure) noexcept
{
    Check(image==base&&size==contract::imageSize&&gen==7,"exact CE image/generation forwarded");
    Check(set.entries.data()==contract::muzzle::entries.data()&&set.witnesses.data()==contract::muzzle::witnesses.data()&&
        set.relatives.data()==contract::muzzle::relatives.data(),"only own CE muzzle contracts authorize hooks");
    failure="fixture missing/ambiguous witness";return verification;
}}
static int Index(void* target)
{for(int i=0;i<4;++i)if(reinterpret_cast<uintptr_t>(target)==base+rvas[i])return i;Check(false,"unverified target");return -1;}
static MH_STATUS Create(void* target,void* detour,void** original)
{
    const int i=Index(target);++created;const void* expected[]{&MuzzleFireHook,&MuzzleMarkersHook,&MuzzleQueryHook,&MuzzleDirectQueryHook};
    Check(detour==expected[i]&&original,"exact hook order and original destination");
    if(i==failCreate)return MH_ERROR_MEMORY_ALLOC;
    exists[i]=true;*original=reinterpret_cast<void*>(uintptr_t(0x10000+i*0x100));return MH_OK;
}
static MH_STATUS Enable(void* target)
{const int i=Index(target);Check(exists[i],"create before enable");if(i==failEnable)return MH_ERROR_MEMORY_PROTECT;enabled[i]=true;return MH_OK;}
static MH_STATUS MCCVR_DisableHookForRetirement(void* target)
{const int i=Index(target);if(i==failDisable)return MH_ERROR_MEMORY_PROTECT;enabled[i]=false;return MH_OK;}
static MH_STATUS RemoveHook(void* target)
{
    const int i=Index(target);++removed;
    Check(drained&&!ingress&&!callbacks&&!enabled[i],"never remove a live callback/trampoline");
    if(i==failRemove)return MH_ERROR_MEMORY_PROTECT;exists[i]=false;return MH_OK;
}
static bool WaitForNativeDetourQuiescence(const void* const* functions,const void* const* originals,size_t count,const std::atomic<uint32_t>& active)
{
    Check(count==4,"all four optional CE entries drained together");
    const void* expected[]{&MuzzleFireHook,&MuzzleMarkersHook,&MuzzleQueryHook,&MuzzleDirectQueryHook};
    for(int i=0;i<4;++i)Check(functions[i]==expected[i]&&(!exists[i]||originals[i]==reinterpret_cast<void*>(uintptr_t(0x10000+i*0x100))),"exact live trampoline retained through quiescence");
    drained=!active.load()&&!ingress;return drained;
}
#define MH_CreateHook Create
#define MH_EnableHook Enable
#define MH_RemoveHook RemoveHook
#define LOG(...) (++logged)
#include "../src/dll/haloce_muzzle_lifecycle.inl"
#undef MH_CreateHook
#undef MH_EnableHook
#undef MH_RemoveHook
#undef LOG
static void Reset()
{
    muzzleFireHook=muzzleMarkersHook=muzzleQueryHook=muzzleDirectQueryHook={};
    installed=aimInstalled=targetInstalled=true;muzzleInstalled=muzzleFaulted=false;muzzleRetiring=false;
    verification=true;ingress=drained=false;callbacks=0;created=removed=0;
    failCreate=failEnable=failDisable=failRemove=-1;
    std::memset(exists,0,sizeof(exists));std::memset(enabled,0,sizeof(enabled));
}
static bool Install(){return InstallMuzzle(base,contract::imageSize,7);}
int main()
{
    Reset();verification=false;Check(!Install()&&!created&&installed&&aimInstalled,"failed native proof isolates barrel feature");
    for(int dependency=0;dependency<3;++dependency)
    {
        Reset();if(dependency==0)installed=false;else if(dependency==1)aimInstalled=false;else targetInstalled=false;
        Check(!Install()&&!created,"barrel waits for actual palette/aim/acquisition dependencies");
    }
    for(int i=0;i<4;++i)
    {
        Reset();failCreate=i;Check(!Install()&&!muzzleInstalled&&installed&&aimInstalled,"partial create remains feature-local");
        Check(RemoveMuzzle(),"partial create cleanup");
        Reset();failEnable=i;Check(!Install()&&!muzzleInstalled&&installed&&targetInstalled,"partial enable remains feature-local");
        Check(RemoveMuzzle(),"partial enable cleanup");
        Reset();Check(Install(),"complete native contract enables feature");failDisable=i;
        Check(!RemoveMuzzle()&&!removed&&muzzleRetiring&&!muzzleInstalled,"disable failure retains dependencies");
        failDisable=-1;Check(RemoveMuzzle()&&!muzzleRetiring,"disable retry succeeds");
        Reset();Check(Install(),"installed");failRemove=i;
        Check(!RemoveMuzzle()&&muzzleRetiring,"removal failure remains retryable");
        failRemove=-1;Check(RemoveMuzzle()&&!muzzleRetiring,"partial removal retry succeeds");
    }
    Reset();Check(Install(),"installed before ingress check");ingress=true;
    Check(!RemoveMuzzle()&&!removed,"zero count with thread in ingress retains dependencies");
    ingress=false;callbacks=1;Check(!RemoveMuzzle()&&!removed,"live fire lease retains dependencies");
    callbacks=0;Check(RemoveMuzzle()&&removed==4&&!muzzleFireHook.original&&!muzzleRetiring,"complete drain clears exact originals");
    std::printf("PASS: %u production CE muzzle lifecycle checks\n",checks);
}
