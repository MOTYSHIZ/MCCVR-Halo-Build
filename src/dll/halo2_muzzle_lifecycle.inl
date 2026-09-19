bool RemoveHalo2Muzzle()
{
    auto& feature=g_halo2Muzzle;
    feature.enabled.store(false,std::memory_order_release);
    if(!feature.target)return true;
    const auto disabled=MCCVR_DisableHookForRetirement(feature.target);
    if(disabled!=MH_OK&&disabled!=MH_ERROR_DISABLED&&disabled!=MH_ERROR_NOT_CREATED)
    {LOG("Halo 2 muzzle CleanupRequired: disable failed");return false;}
    const void* functions[]{reinterpret_cast<const void*>(&Halo2MuzzleMarkersDetour)};
    const void* originals[]{reinterpret_cast<const void*>(feature.original)};
    if(!WaitForNativeDetourQuiescence(functions,originals,1,feature.callbacks))
    {LOG("Halo 2 muzzle CleanupRequired: callbacks or ingress busy");return false;}
    const auto removed=MH_RemoveHook(feature.target);
    if(removed!=MH_OK&&removed!=MH_ERROR_NOT_CREATED)
    {LOG("Halo 2 muzzle CleanupRequired: removal failed");return false;}
    feature.target=nullptr;feature.original=nullptr;return true;
}

bool InstallHalo2Muzzle(uintptr_t base,size_t size)
{
    auto& feature=g_halo2Muzzle;
    if(feature.target||!g_halo2Dual.enabled.load()||size<=0x8E4BAF)
    {LOG("Halo 2 muzzle StockFallback: firing transaction unavailable or cleanup pending");return false;}
    struct Binding {uint32_t rva;const char* pattern;};
    constexpr Binding bindings[]{
        {0x8D6570,"48 83 EC 38 C6 44 24 28 00 C6 44 24 20 00 E8 1D AC FF FF 48 83 C4 38 C3"},
        {0x8D11A0,"48 89 5C 24 10 66 44 89 4C 24 20 55 56 57 41 55 41 56 48 83 EC 60 33 F6 44 8B F1 49 8B D8 44 8B EA 8B F9 0F B7 EE"}};
    for(const auto& binding:bindings)
    {
        uintptr_t match=0;uint32_t count=0;
        if(!CountPatternMatches(base,size,binding.pattern,match,count)||count!=1||match!=base+binding.rva)
        {LOG("Halo 2 muzzle StockFallback: missing/ambiguous marker binding +%X",binding.rva);return false;}
    }
    struct Edge {uint32_t call,target;};
    constexpr Edge edges[]{{0x8E4BAA,0x8D6570},{0x8D657E,0x8D11A0}};
    for(const auto& edge:edges)
    {
        const auto* bytes=reinterpret_cast<const uint8_t*>(base+edge.call);
        if(bytes[0]!=0xE8||base+edge.call+5+*reinterpret_cast<const int32_t*>(bytes+1)!=base+edge.target)
        {LOG("Halo 2 muzzle StockFallback: native marker edge changed +%X",edge.call);return false;}
    }
    feature.base=base;feature.generation=g_halo2Dual.generation;feature.faulted.store(false);
    void* target=reinterpret_cast<void*>(base+0x8D6570);
    if(MH_CreateHook(target,reinterpret_cast<void*>(&Halo2MuzzleMarkersDetour),
        reinterpret_cast<void**>(&feature.original))!=MH_OK)
    {LOG("Halo 2 muzzle StockFallback: marker hook creation failed");return false;}
    feature.target=target;
    if(MH_EnableHook(target)!=MH_OK)
    {LOG("Halo 2 muzzle StockFallback: marker hook enable failed");(void)RemoveHalo2Muzzle();return false;}
    feature.enabled.store(true,std::memory_order_release);
    LOG("Halo 2 muzzle Installed: optional committed FP marker, native targeting/obstruction; option=%d",g_config.gun_barrel_aim?1:0);
    return true;
}
