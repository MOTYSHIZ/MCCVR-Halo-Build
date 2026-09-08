// Included by game.cpp. Evidence: docs/GESTURE-MELEE-BINDING-EVIDENCE.md.
// Only the cold worker touches game memory. Input consumes generation-tagged
// atomic snapshots; there is no new native hook or game-state write.
namespace
{
struct GestureBindingDescriptor
{
    GameTitle title;
    unsigned action, actionCount, stride, remap, controllerOffset;
    uintptr_t readerRva, stateLoadRva;
    const char* readerPattern;
    const char* statePattern;
};
constexpr GestureBindingDescriptor kGestureBindings[] = {
    {GameTitle::Halo2,5,0x3C,0x17C4,0x1C,0,
     0x6D9A00,0x6D9B16,
     "48 89 5C 24 08 48 89 7C 24 10 4D 85 C9 48 C7 01 03 00 00 00 4D 8B D9 C7 41 08 00 00 00 00 48 8D 05 ?? ?? ?? ?? 4C 8B D1 4C 0F 44 D8 83 FA 3B 0F 87 A5 00 00 00 48 63 C2 48 6B F8 64 41 83 F8 FF 75 69",
     "48 69 D0 C4 17 00 00 48 8D 05 ?? ?? ?? ?? 41 B8 C4 17 00 00"},
    {GameTitle::Halo3,5,0x43,0x518,0xC0,0x514,
     0x187028,0xF751A,
     "48 63 81 14 05 00 00 4C 8B C1 4C 63 CA 83 F8 03 77 0C 48 8D 0D ?? ?? ?? ?? 8B 0C 81 EB 06 8B 0D ?? ?? ?? ?? 83 F9 01 75 10 4A 8D 04 4D 73 00 00 00 49 03 C1 49 8D 04 80 C3 4B 63 84 88 C0 00 00 00 48 8D 0C 40 45 88 4C 88 08 4B 63 84 88 C0 00 00 00 48 8D 0C 40 49 8D 04 88 C3",
     "48 69 D8 18 05 00 00 48 8D 05 ?? ?? ?? ?? 48 03 D8"},
    {GameTitle::Halo3ODST,5,0x47,0x558,0xC0,0x554,
     0x1B8420,0xBFC81,
     "48 63 81 54 05 00 00 4C 8B C1 4C 63 CA 83 F8 03 77 0C 48 8D 0D ?? ?? ?? ?? 8B 0C 81 EB 06 8B 0D ?? ?? ?? ?? 83 F9 01 75 10 4A 8D 04 4D 77 00 00 00 49 03 C1 49 8D 04 80 C3 4B 63 84 88 C0 00 00 00 48 8D 0C 40 45 88 4C 88 08 4B 63 84 88 C0 00 00 00 48 8D 0C 40 49 8D 04 88 C3",
     "49 69 DE 58 05 00 00 48 8D 05 ?? ?? ?? ?? 48 03 D8"},
    {GameTitle::HaloReach,4,0x49,0x700,0x100,0x6FC,
     0xD1070,0x62C2D,
     "48 63 81 FC 06 00 00 4C 8B C1 83 F8 03 77 0D 48 8D 0D ?? ?? ?? ?? 44 8B 0C 81 EB 07 44 8B 0D ?? ?? ?? ?? 48 63 CA 41 83 F9 01 75 0F 48 C1 E1 04 49 8D 80 24 02 00 00 48 03 C1 C3 49 63 84 88 00 01 00 00 48 03 C0 41 89 54 C0 08 49 63 84 88 00 01 00 00 48 C1 E0 04 49 03 C0 C3",
     "48 69 D8 00 07 00 00 48 8D 05 ?? ?? ?? ?? 48 03 D8"},
    {GameTitle::Halo4,3,0x55,0x7E8,0x100,0x7E4,
     0x13F5CC,0xA2818,
     "48 63 81 E4 07 00 00 4C 8B C1 83 F8 03 77 0D 48 8D 0D ?? ?? ?? ?? 44 8B 0C 81 EB 07 44 8B 0D ?? ?? ?? ?? 48 63 CA 41 83 F9 01 75 0F 48 C1 E1 04 49 8D 80 54 02 00 00 48 03 C1 C3 49 63 84 88 00 01 00 00 48 03 C0 41 89 54 C0 08 49 63 84 88 00 01 00 00 48 C1 E0 04 49 03 C0 C3",
     "4C 69 EB E8 07 00 00 48 8D 05 ?? ?? ?? ?? 4C 03 E8"},
};

// Transport masks independently confirmed in each kit's XInput converter.
constexpr uint16_t kGesturePadMasks[14] = {
    1,2,4,8,0x10,0x20,0x40,0x80,0x1000,0x2000,0x4000,0x8000,0x100,0x200};
constexpr uint16_t kGestureHalo2PadMasks[14] = {
    1,2,4,8,0x10,0x20,0x40,0x80,0x100,0x200,0x1000,0x2000,0x4000,0x8000};
constexpr uint32_t kGestureLeftTrigger = 1u << 16;
constexpr uint32_t kGestureRightTrigger = 1u << 17;

struct GestureBindingCache
{
    std::atomic<uint64_t> value[4]{};
    std::atomic<uint64_t> observedMs{0};
    uintptr_t attemptedBase=0, state=0;
    uint32_t attemptedGeneration=0;
    GameTitle attemptedTitle=GameTitle::None;
    uint32_t lastTransport[4]{UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX};
} g_gestureBinding;

uint32_t GestureTransport(GameTitle title,unsigned button) noexcept
{
    if(button==0) return kGestureLeftTrigger;
    if(button==1) return kGestureRightTrigger;
    if(button>=16) return 0;
    return title==GameTitle::Halo2 ? kGestureHalo2PadMasks[button-2] : kGesturePadMasks[button-2];
}

bool GestureReadNativeBindings(const GestureBindingDescriptor& d,uintptr_t state,
    uint32_t (&transport)[4]) noexcept
{
    __try
    {
        for(unsigned controller=0;controller<4;++controller)
        {
            const uintptr_t record=state+controller*d.stride;
            if(d.title==GameTitle::Halo2)
            {
                // Native binding records: count then up to eight {type,index,hold}.
                // Type 2 is gamepad. Axis and held bindings need a separate
                // proven transport; declining them must not invent a button.
                const uintptr_t action=record+d.action*100;
                const int count=*reinterpret_cast<const int*>(action+0x1C);
                if(count<1 || count>8) continue;
                for(int i=0;i<count;++i)
                {
                    const int* binding=reinterpret_cast<const int*>(action+0x20+i*12);
                    if(binding[0]!=2) continue;
                    if(binding[2]==0) transport[controller]=GestureTransport(d.title,static_cast<unsigned>(binding[1]));
                    break;
                }
            }
            else
            {
                if(*reinterpret_cast<const unsigned*>(record+d.controllerOffset)!=controller) continue;
                const unsigned button=*reinterpret_cast<const unsigned*>(record+d.remap+d.action*4);
                transport[controller]=GestureTransport(d.title,button);
            }
        }
        return true;
    }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}

bool GestureUniqueAt(uintptr_t base,size_t size,uintptr_t rva,const char* pattern)
{
    const uintptr_t found=sig::Find(base,size,pattern);
    return rva<size && found==base+rva &&
        !sig::Find(found+1,size-(found+1-base),pattern);
}

void RefreshGestureMeleeBinding(const TitleDescriptor* title,bool levelRunning,uint64_t now)
{
    if(!title || !levelRunning || !g_config.gesture_melee)
    { g_gestureBinding.observedMs.store(0,std::memory_order_release); return; }
    const GestureBindingDescriptor* descriptor=nullptr;
    for(const auto& d:kGestureBindings) if(d.title==title->title) descriptor=&d;
    if(!descriptor) { g_gestureBinding.observedMs.store(0,std::memory_order_release); return; }
    // Pin only while this cold read/verification runs. No module pointer is
    // exposed to the input hook and no reference survives the worker tick.
    HMODULE pinned=nullptr;
    if(!GetModuleHandleExW(0,title->moduleName,&pinned))
    { g_gestureBinding.observedMs.store(0,std::memory_order_release); return; }
    uintptr_t base=0; size_t size=0;
    const uint32_t generation=TitleAdapter_GetGeneration(title->title);
    uint32_t transport[4]{};
    const auto& d=*descriptor;
    bool readable=sig::ModuleRange(title->moduleName,base,size) && base==reinterpret_cast<uintptr_t>(pinned);
    if(readable && (g_gestureBinding.attemptedBase!=base ||
        g_gestureBinding.attemptedGeneration!=generation || g_gestureBinding.attemptedTitle!=title->title))
    {
        g_gestureBinding.state=0;
        g_gestureBinding.attemptedBase=base;
        g_gestureBinding.attemptedGeneration=generation;
        g_gestureBinding.attemptedTitle=title->title;
        for(auto& previous:g_gestureBinding.lastTransport) previous=UINT32_MAX;
        if(GestureUniqueAt(base,size,d.readerRva,d.readerPattern) &&
           GestureUniqueAt(base,size,d.stateLoadRva,d.statePattern))
        {
            const uintptr_t state=sig::RipTarget(base+d.stateLoadRva+10,base+d.stateLoadRva+14);
            if(state>=base && state-base<=size && size-(state-base)>=4*d.stride)
                g_gestureBinding.state=state;
        }
        LOG("Gesture melee %s: active binding reader %s; generation %u",title->displayName,
            g_gestureBinding.state ? "verified (read only)" : "unavailable; gesture stays stock",generation);
    }
    readable=readable && g_gestureBinding.state && GestureReadNativeBindings(d,g_gestureBinding.state,transport);
    FreeLibrary(pinned);
    if(!readable) { g_gestureBinding.observedMs.store(0,std::memory_order_release); return; }
    for(unsigned controller=0;controller<4;++controller)
    {
        if(g_gestureBinding.lastTransport[controller]!=transport[controller])
        {
            g_gestureBinding.lastTransport[controller]=transport[controller];
            LOG("Gesture melee %s controller %u: configured transport 0x%05X%s",title->displayName,
                controller,transport[controller],transport[controller] ? "" : " (unavailable or unsupported binding; no injection)");
        }
        const uint64_t value=(uint64_t(generation)<<32) | (uint64_t(title->title)<<24) | transport[controller];
        g_gestureBinding.value[controller].store(value,std::memory_order_release);
    }
    g_gestureBinding.observedMs.store(now,std::memory_order_release);
}

uint32_t ReadGestureMeleeTransport(GameTitle title,uint32_t generation,unsigned controller,uint64_t now) noexcept
{
    if(controller>=4) return 0;
    const uint64_t observed=g_gestureBinding.observedMs.load(std::memory_order_acquire);
    if(!observed || now<observed || now-observed>150) return 0;
    const uint64_t value=g_gestureBinding.value[controller].load(std::memory_order_acquire);
    if(uint32_t(value>>32)!=generation || ((value>>24)&0xFF)!=uint64_t(title)) return 0;
    return uint32_t(value)&0x3FFFF;
}
}
