// Exercise the actual optional installer/retirement state machine, including
// zero-counter threads paused before entry accounting. Native byte identity is
// checked independently by tools/verify-halo2-dual-bindings.py.
static uintptr_t lifecycleBase{};
static unsigned bindingScan{},createCalls{},enableCalls{},removeCalls{},logged{};
static int missingBinding=-1,ambiguousBinding=-1,failCreate=-1,failEnable=-1,
    failDisable=-1,failRemove=-1;
static bool ingress=false,quiesced=false;
static bool hookExists[4]{},hookEnabled[4]{};
static constexpr uint32_t hookRvas[]{0x8E4940,0x8F0F70,0x6F0E60,0x6D4730};
static bool CountPatternMatches(uintptr_t base,size_t,const char*,uintptr_t& match,uint32_t& count)
{
    constexpr uint32_t bindings[]{0x8E4940,0x8F0F70,0x6F0E60,0x6D4730,0x7596A0};
    const unsigned number=bindingScan++;
    Check(number<5,"bounded H2 cold binding inventory");
    match=base+bindings[number];count=int(number)==missingBinding?0:int(number)==ambiguousBinding?2:1;
    return true;
}
static int HookIndex(void* target)
{
    for(int i=0;i<4;++i)if(reinterpret_cast<uintptr_t>(target)==lifecycleBase+hookRvas[i])return i;
    Check(false,"only known optional hooks");return -1;
}
static MH_STATUS FixtureCreate(void* target,void* detour,void** original)
{
    const int i=HookIndex(target);++createCalls;
    const void* expected[]{reinterpret_cast<void*>(&Halo2IndependentFireDetour),reinterpret_cast<void*>(&Halo2IndependentAimDetour),
        reinterpret_cast<void*>(&Halo2IndependentLocationDetour),reinterpret_cast<void*>(&Halo2IndependentCameraDetour)};
    Check(detour==expected[i]&&original,"correct production hook entry");
    if(i==failCreate)return MH_ERROR_MEMORY_ALLOC;
    hookExists[i]=true;*original=reinterpret_cast<void*>(uintptr_t(0x4000+i*0x100));return MH_OK;
}
static MH_STATUS FixtureEnable(void* target)
{
    const int i=HookIndex(target);++enableCalls;Check(hookExists[i],"enable created entry only");
    if(i==failEnable)return MH_ERROR_MEMORY_PROTECT;
    hookEnabled[i]=true;return MH_OK;
}
static MH_STATUS MCCVR_DisableHookForRetirement(void* target)
{
    const int i=HookIndex(target);
    if(i==failDisable)return MH_ERROR_MEMORY_PROTECT;
    if(!hookExists[i])return MH_ERROR_NOT_CREATED;
    const auto status=hookEnabled[i]?MH_OK:MH_ERROR_DISABLED;hookEnabled[i]=false;return status;
}
static MH_STATUS FixtureRemove(void* target)
{
    const int i=HookIndex(target);++removeCalls;
    Check(quiesced&&!hookEnabled[i]&&!g_halo2Dual.callbacks.load(),"never free busy native entry");
    if(i==failRemove)return MH_ERROR_MEMORY_PROTECT;
    const auto status=hookExists[i]?MH_OK:MH_ERROR_NOT_CREATED;hookExists[i]=false;return status;
}
static bool WaitForNativeDetourQuiescence(const void* const* functions,const void* const* original,
    size_t count,const std::atomic<uint32_t>& callbacks)
{
    const void* expected[]{reinterpret_cast<void*>(&Halo2IndependentFireDetour),reinterpret_cast<void*>(&Halo2IndependentAimDetour),
        reinterpret_cast<void*>(&Halo2IndependentLocationDetour),reinterpret_cast<void*>(&Halo2IndependentCameraDetour)};
    Check(count==4,"all four native entries included");
    for(size_t i=0;i<count;++i)
    {
        Check(functions[i]==expected[i],"exact detour range");
        if(i<4)
        {
            Check(!hookEnabled[i],"all hooks disabled before quiescence");
            Check(original[i]==(hookExists[i]?reinterpret_cast<void*>(0x4000+i*0x100):nullptr),"no retired trampoline retained on retry");
        }
    }
    quiesced=!callbacks.load()&&!ingress;return quiesced;
}
#define MH_CreateHook FixtureCreate
#define MH_EnableHook FixtureEnable
#define MH_RemoveHook FixtureRemove
#define LOG(...) (++logged)
bool RemoveHalo2DualAim();
static bool InstallHalo2Muzzle(uintptr_t,size_t){return false;}
static bool RemoveHalo2Muzzle(){return true;}
#include "../src/dll/halo2_dual_lifecycle.inl"
#undef LOG
#undef MH_CreateHook
#undef MH_EnableHook
#undef MH_RemoveHook

static void LifecycleTests()
{
    constexpr size_t size=0x8F2000;
    auto* image=static_cast<unsigned char*>(VirtualAlloc(nullptr,size,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    Check(image!=nullptr,"private lifecycle image");lifecycleBase=reinterpret_cast<uintptr_t>(image);
    auto edge=[&](uint32_t call,uint32_t target){image[call]=0xE8;*reinterpret_cast<int32_t*>(image+call+1)=int32_t(target-call-5);};
    edge(0x8E4FC8,0x8F0F70);edge(0x8E527C,0x7596A0);edge(0x759325,0x6F0E60);
    edge(0x75934C,0x6C0DF0);edge(0x7597B8,0x6D4730);edge(0x6C2987,0x759260);
    const unsigned char field[]{0x49,0x8D,0xB7,0xD4,0x01,0,0},parent[]{0x41,0x8B,0x87,0x60,0x02,0,0};
    std::memcpy(image+0x8E4ED0,field,sizeof(field));std::memcpy(image+0x8E4EE1,parent,sizeof(parent));
    auto reset=[&]{
        bindingScan=createCalls=enableCalls=removeCalls=0;quiesced=ingress=false;
        missingBinding=ambiguousBinding=failCreate=failEnable=failDisable=failRemove=-1;
        std::memset(hookExists,0,sizeof(hookExists));std::memset(hookEnabled,0,sizeof(hookEnabled));
        g_halo2Dual.fireTarget=g_halo2Dual.aimTarget=g_halo2Dual.locationTarget=g_halo2Dual.cameraTarget=nullptr;
        g_halo2Dual.fireOriginal=nullptr;g_halo2Dual.aimOriginal=nullptr;g_halo2Dual.locationOriginal=nullptr;g_halo2Dual.cameraOriginal=nullptr;
        g_halo2Dual.enabled=false;g_halo2Dual.callbacks=0;
    };
    for(int i=0;i<5;++i)
    {
        reset();missingBinding=i;Check(!InstallHalo2DualAim(lifecycleBase,size)&&createCalls==0,"missing native binding stays stock");
        reset();ambiguousBinding=i;Check(!InstallHalo2DualAim(lifecycleBase,size)&&createCalls==0,"ambiguous native binding stays stock");
    }
    for(int i=0;i<4;++i)
    {
        reset();failCreate=i;Check(!InstallHalo2DualAim(lifecycleBase,size)&&!g_halo2Dual.enabled,"partial creation isolated");
        Check(RemoveHalo2DualAim(),"partial creation fully retired");
        reset();failEnable=i;Check(!InstallHalo2DualAim(lifecycleBase,size)&&!g_halo2Dual.enabled,"partial enable isolated");
        Check(RemoveHalo2DualAim(),"partial enable fully retired");
        reset();Check(InstallHalo2DualAim(lifecycleBase,size),"fixture installed");failDisable=i;
        Check(!RemoveHalo2DualAim()&&removeCalls==0,"failed disable retains all trampolines");
        failDisable=-1;Check(RemoveHalo2DualAim(),"disable cleanup retry succeeds");
        reset();Check(InstallHalo2DualAim(lifecycleBase,size),"fixture installed");failRemove=i;
        Check(!RemoveHalo2DualAim(),"failed removal reported");failRemove=-1;
        Check(RemoveHalo2DualAim(),"partial removal retry has no stale originals");
    }
    reset();Check(InstallHalo2DualAim(lifecycleBase,size),"fixture installed");ingress=true;
    Check(!RemoveHalo2DualAim()&&removeCalls==0,"zero counter with live ingress retains code");
    ingress=false;g_halo2Dual.callbacks=1;Check(!RemoveHalo2DualAim()&&removeCalls==0,"live callback retains code");
    g_halo2Dual.callbacks=0;Check(RemoveHalo2DualAim(),"drained cleanup succeeds");
    reset();image[0x8E4EE1]^=1;Check(!InstallHalo2DualAim(lifecycleBase,size)&&createCalls==0,"changed controlling-parent field blocks only feature");
    image[0x8E4EE1]^=1;reset();image[0x7597B8]^=1;
    Check(!InstallHalo2DualAim(lifecycleBase,size)&&createCalls==0,"changed downstream consumer blocks only feature");
    Check(VirtualFree(image,0,MEM_RELEASE)!=0,"private lifecycle image released");
}
