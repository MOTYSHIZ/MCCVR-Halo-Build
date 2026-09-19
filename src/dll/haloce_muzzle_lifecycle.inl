bool RemoveMuzzle() noexcept
{
    muzzleInstalled=false;muzzleRetiring=true;bool any=false;
    for(Hook* hook:{&muzzleFireHook,&muzzleMarkersHook,&muzzleQueryHook,&muzzleDirectQueryHook})
    {
        if(!hook->target)continue;any=true;if(!hook->enabled)continue;
        const auto status=MCCVR_DisableHookForRetirement(hook->target);
        if(status!=MH_OK&&status!=MH_ERROR_DISABLED&&status!=MH_ERROR_NOT_CREATED)
        {LOG("CE barrel CleanupRequired: disable failed");return false;}
        hook->enabled=false;
    }
    if(!any){muzzleRetiring=false;return true;}
    const void* functions[]{reinterpret_cast<const void*>(&MuzzleFireHook),reinterpret_cast<const void*>(&MuzzleMarkersHook),
        reinterpret_cast<const void*>(&MuzzleQueryHook),reinterpret_cast<const void*>(&MuzzleDirectQueryHook)};
    const void* originals[]{muzzleFireHook.original,muzzleMarkersHook.original,muzzleQueryHook.original,muzzleDirectQueryHook.original};
    if(!WaitForNativeDetourQuiescence(functions,originals,4,callbacks))
    {LOG("CE barrel CleanupRequired: callbacks or ingress busy");return false;}
    for(Hook* hook:{&muzzleFireHook,&muzzleMarkersHook,&muzzleQueryHook,&muzzleDirectQueryHook})
    {
        if(!hook->target)continue;
        const auto status=MH_RemoveHook(hook->target);
        if(status!=MH_OK&&status!=MH_ERROR_NOT_CREATED)
        {LOG("CE barrel CleanupRequired: removal failed");return false;}
        *hook={};
    }
    muzzleRetiring=false;return true;
}
bool InstallMuzzle(uintptr_t base,size_t size,uint32_t gen) noexcept
{
    for(Hook* hook:{&muzzleFireHook,&muzzleMarkersHook,&muzzleQueryHook,&muzzleDirectQueryHook})
        if(hook->target)return false;
    if(!installed.load()||!aimInstalled.load()||!targetInstalled.load())return false;
    const char* failure{};
    const NativeContractSet contracts{contract::muzzle::entries,contract::muzzle::witnesses,
        contract::muzzle::relatives,contract::muzzle::pointers};
    if(!VerifyNativeFeatureBindings(base,size,gen,contracts,failure))
    {LOG("CE barrel StockFallback: binding verification failed: %s",failure?failure:"unknown");return false;}
    const uintptr_t addresses[]{base+contract::muzzle::muzzle_fire,base+contract::muzzle::muzzle_markers,
        base+contract::muzzle::muzzle_query,base+contract::muzzle::muzzle_direct_query};
    void* detours[]{reinterpret_cast<void*>(&MuzzleFireHook),reinterpret_cast<void*>(&MuzzleMarkersHook),
        reinterpret_cast<void*>(&MuzzleQueryHook),reinterpret_cast<void*>(&MuzzleDirectQueryHook)};
    Hook* hooks[]{&muzzleFireHook,&muzzleMarkersHook,&muzzleQueryHook,&muzzleDirectQueryHook};
    for(size_t i=0;i<4;++i)
    {
        void* target=reinterpret_cast<void*>(addresses[i]);
        if(MH_CreateHook(target,detours[i],&hooks[i]->original)!=MH_OK)
        {LOG("CE barrel StockFallback: create failed at %zu",i);(void)RemoveMuzzle();return false;}
        hooks[i]->target=target;
    }
    for(Hook* hook:hooks)
    {
        if(MH_EnableHook(hook->target)!=MH_OK)
        {LOG("CE barrel StockFallback: enable failed");(void)RemoveMuzzle();return false;}
        hook->enabled=true;
    }
    muzzleInstalledAt=GetTickCount64();muzzleFaulted=false;muzzleInstalled=true;
    LOG("CE barrel Installed: optional authored committed muzzle, native acquisition and obstruction");return true;
}
