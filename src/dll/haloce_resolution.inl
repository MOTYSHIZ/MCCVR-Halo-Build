// E-CE-RES-1. Included inside the CE core's private namespace. Only Saber
// pool allocations change; Original's native render targets and the backend
// render/HUD dimensions remain native. No allocation is done by an eye hook.
namespace ce_resolution
{
using InitializeFn=uintptr_t(__fastcall*)(uintptr_t);
using EntryFn=uintptr_t(__fastcall*)(uintptr_t,uintptr_t,uint64_t,int32_t,int32_t,uint32_t,uint32_t,uintptr_t);
using ChildFn=uintptr_t(__fastcall*)(uintptr_t,uintptr_t,int32_t,int32_t,uint64_t,uint64_t);
using ManageFn=void(__fastcall*)(uint64_t,uint64_t,uint64_t,uint64_t);
using ReleaseFn=void(__fastcall*)();
using ReconfigureFn=uintptr_t(__fastcall*)(int32_t,uint8_t);
enum Index { Initialize,Entry,Child,Manage,HookCount };
std::array<Hook,HookCount> hooks;
std::atomic<bool> enabled{};
std::atomic<uint32_t> callbacks{};
std::atomic<uint64_t> rebuilds{},failures{},children{},packedAllocations{};
std::atomic<uint64_t> retryAtMs{};
std::atomic<uint32_t> lastFailure{},requestedWidth{},requestedHeight{};
struct Pool { uintptr_t address{};uint32_t generation{},width{},height{};bool full{}; };
Snapshot<Pool> completed;
thread_local Pool initializing;
struct EntryAllocation { uintptr_t pool{};int32_t index{};uint32_t usage{}; };
thread_local EntryAllocation allocating;
thread_local bool managing{};
ReleaseFn release{};
ReconfigureFn reconfigure{};

bool Dimensions(uint32_t& width,uint32_t& height) noexcept
{
    uintptr_t backend{},config{};
    return Read(bindings.base+0x2e3bdd8,backend)&&backend&&
        Read(backend+0x118,config)&&config&&Read(config+0x20,width)&&Read(config+0x24,height)&&
        width>0&&width<=16384&&height>0&&height<=8192;
}
bool Desired() noexcept
{
    return enabled.load(std::memory_order_acquire)&&Current()&&Anniversary();
}
bool PoolMember(uintptr_t parent,Pool& pool) noexcept
{
    pool=initializing;
    if (!pool.address&&!completed.Read(pool)) return false;
    if (!pool.full||pool.generation!=generation.load(std::memory_order_acquire)) return false;
    uintptr_t live{};int32_t count{};
    if (!Read(bindings.base+0x1bea8a0,live)||live!=pool.address||
        !Read(live+0xc,count)||count<0||count>160) return false;
    // This is the native allocation path, outside rendering. Membership keeps
    // shadow maps, imported textures and arbitrary split surfaces native.
    for (int32_t i=0;i<count;++i)
    {
        uintptr_t root{};uint32_t usage{};
        if (!Read(live+0x10+size_t(i)*0x38,root)||!Read(live+0x18+size_t(i)*0x38,usage)) return false;
        if (root==parent) return (usage&7u)!=0&&(usage&0x4000000u)==0;
    }
    // +20a9b0 installs pool[count].root before calling its +a8 initializer,
    // which constructs both split children. Usage and count are published
    // only after that initializer returns. Admit precisely this in-flight
    // native slot using the entry's scoped arguments, never an arbitrary root.
    if (initializing.address==live&&allocating.pool==live&&allocating.index==count&&count<160&&
        (allocating.usage&7u)!=0&&(allocating.usage&0x4000000u)==0)
    {
        uintptr_t root{};
        return Read(live+0x10+size_t(count)*0x38,root)&&root==parent;
    }
    return false;
}
uintptr_t InitializeBody(uintptr_t pool)
{
    const Pool previous=initializing;
    uint32_t width{},height{};uintptr_t nativePool{};
    const bool full=Desired()&&Dimensions(width,height)&&
        Read(bindings.base+0x1bea8a0,nativePool)&&nativePool==pool;
    initializing={pool,generation.load(),width,height,full};
    uintptr_t result{};
    __try
    {
        result=reinterpret_cast<InitializeFn>(hooks[Initialize].original)(pool);
        if (result) completed.Publish(initializing);
        else { completed.Publish({});failures.fetch_add(1);lastFailure=1; }
    }
    __finally { initializing=previous; }
    return result;
}
uintptr_t __fastcall InitializeHook(uintptr_t pool)
{
    callbacks.fetch_add(1,std::memory_order_acq_rel);
    uintptr_t result{};
    __try { result=InitializeBody(pool); }
    __finally { callbacks.fetch_sub(1,std::memory_order_release); }
    return result;
}
uintptr_t __fastcall EntryHook(uintptr_t pool,uintptr_t name,uint64_t flags,
    int32_t width,int32_t height,uint32_t format,uint32_t usage,uintptr_t backing)
{
    callbacks.fetch_add(1,std::memory_order_acq_rel);
    const EntryAllocation previous=allocating;
    uintptr_t result{};
    __try
    {
        int32_t index{};
        allocating={};
        if (initializing.full&&initializing.address==pool&&Read(pool+0xc,index)&&index>=0&&index<160)
            allocating={pool,index,usage};
        // Native pool roles 04000001/24000001 are unsplit packed outputs.
        // Preserve allocation flags, formats, ownership and lookup roles.
        if (initializing.full&&initializing.address==pool&&width==0&&height==0&&
            (usage==0x04000001u||usage==0x24000001u))
        {
            width=static_cast<int32_t>(initializing.width);
            height=static_cast<int32_t>(initializing.height*2);
            packedAllocations.fetch_add(1,std::memory_order_relaxed);
        }
        result=reinterpret_cast<EntryFn>(hooks[Entry].original)(pool,name,flags,width,height,format,usage,backing);
    }
    __finally { allocating=previous;callbacks.fetch_sub(1,std::memory_order_release); }
    return result;
}
uintptr_t __fastcall ChildHook(uintptr_t parent,uintptr_t name,int32_t width,
    int32_t height,uint64_t opaque,uint64_t selector)
{
    callbacks.fetch_add(1,std::memory_order_acq_rel);
    uintptr_t result{};
    __try
    {
        Pool pool{};int16_t nativeWidth{},nativeHeight{};uint32_t flags{};
        if (enabled.load(std::memory_order_acquire)&&PoolMember(parent,pool)&&
            Read(parent+0x10,nativeWidth)&&Read(parent+0x12,nativeHeight)&&
            Read(parent+0x88,flags)&&(flags&0x10u)&&width==nativeWidth&&nativeHeight>0&&
            height==nativeHeight/2&&nativeHeight<=8192)
        {
            // Use the parent's full authored height, including odd heights.
            // Multiplying the truncated native half height loses the last row.
            height=nativeHeight;
            children.fetch_add(1,std::memory_order_relaxed);
        }
        result=reinterpret_cast<ChildFn>(hooks[Child].original)(parent,name,width,height,opaque,selector);
    }
    __finally { callbacks.fetch_sub(1,std::memory_order_release); }
    return result;
}
void ManageBody(uint64_t a,uint64_t b,uint64_t c,uint64_t d)
{
    // +455170 invokes this before camera preparation/work submission. The
    // native release drains its outstanding job, then reconfigure recreates
    // native resources. Never run this from Present, Poll, or an eye callback.
    reinterpret_cast<ManageFn>(hooks[Manage].original)(a,b,c,d);
    uint32_t width{},height{};uintptr_t pool{};Pool ready{};Tracking tracking{};
    if (managing||!Desired()||!TrackingNow(tracking)||!Dimensions(width,height)||
        !Read(bindings.base+0x1bea8a0,pool)||!pool||!release||!reconfigure) return;
    requestedWidth=width;requestedHeight=height;
    if (completed.Read(ready)&&ready.full&&ready.address==pool&&ready.generation==generation.load()&&
        ready.width==width&&ready.height==height) return;
    const uint64_t now=GetTickCount64();
    if (now<retryAtMs.load(std::memory_order_acquire)) return;
    retryAtMs.store(now+1000,std::memory_order_release);
    managing=true;
    __try
    {
        RevokeCopiedWorkerList();completedFrame.Publish({});renderReady.Publish({});wanted.Publish({});
        handoff.Invalidate(PreparationOrigin::ActiveList);handoff.Invalidate(PreparationOrigin::CopiedList);
        preparedLists[0].Publish({});preparedLists[1].Publish({});completed.Publish({});
        release();
        if (reconfigure(0,0)&&completed.Read(ready)&&ready.full&&ready.address==pool&&
            ready.generation==generation.load()&&ready.width==width&&ready.height==height)
        { rebuilds.fetch_add(1);lastFailure=0;retryAtMs=0; }
        else { failures.fetch_add(1);lastFailure=2; }
    }
    __finally { managing=false; }
}
void __fastcall ManageHook(uint64_t a,uint64_t b,uint64_t c,uint64_t d)
{
    callbacks.fetch_add(1,std::memory_order_acq_rel);
    __try { ManageBody(a,b,c,d); }
    __finally { callbacks.fetch_sub(1,std::memory_order_release); }
}
const void* functions[HookCount]={reinterpret_cast<void*>(&InitializeHook),reinterpret_cast<void*>(&EntryHook),
    reinterpret_cast<void*>(&ChildHook),reinterpret_cast<void*>(&ManageHook)};
bool Remove() noexcept
{
    enabled.store(false,std::memory_order_release);
    for (auto& hook:hooks) if (hook.enabled)
    {
        const auto status=MCCVR_DisableHookForRetirement(hook.target);
        if (status!=MH_OK&&status!=MH_ERROR_DISABLED) return false;
        hook.enabled=false;
    }
    const void* originals[HookCount]{};
    for (size_t i=0;i<HookCount;++i) originals[i]=hooks[i].original;
    if (callbacks.load()||!WaitForNativeDetourQuiescence(functions,originals,HookCount,callbacks)) return false;
    for (auto& hook:hooks) if (hook.target)
    {
        const auto status=MH_RemoveHook(hook.target);
        if (status!=MH_OK&&status!=MH_ERROR_NOT_CREATED) return false;
        hook={};
    }
    completed.Publish({});release=nullptr;reconfigure=nullptr;retryAtMs=0;
    return true;
}
bool Install() noexcept
{
    using namespace contract::anniversary_resolution;
    const NativeContractSet contracts{entries,witnesses,relatives,pointers};
    const char* failure{};
    if (!VerifyNativeFeatureBindings(bindings.base,bindings.size,generation.load(),contracts,failure))
    { LOG("CE full resolution stock fallback: %s; existing camera retained",failure?failure:"contracts");return false; }
    const uint32_t addresses[HookCount]={resolution_pool_initialize,resolution_pool_entry,
        resolution_split_child,resolution_management};
    for (size_t i=0;i<HookCount;++i)
    {
        auto& hook=hooks[i];auto* target=reinterpret_cast<void*>(bindings.base+addresses[i]);
        if (MH_CreateHook(target,const_cast<void*>(functions[i]),&hook.original)!=MH_OK)
        {
            if (!Remove()) LOG("CE resolution partial installation retained for callback drain; camera retained");
            return false;
        }
        hook.target=target;
    }
    release=reinterpret_cast<ReleaseFn>(bindings.base+resolution_release);
    reconfigure=reinterpret_cast<ReconfigureFn>(bindings.base+resolution_reconfigure);
    enabled=true;
    for (auto& hook:hooks)
    {
        if (MH_EnableHook(hook.target)!=MH_OK)
        {
            if (!Remove()) LOG("CE resolution partial activation retained for callback drain; camera retained");
            return false;
        }
        hook.enabled=true;
    }
    return true;
}
}
