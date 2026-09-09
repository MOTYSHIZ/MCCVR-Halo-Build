// H4EK-owned native contact path. Included after Halo 4's physics types/state.
// Binding and layout evidence: CONTACT-RESUME-CHECKPOINT.md.
using Halo4ContactUpdateFn=void(__fastcall*)(void*);
using Halo4ContactBuildFn=void(__fastcall*)(uint32_t,int32_t,uint32_t*);
using Halo4ContactConsumeFn=void(__fastcall*)(uint32_t,uint64_t,int16_t,float,uint8_t,uint32_t*,void*);
using Halo4ContactDamageFn=void(__fastcall*)(uint32_t,int32_t,const void*,const void*,const float*);
using Halo4ContactObjectFn=const uint8_t*(__fastcall*)(uint32_t,uint32_t);
struct Halo4ContactRuntime
{
    std::atomic<bool> enabled{false},faulted{false},processing{false};
    std::atomic<uint32_t> callbacks{0},owner{UINT32_MAX};
    std::atomic<uint64_t> queries{0},contacts{0},submitted[2]{},rejected{0},overflow{0},predicted{0};
    uintptr_t base=0;
    uint32_t generation=0;
    void* updateTarget=nullptr;
    void* damageTarget=nullptr;
    Halo4ContactUpdateFn updateOriginal=nullptr;
    Halo4ContactDamageFn damageOriginal=nullptr;
    Halo4ContactBuildFn build=nullptr;
    Halo4ContactConsumeFn consume=nullptr;
    Halo4ContactObjectFn object=nullptr;
    uint32_t(__fastcall* playbackMode)()=nullptr;
    uint32_t(__fastcall* simulationMode)()=nullptr;
    const uint64_t* flags=nullptr;
    ContactMeleeQueue queue[2];
    contact_melee::Hand hands[2];
} g_halo4Contact;

struct Halo4ContactScope
{
    bool active=false,applying=false,submitted=false;
    uint32_t owner=UINT32_MAX,target=UINT32_MAX;
    unsigned rays=0;
    contact_melee::Sweep sweep{};
    float direction[3]{};
};
thread_local Halo4ContactScope g_halo4ContactScope;
CinematicControlState ReadHalo4CinematicControl() noexcept;

bool Halo4ContactMeleeReady()
{
    return g_halo4Contact.enabled.load(std::memory_order_acquire) &&
        !g_halo4Contact.faulted.load(std::memory_order_acquire);
}

const uint8_t* Halo4ContactObject(uint32_t handle,bool requireBiped=false)
{
    if(handle==UINT32_MAX || !(handle>>16) || !g_halo4Contact.object ||
        !g_halo4EngineTlsIndex || *g_halo4EngineTlsIndex>=256) return nullptr;
    auto** slots=reinterpret_cast<const uint8_t**>(__readgsqword(0x58));
    const auto* tls=slots ? slots[*g_halo4EngineTlsIndex] : nullptr;
    const auto* table=tls ? *reinterpret_cast<const uint8_t* const*>(tls+0x18) : nullptr;
    // H4EK datum_get: stride +20, valid +31, extent +44, entries +50.
    // Retail 43AD8 checks the full signed salt before 5DA400 checks kind +4.
    if(!table || !table[0x31] || *reinterpret_cast<const uint64_t*>(table+0x20)!=0x18 ||
        !*reinterpret_cast<const void* const*>(table+0x50)) return nullptr;
    const int32_t count=*reinterpret_cast<const int32_t*>(table+0x44);
    if(count<=0 || count>0x10000 || (handle&0xFFFF)>=static_cast<uint32_t>(count)) return nullptr;
    // The verified native accessor applies this type mask after its own full
    // salt check. Targets may be any object; only the attacker must be biped.
    return g_halo4Contact.object(handle,requireBiped ? 1u : UINT32_MAX);
}

const uint8_t* Halo4ContactBiped(uint32_t handle) { return Halo4ContactObject(handle,true); }

bool Halo4RedirectContactRay(uintptr_t caller,Halo4PhysicsRayCastInput* input,
    Halo4PhysicsRayCastResult* output,bool& returned)
{
    auto& scope=g_halo4ContactScope;
    if(!scope.active || caller<g_halo4Contact.base+0x601B1C ||
        caller>=g_halo4Contact.base+0x60296C) return false;
    returned=false;
    // Only the centre of the native 25-ray grid uses the physical segment.
    // The later obstruction/retarget ray is suppressed inside this scope too.
    if(caller!=g_halo4Contact.base+0x6020EA || ++scope.rays!=13 ||
        !input || !output || !g_halo4WorldCollision.originalRayCast) return true;
    auto local=*input;
    const float start[]{scope.sweep.start.x,scope.sweep.start.y,scope.sweep.start.z};
    const float end[]{scope.sweep.end.x,scope.sweep.end.y,scope.sweep.end.z};
    memcpy(local.start,start,sizeof(start));
    memcpy(local.end,end,sizeof(end));
    returned=g_halo4WorldCollision.originalRayCast(&local,output) &&
        output->type==4 && static_cast<uint32_t>(output->objectIndex)==scope.target &&
        Halo4ContactObject(scope.target);
    return true;
}

__declspec(noinline) void __fastcall Halo4ContactDamageDetour(uint32_t unit,int32_t damage,
    const void* definition,const void* impact,const float* direction)
{
    g_halo4Contact.callbacks.fetch_add(1,std::memory_order_acq_rel);
    __try
    {
        auto& scope=g_halo4ContactScope;
        const uintptr_t caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
        const bool own=scope.active && scope.applying &&
            (caller==g_halo4Contact.base+0x6012F2 || caller==g_halo4Contact.base+0x601315 ||
             caller==g_halo4Contact.base+0x6013C9 || caller==g_halo4Contact.base+0x6013FD ||
             caller==g_halo4Contact.base+0x601935);
        const bool exact=own && unit==scope.owner && impact &&
            *reinterpret_cast<const uint32_t*>(static_cast<const uint8_t*>(impact)+0x1C)==scope.target &&
            Halo4ContactBiped(scope.owner) && Halo4ContactObject(scope.target);
        if(g_halo4Contact.damageOriginal && (!own || exact))
        {
            g_halo4Contact.damageOriginal(unit,damage,definition,impact,own ? scope.direction : direction);
            if(own) scope.submitted=true;
        }
    }
    __finally { g_halo4Contact.callbacks.fetch_sub(1,std::memory_order_acq_rel); }
}

struct Halo4ContactBackend
{
    uint32_t owner=UINT32_MAX;
    bool Query(const contact_melee::Sweep& sweep,contact_melee::Hit& hit) noexcept
    {
        Halo4PhysicsRayCastInput input{};
        Halo4PhysicsRayCastResult output{};
        if(!g_halo4WorldCollision.originalRayCast || !g_halo4Contact.flags) return false;
        input.profile=0x1A; // H4EK E71F40 / retail 601B1C native melee profile.
        memcpy(&input.collisionFlags,g_halo4Contact.flags,sizeof(uint64_t));
        const float start[]{sweep.start.x,sweep.start.y,sweep.start.z};
        const float end[]{sweep.end.x,sweep.end.y,sweep.end.z};
        memcpy(input.start,start,sizeof(start));
        memcpy(input.end,end,sizeof(end));
        input.ignoredObjects[0]=static_cast<int32_t>(owner);
        input.ignoredObjectCount=1;
        input.resultOptions[0]=1;
        g_halo4Contact.queries.fetch_add(1,std::memory_order_relaxed);
        if(!g_halo4WorldCollision.originalRayCast(&input,&output) || output.type!=4 ||
            static_cast<uint32_t>(output.objectIndex)==owner ||
            !Halo4ContactObject(static_cast<uint32_t>(output.objectIndex))) return false;
        hit.unit=static_cast<uint32_t>(output.objectIndex);
        hit.fraction=output.fraction;
        memcpy(&hit.position,output.position,sizeof(output.position));
        memcpy(&hit.normal,output.normal,sizeof(output.normal));
        hit.object=true;
        if(!contact_melee::Finite(hit.position) || !contact_melee::Finite(hit.normal) ||
            !std::isfinite(hit.fraction) || hit.fraction<0 || hit.fraction>1) return false;
        g_halo4Contact.contacts.fetch_add(1,std::memory_order_relaxed);
        return true;
    }
    bool Apply(uint32_t unit,const contact_melee::Hit& hit,const contact_melee::Sweep& sweep) noexcept
    {
        if(unit!=owner || !Halo4ContactBiped(owner) || !Halo4ContactObject(hit.unit)) return false;
        const auto delta=contact_melee::Subtract(sweep.end,sweep.start);
        const float length=std::sqrt(contact_melee::Dot(delta,delta));
        if(!std::isfinite(length) || length<=1e-6f) return false;
        g_halo4ContactScope={true,false,false,owner,hit.unit,0,sweep,
            {delta.x/length,delta.y/length,delta.z/length}};
        uint32_t parameters[20]{};
        bool accepted=false;
        __try
        {
            g_halo4Contact.build(owner,0xEA,parameters);
            if(g_halo4ContactScope.rays==25 && parameters[0]==hit.unit &&
                Halo4ContactObject(hit.unit))
            {
                const int16_t mode=g_halo4Contact.simulationMode()==4 ? 1 : 0;
                g_halo4ContactScope.applying=true;
                g_halo4Contact.consume(owner,0xEA,mode,1.0f,1,parameters,nullptr);
                if(mode==1) g_halo4Contact.predicted.fetch_add(1,std::memory_order_relaxed);
                accepted=mode==1 || g_halo4ContactScope.submitted;
            }
        }
        __finally { g_halo4ContactScope.active=false; }
        return accepted;
    }
};

void Halo4ContactTick(uint32_t unit,void* instance)
{
    if(!Halo4ContactMeleeReady() || !instance ||
        TitleAdapter_GetActiveTitle()!=GameTitle::Halo4) return;
    bool expected=false;
    if(!g_halo4Contact.processing.compare_exchange_strong(expected,true,std::memory_order_acquire)) return;
    __try
    {
        const auto* biped=Halo4ContactBiped(unit);
        if(unit==g_halo4Contact.owner.load(std::memory_order_acquire) && biped==instance)
        {
            const uint64_t now=GetTickCount64();
            const bool admit=g_config.physical_melee && g_enabled.load() && VR_IsStereoEnabled() &&
                g_halo4Camera.armed.load() && !g_halo4Camera.teardownRequested.load() &&
                g_halo4Contact.generation==g_halo4Camera.generation.load() &&
                g_halo4Contact.playbackMode && g_halo4Contact.playbackMode()==0 &&
                ReadHalo4CinematicControl()==CinematicControlState::PlayerControlled &&
                *reinterpret_cast<const int32_t*>(biped+0x24)==-1;
            for(int hand=0;hand<2;++hand)
            {
                ContactMeleePacket packet{};
                for(unsigned n=0;n<8 && g_halo4Contact.queue[hand].Pop(packet);++n)
                {
                    if(!admit || packet.frame.unit!=unit || packet.generation!=g_halo4Contact.generation ||
                        packet.publishedAtMs>now || now-packet.publishedAtMs>100)
                    { g_halo4Contact.hands[hand].Reset(); continue; }
                    Halo4ContactBackend backend{};
                    backend.owner=unit;
                    const auto result=g_halo4Contact.hands[hand].Process(packet.frame,
                        std::clamp(g_config.physical_melee_swing_speed, kPhysicalMeleeSpeedMin, kPhysicalMeleeSpeedMax),backend);
                    if(result==contact_melee::ContactResult::Applied)
                    { g_halo4Contact.submitted[hand].fetch_add(1,std::memory_order_relaxed); VR_PulseContactHaptics(hand==0,0.65f); }
                    else if(result==contact_melee::ContactResult::NativeRejected)
                        g_halo4Contact.rejected.fetch_add(1,std::memory_order_relaxed);
                }
            }
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        g_halo4ContactScope.active=false;
        g_halo4Contact.faulted.store(true,std::memory_order_release);
    }
    g_halo4Contact.processing.store(false,std::memory_order_release);
}

__declspec(noinline) void __fastcall Halo4ContactUpdateDetour(void* instance)
{
    g_halo4Contact.callbacks.fetch_add(1,std::memory_order_acq_rel);
    __try
    {
        uint32_t unit=UINT32_MAX;
        // The native update can destroy/reparent its instance. Capture its
        // handle while it is live; afterwards revalidate by full handle.
        __try
        {
            if(instance && Halo4ContactMeleeReady())
                unit=*reinterpret_cast<const uint32_t*>(static_cast<const uint8_t*>(instance)-0xC);
        }
        __except(EXCEPTION_EXECUTE_HANDLER) { unit=UINT32_MAX; }
        if(g_halo4Contact.updateOriginal) g_halo4Contact.updateOriginal(instance);
        if(unit!=UINT32_MAX) Halo4ContactTick(unit,instance);
    }
    __finally { g_halo4Contact.callbacks.fetch_sub(1,std::memory_order_acq_rel); }
}

bool RemoveHalo4ContactMelee()
{
    g_halo4Contact.enabled.store(false,std::memory_order_release);
    if(!g_halo4Contact.updateTarget && !g_halo4Contact.damageTarget) return true;
    void** targets[]{&g_halo4Contact.updateTarget,&g_halo4Contact.damageTarget};
    for(auto target:targets) if(*target)
    {
        const auto status=MH_DisableHook(*target);
        if(status!=MH_OK && status!=MH_ERROR_DISABLED && status!=MH_ERROR_NOT_CREATED) return false;
    }
    const void* functions[]{reinterpret_cast<void*>(&Halo4ContactUpdateDetour),
        reinterpret_cast<void*>(&Halo4ContactDamageDetour),reinterpret_cast<void*>(&Halo4ContactTick)};
    const void* trampolines[]{reinterpret_cast<void*>(g_halo4Contact.updateOriginal),
        reinterpret_cast<void*>(g_halo4Contact.damageOriginal),nullptr};
    if(!WaitForNativeDetourQuiescence(functions,trampolines,3,g_halo4Contact.callbacks)) return false;
    for(auto target:targets) if(*target)
    {
        const auto status=MH_RemoveHook(*target);
        if(status!=MH_OK && status!=MH_ERROR_NOT_CREATED) return false;
        *target=nullptr;
    }
    g_halo4Contact.updateOriginal=nullptr;
    g_halo4Contact.damageOriginal=nullptr;
    g_halo4Contact.owner.store(UINT32_MAX);
    return true;
}

bool Halo4ContactVerifyCall(uintptr_t base,uintptr_t caller,uintptr_t target)
{
    const auto* instruction=reinterpret_cast<const uint8_t*>(base+caller);
    return instruction[0]==0xE8 && base+caller+5+
        *reinterpret_cast<const int32_t*>(instruction+1)==base+target;
}

bool InstallHalo4ContactMelee(uintptr_t base,size_t size,uint32_t generation)
{
    struct Binding { uintptr_t rva; const char* pattern; };
    constexpr Binding bindings[]{
        {0x5ECCB4,"48 8B C4 48 89 58 10 48 89 70 18 48 89 78 20 55 41 54 41 55 41 56 41 57 48 8D 68 A1 48 81 EC C0 00 00 00 48 8B 05 32 E3 84 00 48 33 C4 48 89 45 27 48 8B F1 48 89 4D D7 E8 57 02 04 00 8B 5E F4 44 8A E0"},
        {0x601B1C,"48 8B C4 48 89 58 20 55 56 57 41 54 41 55 41 56 41 57 48 8D A8 38 FD FF FF 48 81 EC 90 03 00 00 0F 29 70 B8 0F 29 78 A8 44 0F 29 40 98 44 0F 29 48 88 44 0F 29 90 78 FF FF FF 44 0F 29 98 68 FF"},
        {0x60296C,"48 89 5C 24 10 55 56 57 41 54 41 55 41 56 41 57 48 8D 6C 24 F1 48 81 EC D0 00 00 00 48 8B 05 81 86 83 00 48 33 C4 48 89 45 07 48 8B 45 7F 45 33 FF 48 8B 5D 77"},
        {0x603330,"48 8B C4 48 89 58 10 55 56 57 41 54 41 55 41 56 41 57 48 8D A8 D8 FD FF FF 48 81 EC F0 02 00 00 0F 29 70 B8 0F 29 78 A8 44 0F 29 40 98 44 0F 29 48 88 48 8B 05"},
        {0x5DA400,"48 83 EC 28 44 8B 05 0D CE A7 00 44 8B DA 65 48 8B 04 25 58 00 00 00 8B D1 41 B9 18 00 00 00 45 33 D2 4A 8B 04 C0 49 8B 0C 01 E8 A9 96 A6 FF 48 8B C8 48 85 C0 74 0E 0F B6 40 04 41 0F A3 C3 73 04 4C 8B 51 10 49 8B C2"},
        {0x43AD8,"45 33 C0 83 FA FF 74 25 0F B7 C2 3B 41 44 7D 1D 4C 8B 49 20 4C 0F AF C8 4C 03 49 50 66 45 39 01 74 0B C1 FA 10 66 41 39 11 4D 0F 44 C1 49 8B C0 C3"},
        {0x9C62C,"48 83 EC 28 E8 CF F7 FF FF 33 C9 84 C0 74 22 8B 0D D7 AB FB 00 65 48 8B 04 25 58 00 00 00 BA 40 00 00 00 48 8B 04 C8 48 8B 0C 10 8B 89 00 CE 01 00 8B C1 48 83 C4 28 C3"},
        {0x9C5A0,"48 83 EC 28 E8 5B F8 FF FF 33 C9 84 C0 74 1F 8B 0D 63 AC FB 00 65 48 8B 04 25 58 00 00 00 BA 40 00 00 00 48 8B 04 C8 48 8B 0C 10 8B 49 1C 8B C1 48 83 C4 28 C3"},
        {0x601EE4,"48 8B 05 AD 91 9F 02 41 BE FE FF FF FF F3 0F 10 25 63 35 79 00 F3 44 0F"},
    };
    g_halo4Contact.enabled.store(false,std::memory_order_release);
    if(!g_halo4WorldCollision.originalRayCast || !g_halo4EngineTlsIndex ||
        g_halo4Contact.updateTarget || g_halo4Contact.damageTarget) return false;
    for(const auto& binding:bindings)
    {
        const uintptr_t hit=sig::Find(base,size,binding.pattern);
        if(hit!=base+binding.rva || sig::Find(hit+1,base+size-hit-1,binding.pattern))
        { LOG("Halo 4 contact melee unavailable: native binding +0x%llX missing/ambiguous",static_cast<unsigned long long>(binding.rva)); return false; }
    }
    if(!Halo4ContactVerifyCall(base,0x6020E5,0x1C1D4C) ||
        !Halo4ContactVerifyCall(base,0x6026D4,0x1C1D4C) ||
        !Halo4ContactVerifyCall(base,0x602DD3,0x601120) ||
        !Halo4ContactVerifyCall(base,0x602E10,0x601120) ||
        !Halo4ContactVerifyCall(base,0x6012ED,0x603330) ||
        !Halo4ContactVerifyCall(base,0x601310,0x603330) ||
        !Halo4ContactVerifyCall(base,0x6013C4,0x603330) ||
        !Halo4ContactVerifyCall(base,0x6013F8,0x603330) ||
        !Halo4ContactVerifyCall(base,0x601930,0x603330) ||
        !Halo4ContactVerifyCall(base,0x5DA42A,0x43AD8) ||
        !Halo4ContactVerifyCall(base,0x9C630,0x9BE04) ||
        !Halo4ContactVerifyCall(base,0x9C5A4,0x9BE04) ||
        !Halo4ContactVerifyCall(base,0x5ECCEC,0x62CF48))
    { LOG("Halo 4 contact melee unavailable: native call-edge verification failed"); return false; }
    const uintptr_t flags=base+0x601EEB+*reinterpret_cast<const int32_t*>(base+0x601EE7);
    if(flags!=base+0x2FFB098 || flags+sizeof(uint64_t)>base+size) return false;
    g_halo4Contact.base=base;
    g_halo4Contact.generation=generation;
    g_halo4Contact.flags=reinterpret_cast<const uint64_t*>(flags);
    g_halo4Contact.build=reinterpret_cast<Halo4ContactBuildFn>(base+0x601B1C);
    g_halo4Contact.consume=reinterpret_cast<Halo4ContactConsumeFn>(base+0x60296C);
    g_halo4Contact.object=reinterpret_cast<Halo4ContactObjectFn>(base+0x5DA400);
    g_halo4Contact.playbackMode=reinterpret_cast<uint32_t(__fastcall*)()>(base+0x9C62C);
    g_halo4Contact.simulationMode=reinterpret_cast<uint32_t(__fastcall*)()>(base+0x9C5A0);
    g_halo4Contact.faulted.store(false);
    g_halo4Contact.processing.store(false);
    g_halo4Contact.owner.store(UINT32_MAX);
    for(int hand=0;hand<2;++hand)
    { g_halo4Contact.queue[hand].Reset(); g_halo4Contact.hands[hand].Reset(); }
    void* const damageTarget=reinterpret_cast<void*>(base+0x603330);
    if(MH_CreateHook(damageTarget,reinterpret_cast<void*>(&Halo4ContactDamageDetour),
        reinterpret_cast<void**>(&g_halo4Contact.damageOriginal))!=MH_OK)
    { LOG("Halo 4 contact melee unavailable: damage hook creation failed"); return false; }
    g_halo4Contact.damageTarget=damageTarget;
    if(MH_EnableHook(damageTarget)!=MH_OK)
    { LOG("Halo 4 contact melee unavailable: damage hook enable failed; cleanup retained"); return false; }
    void* const updateTarget=reinterpret_cast<void*>(base+0x5ECCB4);
    if(MH_CreateHook(updateTarget,reinterpret_cast<void*>(&Halo4ContactUpdateDetour),
        reinterpret_cast<void**>(&g_halo4Contact.updateOriginal))!=MH_OK)
    { LOG("Halo 4 contact melee unavailable: simulation hook creation failed"); return false; }
    g_halo4Contact.updateTarget=updateTarget;
    if(MH_EnableHook(updateTarget)!=MH_OK)
    { LOG("Halo 4 contact melee unavailable: simulation hook enable failed; cleanup retained"); return false; }
    g_halo4Contact.enabled.store(true,std::memory_order_release);
    LOG("Halo 4 contact melee installed: independent physical hand/weapon sweeps, exact-target native damage; headset verification pending");
    return true;
}

void Halo4PublishContactHand(int hand,const float points[][3],uint32_t count,
    int32_t unit,uint64_t shape)
{
    if(!Halo4ContactMeleeReady() || !g_config.physical_melee || hand<0 || hand>1 ||
        !points || !count || count>contact_melee::kMaxPoints || unit==-1 ||
        g_halo4FloatingPair.generation!=g_halo4Contact.generation) return;
    ContactMeleePacket packet{};
    packet.publishedAtMs=GetTickCount64();
    packet.generation=g_halo4Contact.generation;
    packet.frame=g_halo4FloatingPair.contactFrames[hand];
    auto& frame=packet.frame;
    frame.unit=static_cast<uint32_t>(unit);
    frame.shape=shape ? shape : 1;
    frame.count=count;
    // These extrema have already been rebased onto the unblocked wrist/root
    // by Halo 4's accepted authored-volume publisher.
    for(uint32_t i=0;i<count;++i)
        frame.points[i]=frame.transform.Tracking({points[i][0],points[i][1],points[i][2]});
    g_halo4Contact.owner.store(frame.unit,std::memory_order_release);
    const int published=g_halo4Contact.queue[hand].Push(packet);
    if(hand==1 && published) g_halo4FloatingPair.rightContactPublished=true;
    if(!published)
        g_halo4Contact.overflow.fetch_add(1,std::memory_order_relaxed);
}

void ReportHalo4ContactMelee()
{
    if(!g_halo4Contact.updateTarget && !g_halo4Contact.damageTarget) return;
    LOG("Halo 4 contact melee: enabled=%d fault=%d queries=%llu contacts=%llu submissions(L/R)=%llu/%llu nativeRejected=%llu queueDrops=%llu predictedAttempts=%llu",
        Halo4ContactMeleeReady()?1:0,g_halo4Contact.faulted.load()?1:0,
        g_halo4Contact.queries.exchange(0),g_halo4Contact.contacts.exchange(0),
        g_halo4Contact.submitted[0].exchange(0),g_halo4Contact.submitted[1].exchange(0),
        g_halo4Contact.rejected.exchange(0),g_halo4Contact.overflow.exchange(0),g_halo4Contact.predicted.exchange(0));
}
