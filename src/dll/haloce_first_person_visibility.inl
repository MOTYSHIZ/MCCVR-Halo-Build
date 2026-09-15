// Included inside the first-person feature's anonymous namespace.
// Native evidence and executed producer/consumer regression:
// docs/HALOCE-FIRST-PERSON-VISIBILITY-2026-09-15.md.
struct FirstPersonVisibilityRecord
{
    uint32_t player{},model{},nativeModelIndex{},reserved{};
    uintptr_t container{};
    uint32_t state{};
    uint8_t exclusion[4]{};
};
static_assert(sizeof(FirstPersonVisibilityRecord)==0x20&&
    offsetof(FirstPersonVisibilityRecord,container)==0x10);
inline constexpr size_t kNativeVisibilityListBytes=0xbde8;
struct FirstPersonVisibilityList
{
    SaberViewPair primary;
    uint8_t nativeTail[kNativeVisibilityListBytes-sizeof(SaberViewPair)];
};
static_assert(sizeof(FirstPersonVisibilityList)==kNativeVisibilityListBytes);

bool CurrentFirstPersonVisibilityModel(uintptr_t container) noexcept
{
    uint32_t flags{},models[2]{};uintptr_t records{};int32_t count{};
    if (!container||container>UINTPTR_MAX-0x400||!Read(container+4,flags)||
        (flags&0x4400)!=0x4400||!moduleBase||
        !Read(moduleBase+0x1b7aa88,models[0])||!Read(moduleBase+0x1b7aa98,models[1])||
        !Read(moduleBase+0x2b050e8,records)||!Read(moduleBase+0x2b050f0,count)||
        !records||count<=0||count>256||records>UINTPTR_MAX-size_t(count)*sizeof(FirstPersonVisibilityRecord))
        return false;
    // 256 is a safety bound for this optional lookup, not a claimed engine
    // capacity. Native source-player/model identities must match the exact
    // container; first-person flags alone never admit an ordinary world model.
    for (int32_t index=0;index<count;++index)
    {
        FirstPersonVisibilityRecord record{},after{};
        const uintptr_t address=records+size_t(index)*sizeof(record);
        if (!Read(address,record)) return false;
        if (record.container!=container||record.player!=0||!record.model||record.model==0xffffffffu||
            (record.model!=models[0]&&record.model!=models[1])) continue;
        uintptr_t currentRecords{};int32_t currentCount{};uint32_t currentModels[2]{};
        return Read(moduleBase+0x2b050e8,currentRecords)&&currentRecords==records&&
            Read(moduleBase+0x2b050f0,currentCount)&&currentCount==count&&Read(address,after)&&
            after.player==record.player&&after.model==record.model&&after.container==container&&
            Read(moduleBase+0x1b7aa88,currentModels[0])&&Read(moduleBase+0x1b7aa98,currentModels[1])&&
            currentModels[0]==models[0]&&currentModels[1]==models[1];
    }
    return false;
}

// Keep the complete 49KB native snapshot off ordinary world-object stacks.
// No native list/container flags change, including during exceptions. Every
// opaque pointer in the copied list is borrowed for this native call only.
__declspec(noinline) bool RunFirstPersonSourceVisibility(VisibilityPrepareFn original,
    uintptr_t list,int32_t serial,float elapsed,uintptr_t container)
{
    Tracking tracking{},after{};
    if (!HaloCE_GetAnniversaryPreparedListTracking(list,tracking)||
        tracking.controllers.controlsPresentationBlocked||tracking.generation!=generation.load()||
        !list||list>UINTPTR_MAX-kNativeVisibilityListBytes) return false;
    alignas(16) FirstPersonVisibilityList staged;
    if (!Read(list,staged)||staged.primary.count<2||staged.primary.count>kNativeViewCapacity||
        staged.primary.views[0].viewIndex!=0||staged.primary.views[1].viewIndex!=1) return false;
    SaberViewPair current{};
    if (!Read(list,current)||std::memcmp(&current,&staged.primary,sizeof(current))||
        !Current()||!visibilityInstalled.load(std::memory_order_acquire)||
        !CurrentFirstPersonVisibilityModel(container)||
        !HaloCE_GetAnniversaryPreparedListTracking(list,after)||
        after.generation!=tracking.generation||after.spaceEpoch!=tracking.spaceEpoch||
        after.serial!=tracking.serial||after.controllers.controlsPresentationBlocked) return false;
    // The source-player selector controls native FP inclusion/exclusion and
    // LOD decisions. The loop ordinal still publishes distinct eye bits. Keep
    // each eye camera, view flags, every auxiliary view and the full tail exact.
    staged.primary.views[1].viewIndex=staged.primary.views[0].viewIndex;
    original(reinterpret_cast<uintptr_t>(&staged),serial,elapsed,container);
    return true;
}
__declspec(noinline) void __fastcall VisibilityPrepareHook(uintptr_t list,int32_t serial,
    float elapsed,uintptr_t container)
{
    const auto original=reinterpret_cast<VisibilityPrepareFn>(visibilityPrepareHook.original);
    if (!original) return;
    callbacks.fetch_add(1,std::memory_order_acq_rel);
    __try
    {
        if (visibilityInstalled.load(std::memory_order_acquire)&&Current()&&
            CurrentFirstPersonVisibilityModel(container))
        {
            visibilityObserved.fetch_add(1,std::memory_order_relaxed);
            if (RunFirstPersonSourceVisibility(original,list,serial,elapsed,container))
            { visibilityApplied.fetch_add(1,std::memory_order_relaxed);return; }
            visibilityRefused.fetch_add(1,std::memory_order_relaxed);
        }
        original(list,serial,elapsed,container);
    }
    __finally { callbacks.fetch_sub(1,std::memory_order_release); }
}
void __fastcall VisibilitySubmitHook(uintptr_t nativeContext,uintptr_t list,uint8_t phase)
{
    const auto original=reinterpret_cast<VisibilitySubmitFn>(visibilitySubmitHook.original);
    if (!original) return;
    // Native phase one starts workers inside this call. Publish the exact
    // copied-list receipt before entering it, with the caller proving that the
    // native full-list copy has happened. R8B alone is the native phase ABI.
    callbacks.fetch_add(1,std::memory_order_acq_rel);
    __try
    {
        if (visibilityInstalled.load(std::memory_order_acquire)&&Current())
            HaloCE_RecordAnniversaryVisibilitySubmission(list,phase,reinterpret_cast<uintptr_t>(_ReturnAddress()));
        original(nativeContext,list,phase);
    }
    __finally { callbacks.fetch_sub(1,std::memory_order_release); }
}

bool RemoveVisibility() noexcept
{
    visibilityInstalled=false;visibilityRetiring=true;
    for (Hook* hook:{&visibilityPrepareHook,&visibilitySubmitHook})
    {
        if (!hook->target||!hook->enabled) continue;
        const auto result=MCCVR_DisableHookForRetirement(hook->target);
        if (result!=MH_OK&&result!=MH_ERROR_DISABLED) return false;
        hook->enabled=false;
    }
    const void* functions[]={reinterpret_cast<const void*>(&VisibilityPrepareHook),reinterpret_cast<const void*>(&VisibilitySubmitHook)};
    const void* trampolines[]={visibilityPrepareHook.original,visibilitySubmitHook.original};
    if (!WaitForNativeDetourQuiescence(functions,trampolines,2,callbacks)) return false;
    for (Hook* hook:{&visibilityPrepareHook,&visibilitySubmitHook})
    {
        if (hook->target&&MH_RemoveHook(hook->target)!=MH_OK) return false;
        *hook={};
    }
    visibilityRetiring=false;return true;
}
bool InstallVisibility(uintptr_t base,size_t size,uint32_t gen) noexcept
{
    const char* failure{};
    const NativeContractSet contracts{contract::first_person_visibility::entries,
        contract::first_person_visibility::witnesses,contract::first_person_visibility::relatives,
        contract::first_person_visibility::pointers};
    if (!VerifyNativeFeatureBindings(base,size,gen,contracts,failure))
    { LOG("CE Anniversary FP visibility stock fallback: binding verification failed: %s",failure?failure:"unknown");return false; }
    struct Entry { Hook* hook;uint32_t rva;void* detour; };
    const Entry entries[]={
        {&visibilityPrepareHook,contract::first_person_visibility::first_person_visibility_prepare,reinterpret_cast<void*>(&VisibilityPrepareHook)},
        {&visibilitySubmitHook,contract::first_person_visibility::first_person_visibility_submit,reinterpret_cast<void*>(&VisibilitySubmitHook)}};
    for (const auto& entry:entries)
    {
        void* target=reinterpret_cast<void*>(base+entry.rva);
        const auto result=MH_CreateHook(target,entry.detour,&entry.hook->original);
        if (result!=MH_OK)
        { LOG("CE Anniversary FP visibility stock fallback: create hook %x status %d",entry.rva,result);(void)RemoveVisibility();return false; }
        entry.hook->target=target;
    }
    for (const auto& entry:entries)
    {
        const auto result=MH_EnableHook(entry.hook->target);
        if (result!=MH_OK)
        { LOG("CE Anniversary FP visibility stock fallback: enable hook %x status %d",entry.rva,result);(void)RemoveVisibility();return false; }
        entry.hook->enabled=true;
    }
    visibilityInstalled=true;
    LOG("CE Anniversary FP visibility installed: owned synthetic eyes share native source-player zero visibility through a private full-list snapshot; hidden models and world lists stay native");
    return true;
}
