// Included in game.cpp after ReachVehicleObjectData. All native locations here
// are Reach-specific HREK homologs; see PHYSICAL-CONTACT-MELEE-WORK.md.
using ReachContactUpdateFn=uint8_t(__fastcall*)(uint32_t);
using ReachContactBuildFn=void(__fastcall*)(uint32_t,int32_t,uint32_t*);
using ReachContactConsumeFn=void(__fastcall*)(uint32_t,uint64_t,int16_t,float,uint8_t,uint32_t*,void*);
using ReachContactDamageFn=void(__fastcall*)(uint32_t,int32_t,const void*,const void*,const float*);
struct ReachContactRuntime
{
    std::atomic<bool> enabled{false},faulted{false},processing{false};
    std::atomic<uint64_t> queries{0},contacts{0},submitted[2]{},rejected{0},overflow{0},predicted{0};
    void* target=nullptr;
    void* damageTarget=nullptr;
    ReachContactDamageFn damageOriginal=nullptr;
    ReachContactUpdateFn original=nullptr;
    ReachContactBuildFn build=nullptr;
    ReachContactConsumeFn consume=nullptr;
    uint8_t(__fastcall* playback)()=nullptr;
    ContactMeleeQueue queue[2];
    contact_melee::Hand hands[2];
    uint32_t generation=0;
} g_reachContact;
struct ReachContactQueryScope
{
    bool active=false;
    uint32_t unit=UINT32_MAX,object=UINT32_MAX;
    unsigned calls=0;
    contact_melee::Sweep sweep{};
    float fraction=0;
};
thread_local ReachContactQueryScope g_reachContactQuery;
struct ReachContactDamageScope
{
    bool active=false,submitted=false;
    uint32_t owner=UINT32_MAX,target=UINT32_MAX;
    float direction[3]{};
};
thread_local ReachContactDamageScope g_reachContactDamage;

__declspec(noinline) void __fastcall ReachContactDamageDetour(uint32_t unit,int32_t damage,
    const void* definition,const void* impact,const float* direction)
{
    g_reachCamera.activeCallbacks.fetch_add(1,std::memory_order_acq_rel);
    __try
    {
        auto& scope=g_reachContactDamage;
        const uintptr_t caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
        const bool own=scope.active &&
            caller>=g_reachCamera.base+0x491100 && caller<g_reachCamera.base+0x4918EC;
        uint8_t kind=0xFF;
        const bool exact=own && unit==scope.owner && impact &&
            *reinterpret_cast<const uint32_t*>(static_cast<const uint8_t*>(impact)+0x1C)==scope.target &&
            ReachVehicleObjectData(static_cast<int32_t>(scope.target),kind) && kind<32;
        // HREK D6CD60's fifth argument is the optional impulse direction.
        // Preserve stock damage/material/ownership; redirect only our exact hit.
        if(g_reachContact.damageOriginal && (!own || exact))
        {
            g_reachContact.damageOriginal(unit,damage,definition,impact,own ? scope.direction : direction);
            if(own) scope.submitted=true;
        }
    }
    __finally { g_reachCamera.activeCallbacks.fetch_sub(1,std::memory_order_acq_rel); }
}

bool ReachContactMeleeReady()
{
    return g_reachContact.enabled.load(std::memory_order_acquire) &&
        !g_reachContact.faulted.load(std::memory_order_acquire);
}

bool ReachRedirectContactVector(uintptr_t caller,uint64_t flags,int32_t mode,
    int32_t ignoredA,int32_t ignoredB,int32_t ignoredC,void* result,uint8_t& returned)
{
    auto& scope=g_reachContactQuery;
    if(!scope.active || caller!=g_reachCamera.base+0x491E8E ||
        static_cast<uint32_t>(ignoredA)!=scope.unit) return false;
    returned=0;
    // HREK D6C1B0 and retail 4919D4 enumerate [-2,2] x [-2,2]. Only
    // their centre sample has zero native aim-assist priority. Substitute the
    // physical segment there and suppress the other 24 head-directed rays.
    if(++scope.calls!=13 || !result || !g_reachWorldCollision.original) return true;
    const float start[]{scope.sweep.start.x,scope.sweep.start.y,scope.sweep.start.z};
    const auto delta=contact_melee::Subtract(scope.sweep.end,scope.sweep.start);
    const float vector[]{delta.x,delta.y,delta.z};
    const auto original=reinterpret_cast<LegacyCollisionTestVectorFn>(g_reachWorldCollision.original);
    returned=original(flags,mode,start,vector,ignoredA,ignoredB,ignoredC,result);
    g_reachContact.queries.fetch_add(1,std::memory_order_relaxed);
    if(returned)
    {
        uint32_t type=0;
        memcpy(&type,result,sizeof(type));
        if(type==4)
        {
            memcpy(&scope.object,static_cast<const uint8_t*>(result)+0x40,sizeof(scope.object));
            memcpy(&scope.fraction,static_cast<const uint8_t*>(result)+4,sizeof(scope.fraction));
        }
    }
    return true;
}

void ReachPublishContactHand(int hand,const FpExplicitPoseTargets& targets,
    const FpInterpolationContext& context,const BoneMatrix& root,const BoneMatrix* solved,
    const float points[][3],int count,int32_t unit,uint32_t generation)
{
    if(!ReachContactMeleeReady() || !g_config.physical_melee || hand<0 || hand>1 ||
        unit==-1 || count<=0 || count>64 || !points || !solved ||
        generation!=g_reachContact.generation) return;
    ContactMeleePacket packet{};
    packet.generation=generation;
    packet.publishedAtMs=GetTickCount64();
    auto& frame=packet.frame;
    frame.timeNs=targets.contactTimeNs;
    frame.serial=targets.contactSerial;
    frame.referenceEpoch=targets.contactReference;
    frame.unit=static_cast<uint32_t>(unit);
    frame.transform=targets.contactSpace[hand];
    frame.rigidMotion=true;
    frame.controllerPose=targets.contactController[hand];
    const uint64_t mask=hand ? context.wristDescendants : context.lWristDescendants;
    frame.shape=(mask ^ (uint64_t(context.count)<<48)) * 1099511628211ull;
    float worldPoints[contact_melee::kMaxPoints][3]{};
    memcpy(worldPoints,points,sizeof(float)*3*count);
    if(hand==1)
    {
        uint64_t weaponShape=0;
        if(LegacyBuildMappedWeaponBounds(GameTitle::HaloReach,context,root,solved,
                worldPoints+count,weaponShape))
        {
            count+=kLegacyCollisionWeaponSamples;
            frame.shape=(frame.shape ^ weaponShape)*1099511628211ull;
        }
    }
    frame.shape=frame.shape ? frame.shape : 1;
    frame.count=static_cast<unsigned>(count);
    for(int i=0;i<count;++i)
    {
        const contact_melee::Point raw{
            worldPoints[i][0]-targets.collisionCorrection[hand][0],
            worldPoints[i][1]-targets.collisionCorrection[hand][1],
            worldPoints[i][2]-targets.collisionCorrection[hand][2]};
        frame.points[i]=frame.transform.Tracking(raw);
    }
    // Invalid tracking/reference frames are deliberately queued to reset the
    // simulation consumer rather than bridging a recenter or dropped pose.
    if(!g_reachContact.queue[hand].Push(packet))
        g_reachContact.overflow.fetch_add(1,std::memory_order_relaxed);
}

struct ReachContactBackend
{
    uint32_t owner=UINT32_MAX;
    uint32_t selected[20]{};
    unsigned selectedPoint=0;
    float selectedFraction=2;
    bool Query(const contact_melee::Sweep& sweep,contact_melee::Hit& hit) noexcept
    {
        uint32_t parameters[20]{};
        g_reachContactQuery={};
        g_reachContactQuery.active=true;
        g_reachContactQuery.unit=owner;
        g_reachContactQuery.sweep=sweep;
        __try { g_reachContact.build(owner,0x8A,parameters); }
        __finally { g_reachContactQuery.active=false; }
        const auto& query=g_reachContactQuery;
        uint8_t kind=0xFF;
        if(query.calls!=25 || query.object==UINT32_MAX || parameters[0]!=query.object ||
            query.object==owner || parameters[1]==UINT32_MAX ||
            !ReachVehicleObjectData(static_cast<int32_t>(query.object),kind) || kind>=32)
            return false;
        hit.unit=query.object; hit.fraction=query.fraction; hit.object=true;
        memcpy(&hit.position,parameters+0xD,sizeof(hit.position));
        memcpy(&hit.normal,parameters+0x10,sizeof(hit.normal));
        if(!contact_melee::Finite(hit.position) || !contact_melee::Finite(hit.normal) ||
            !std::isfinite(hit.fraction) || hit.fraction<0 || hit.fraction>1) return false;
        if(hit.fraction<selectedFraction)
        {
            memcpy(selected,parameters,sizeof(selected));
            selectedPoint=sweep.pointIndex;
            selectedFraction=hit.fraction;
        }
        g_reachContact.contacts.fetch_add(1,std::memory_order_relaxed);
        return true;
    }
    bool Apply(uint32_t unit,const contact_melee::Hit& hit,const contact_melee::Sweep& sweep) noexcept
    {
        uint8_t kind=0xFF;
        if(unit!=owner || selected[0]!=hit.unit || selectedPoint!=sweep.pointIndex ||
            !ReachVehicleObjectData(static_cast<int32_t>(hit.unit),kind) || kind>=32) return false;
        auto** slots=reinterpret_cast<void**>(__readgsqword(0x58));
        const auto index=*reinterpret_cast<const uint32_t*>(g_reachCamera.base+kReachEngineTlsIndexRva);
        if(!slots || index>=0x200 || !slots[index]) return false;
        // Matched consumer reads game globals from TLS +48 and tests +11 ==4
        // for prediction. Its event route preserves the exact hit for the host.
        const auto globals=*reinterpret_cast<const uint8_t* const*>(
            static_cast<const uint8_t*>(slots[index])+0x48);
        if(!globals || (!globals[0] && !globals[1]) || globals[0x1DA]) return false;
        const int16_t mode=globals[0x11]==4 ? 1 : 0;
        if(mode==1) g_reachContact.predicted.fetch_add(1,std::memory_order_relaxed);
        const auto delta=contact_melee::Subtract(sweep.end,sweep.start);
        const float length=std::sqrt(contact_melee::Dot(delta,delta));
        if(!std::isfinite(length) || length<=1e-6f) return false;
        g_reachContactDamage={true,false,owner,hit.unit,
            {delta.x/length,delta.y/length,delta.z/length}};
        // The consumer clamps this value to [0,1] and forwards it as the
        // damage multiplier. A physical strike uses full authored damage;
        // swing speed is solely the admission threshold, not damage scaling.
        __try { g_reachContact.consume(owner,0x8A,mode,1.0f,1,selected,nullptr); }
        __finally { g_reachContactDamage.active=false; }
        // A client submits a native host request; authoritative simulation
        // must actually reach the matching native damage constructor.
        return mode==1 || g_reachContactDamage.submitted;
    }
};

void ReachContactTick(uint32_t unit)
{
    if(!ReachContactMeleeReady() || !g_reachCamera.armed.load(std::memory_order_acquire) ||
        unit!=static_cast<uint32_t>(LegacyCollisionIgnoredObject(GameTitle::HaloReach))) return;
    bool expected=false;
    if(!g_reachContact.processing.compare_exchange_strong(expected,true,std::memory_order_acquire)) return;
    __try
    {
        const uint64_t now=GetTickCount64();
        const bool admit=!exclusive_input::Active() && g_config.physical_melee && g_enabled.load(std::memory_order_acquire) &&
            VR_IsStereoEnabled() && !g_reachCinematicLocked.load(std::memory_order_acquire) &&
            g_reachContact.playback && !g_reachContact.playback() &&
            g_reachCamera.unitInVehicle && !g_reachCamera.unitInVehicle(static_cast<int32_t>(unit));
        for(int hand=0;hand<2;++hand)
        {
            ContactMeleePacket packet{};
            for(unsigned n=0;n<8 && g_reachContact.queue[hand].Pop(packet);++n)
            {
                if(!admit || packet.generation!=g_reachContact.generation || packet.frame.unit!=unit ||
                    packet.publishedAtMs>now || now-packet.publishedAtMs>100)
                { g_reachContact.hands[hand].Reset(); continue; }
                ReachContactBackend backend{};
                backend.owner=unit;
                const auto result=g_reachContact.hands[hand].Process(packet.frame,
                    std::clamp(g_config.physical_melee_swing_speed, kPhysicalMeleeSpeedMin, kPhysicalMeleeSpeedMax),backend);
                if(result==contact_melee::ContactResult::Applied)
                {
                    g_reachContact.submitted[hand].fetch_add(1,std::memory_order_relaxed);
                    VR_PulseContactHaptics(hand==0,0.65f);
                }
                else if(result==contact_melee::ContactResult::NativeRejected)
                    g_reachContact.rejected.fetch_add(1,std::memory_order_relaxed);
            }
            if(!admit) g_reachContact.hands[hand].Reset();
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        g_reachContactQuery.active=false;
        g_reachContactDamage.active=false;
        g_reachContact.faulted.store(true,std::memory_order_release);
    }
    g_reachContact.processing.store(false,std::memory_order_release);
}

__declspec(noinline) uint8_t __fastcall ReachContactUpdateDetour(uint32_t unit)
{
    g_reachCamera.activeCallbacks.fetch_add(1,std::memory_order_acq_rel);
    uint8_t result=0;
    __try
    {
        if(g_reachContact.original) result=g_reachContact.original(unit);
        ReachContactTick(unit);
    }
    __finally { g_reachCamera.activeCallbacks.fetch_sub(1,std::memory_order_acq_rel); }
    return result;
}

bool InstallReachContactMelee(uintptr_t base,size_t size,uint32_t generation)
{
    struct Binding { uintptr_t rva; const char* pattern; };
    constexpr Binding bindings[]{
        {0x49F138,"48 8B C4 48 89 58 08 48 89 68 10 48 89 70 18 48 89 78 20 41 54 41 56 41 57 48 83 EC 20 8B 15 ?? ?? ?? ?? 40 32 FF"},
        {0x4919D4,"48 8B C4 48 89 58 20 55 56 57 41 54 41 55 41 56 41 57 48 8D A8 A8 FE FF FF 48 81 EC 20 02 00 00 0F 29 70 B8 0F 29 78 A8 44 0F 29 40 98 44 0F 29 48 88 44 0F 29 90 78 FF FF FF 44 0F 29 98 68 FF FF FF 44 0F 29 A0 58 FF FF FF 44 0F 29 A8 48 FF FF FF 44 0F 29 B0 38 FF FF FF 44 0F 29 B8 28 FF FF FF 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 45 78 65 48 8B 04 25 58 00 00 00 49 8B D8"},
        {0x4924F0,"48 89 5C 24 10 55 56 57 41 54 41 55 41 56 41 57 48 8D 6C 24 F1 48 81 EC C0 00 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 45 07"},
        // HREK 119360: initialized game globals and game-options playback flag.
        {0x58EEC,"48 83 EC 28 E8 ?? ?? ?? ?? 32 D2 84 C0 74 27 8B 0D ?? ?? ?? ?? 65 48 8B 04 25 58 00 00 00 41 B8 48 00 00 00 48 8B 04 C8 4A 8B 0C 00 38 91 DA 01 00 00 74 02 B2 01 8A C2 48 83 C4 28 C3"},
        {0x492D68,"48 8B C4 48 89 58 10 55 56 57 41 54 41 55 41 56 41 57 48 8D A8 E8 FD FF FF 48 81 EC E0 02 00 00 0F 29 70 B8 0F 29 78 A8 44 0F 29 40 98 44 0F 29 48 88"}};
    g_reachContact.enabled.store(false,std::memory_order_release);
    if(!g_reachWorldCollision.original || g_reachContact.target || g_reachContact.damageTarget) return false;
    for(const auto& binding:bindings)
    {
        const uintptr_t hit=sig::Find(base,size,binding.pattern);
        if(hit!=base+binding.rva || sig::Find(hit+1,base+size-hit-1,binding.pattern))
        { LOG("Reach contact melee unavailable: native binding +0x%llX missing/ambiguous",static_cast<unsigned long long>(binding.rva)); return false; }
    }
    if(!ReachVerifyRel32Call(base,0x4A04D0,0x49F138) ||
        !ReachVerifyRel32Call(base,0x50BA00,0x4919D4) ||
        !ReachVerifyRel32Call(base,0x50BCA7,0x4924F0) ||
        !ReachVerifyRel32Call(base,0x491E89,0x12969C) ||
        !ReachVerifyRel32Call(base,0x58EF0,0x588E0) ||
        !ReachVerifyRel32Call(base,0x492860,0x491100) ||
        !ReachVerifyRel32Call(base,0x491865,0x492D68))
    { LOG("Reach contact melee unavailable: native caller verification failed"); return false; }
    g_reachContact.generation=generation;
    g_reachContact.faulted.store(false);
    g_reachContact.processing.store(false);
    for(int hand=0;hand<2;++hand)
    { g_reachContact.queue[hand].Reset(); g_reachContact.hands[hand].Reset(); }
    g_reachContact.build=reinterpret_cast<ReachContactBuildFn>(base+0x4919D4);
    g_reachContact.consume=reinterpret_cast<ReachContactConsumeFn>(base+0x4924F0);
    g_reachContact.playback=reinterpret_cast<uint8_t(__fastcall*)()>(base+0x58EEC);
    void* const damageTarget=reinterpret_cast<void*>(base+0x492D68);
    if(MH_CreateHook(damageTarget,reinterpret_cast<void*>(&ReachContactDamageDetour),
            reinterpret_cast<void**>(&g_reachContact.damageOriginal))!=MH_OK)
    { LOG("Reach contact melee unavailable: damage-direction hook creation failed"); return false; }
    g_reachContact.damageTarget=damageTarget;
    if(MH_EnableHook(damageTarget)!=MH_OK)
    { LOG("Reach contact melee unavailable: damage-direction hook enable failed; retained for cleanup"); return false; }
    void* const target=reinterpret_cast<void*>(base+0x49F138);
    if(MH_CreateHook(target,reinterpret_cast<void*>(&ReachContactUpdateDetour),
            reinterpret_cast<void**>(&g_reachContact.original))!=MH_OK)
    { LOG("Reach contact melee unavailable: simulation hook creation failed"); return false; }
    g_reachContact.target=target;
    if(MH_EnableHook(target)!=MH_OK)
    { LOG("Reach contact melee unavailable: simulation hook enable failed; retained for normal cleanup"); return false; }
    g_reachContact.enabled.store(true,std::memory_order_release);
    LOG("Reach contact melee installed: per-hand mesh/weapon sweeps, native exact-target melee damage, independent world-collision toggle; headset verification pending");
    return true;
}

void ReportReachContactMelee()
{
    if(!g_reachContact.target) return;
    LOG("Reach contact melee: enabled=%d fault=%d queries=%llu contacts=%llu submissions(L/R)=%llu/%llu nativeRejected=%llu queueDrops=%llu predictedRequests=%llu",
        ReachContactMeleeReady()?1:0,g_reachContact.faulted.load()?1:0,
        g_reachContact.queries.exchange(0),g_reachContact.contacts.exchange(0),
        g_reachContact.submitted[0].exchange(0),g_reachContact.submitted[1].exchange(0),
        g_reachContact.rejected.exchange(0),g_reachContact.overflow.exchange(0),g_reachContact.predicted.exchange(0));
}
