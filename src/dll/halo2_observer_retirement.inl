// Worker-only retirement. Disable every entry before releasing any trampoline;
// callback counts alone do not cover threads stopped in pre-counter ingress.
struct Halo2RetirementHook
{
    void** target;
    std::atomic<uintptr_t>* original;
    const void* detour;
    const std::atomic<uint32_t>* callbacks;
};

bool RetireHalo2ObserverHooks(Halo2RetirementHook* hooks, size_t count)
{
    for (size_t i=0;i<count;++i)
    {
        auto& hook=hooks[i];
        if (!*hook.target) continue;
        const auto status=MCCVR_DisableHookForRetirement(*hook.target);
        if (status!=MH_OK&&status!=MH_ERROR_DISABLED&&status!=MH_ERROR_NOT_CREATED)
        { LOG("Halo 2 observer cleanup pending: disable target=%p status=%d",*hook.target,int(status));return false; }
    }
    for (size_t i=0;i<count;++i)
    {
        auto& hook=hooks[i];
        if (!*hook.target) continue;
        const void* functions[]{hook.detour};
        const void* originals[]{reinterpret_cast<const void*>(hook.original->load(std::memory_order_acquire))};
        if (!WaitForNativeDetourQuiescence(functions,originals,1,*hook.callbacks))
        { LOG("Halo 2 observer cleanup pending: callbacks/ingress target=%p",*hook.target);return false; }
    }
    for (size_t i=0;i<count;++i)
    {
        auto& hook=hooks[i];
        if (!*hook.target) continue;
        const auto status=MH_RemoveHook(*hook.target);
        if (status!=MH_OK&&status!=MH_ERROR_NOT_CREATED)
        { LOG("Halo 2 observer cleanup pending: remove target=%p status=%d",*hook.target,int(status));return false; }
        *hook.target=nullptr;
        hook.original->store(0,std::memory_order_release);
    }
    return true;
}
