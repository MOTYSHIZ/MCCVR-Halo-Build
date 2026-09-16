#include "haloce_contact.h"
#include "haloce_controls.h"
#include "haloce_stereo_core.h"
#include "haloce_native_bindings.h"
#include "contact_melee_queue.h"
#include "hook_quiescence.h"
#include "title_adapter.h"
#include "vr.h"
#include "../common/haloce_contact_logic.h"
#include "../common/haloce_snapshot.h"
#include "../common/halo4_world_collision_logic.h"
#include "../common/vr_interaction_refinement_logic.h"
#include "../common/minhook_lifecycle.h"
#include "../common/config.h"
#include "../common/log.h"
#include <windows.h>
#include <intrin.h>
#include <MinHook.h>

// E-CE-CONTACT-1. CE owns collision, authored damage, attribution and effects.
// Only immutable, pointer-free tracked palettes cross to the native biped tick.
namespace
{
using namespace halo_ce;
using TickFn=uint8_t(__fastcall*)(uint32_t);
using ResolverFn=uint8_t(__fastcall*)(const float*,const float*,float*,uint32_t);
using CollisionFn=uint8_t(__fastcall*)(uint32_t,const float*,const float*,uint32_t,void*);
using MeleeFn=void(__fastcall*)(uint32_t,uint32_t,uint16_t);
using DamageFn=void(__fastcall*)(void*,uint32_t,int16_t,int16_t,int16_t,const void*);
using ObjectFn=uintptr_t(__fastcall*)(uint32_t,uint32_t);
struct Hook { void* target{};void* original{};bool enabled{}; } tickHook,damageHook;
HMODULE retained{};
uintptr_t moduleBase{};
std::atomic<uint32_t> generation{},callbacks{};
std::atomic<bool> active{},installed{},retiring{},worldFault{},meleeFault{},processing{};
std::atomic<bool> worldReady{},meleeReady{};
std::atomic<uint64_t> publications{},ticks{},worldQueries{},worldContacts{},corrections{},
    meleeQueries{},meleeContacts{},meleeApplied[2]{},dropped{},exceptions{};
std::atomic<uint64_t> weaponEnvelopes{},weaponNodeFallbacks{};
std::atomic<uint64_t> meleeReachApplied[2]{};
std::atomic<uint64_t> lastWeaponGraph{};
uint32_t failedGeneration{};
uint64_t lastReport{};
ContactMeleeQueue queues[2];
contact_melee::Hand meleeHands[2];
struct Correction
{
    uint32_t generation{},unit{UINT32_MAX};
    uint64_t shape{},reference{},atMs{};
    contact_melee::Point desired{},delta{};
};
Snapshot<Correction> correction[2];
struct Worker
{
    contact_melee::Frame frame{};
    contact_melee::Point accepted[contact_melee::kMaxPoints]{};
    uint64_t lastAt{};
    bool seeded{};
    ContactReleaseSmoothing smoothing{};
} workers[2];
struct DamageScope
{
    bool active{},applied{};
    uint32_t owner{UINT32_MAX},target{UINT32_MAX};
    contact_melee::Point position{},direction{};
};
thread_local DamageScope damageScope;

bool Current() noexcept
{
    return installed.load(std::memory_order_acquire)&&active.load(std::memory_order_acquire)&&
        !retiring.load(std::memory_order_acquire)&&TitleAdapter_GetActiveTitle()==GameTitle::HaloCE&&
        TitleAdapter_GetGeneration(GameTitle::HaloCE)==generation.load(std::memory_order_acquire);
}
bool Admitted(uint32_t unit,HaloCELocalPlayerState& state,RenderContext& context) noexcept
{
    return Current()&&HaloCEControls_GetLocomotionFrame(state,context)&&state.unit==unit&&
        state.hasControlledUnit&&state.onFoot&&state.nativePreparesFirstPerson&&
        !state.nativeInputBlocked&&!state.nativeLookBlocked&&!state.nativePaused&&!state.nativeCinematicFlag&&
        !context.tracking.controllers.controlsPresentationBlocked&&
        context.tracking.generation==generation.load(std::memory_order_acquire)&&
        HaloCE_RenderContextCurrent(context);
}
uintptr_t Object(uint32_t handle,uint32_t mask) noexcept
{
    if (handle==UINT32_MAX||!(handle>>16)||!moduleBase) return 0;
    return reinterpret_cast<ObjectFn>(moduleBase+contract::player_state::state_object_try_get)(handle,mask);
}

// Native CE collision_result: type +0, fraction +14, point +18, plane +24,
// material +34, object handle +38. Both official and retail producers agree.
struct CollisionResult
{
    int16_t type{-1};uint8_t prefix[0x12]{};
    float fraction{};contact_melee::Point point{},normal{};float planeDistance{};
    uint16_t material{UINT16_MAX},pad{};uint32_t object{UINT32_MAX};uint8_t tail[0x14]{};
};
static_assert(sizeof(CollisionResult)==0x50&&offsetof(CollisionResult,fraction)==0x14&&
    offsetof(CollisionResult,point)==0x18&&offsetof(CollisionResult,normal)==0x24&&
    offsetof(CollisionResult,material)==0x34&&offsetof(CollisionResult,object)==0x38);

void DamageBody(void* event,uint32_t target,int16_t node,int16_t region,
    int16_t material,const void* extra,uintptr_t caller)
{
    const auto original=reinterpret_cast<DamageFn>(damageHook.original);
    if (!original) return;
    auto& scope=damageScope;
    const bool owned=scope.active&&caller==moduleBase+0xb0c8a8;
    if (owned)
    {
        auto* bytes=static_cast<uint8_t*>(event);
        // Exact private event constructed by the native player helper. Its
        // target and attribution must still match the physical collision.
        if (!bytes||target!=scope.target||!Object(target,1)||
            *reinterpret_cast<const uint32_t*>(bytes+0x10)!=scope.owner) return;
        std::memcpy(bytes+0x20,&scope.position,sizeof(scope.position));
        std::memcpy(bytes+0x38,&scope.direction,sizeof(scope.direction));
    }
    original(event,target,node,region,material,extra);
    if (owned) scope.applied=true;
}
__declspec(noinline) void __fastcall DamageHook(void* event,uint32_t target,
    int16_t node,int16_t region,int16_t material,const void* extra)
{
    callbacks.fetch_add(1,std::memory_order_acq_rel);
    __try { DamageBody(event,target,node,region,material,extra,reinterpret_cast<uintptr_t>(_ReturnAddress())); }
    __finally { callbacks.fetch_sub(1,std::memory_order_release); }
}
struct Backend
{
    uint32_t owner{};
    float unitsPerMetre{};
    bool assisted{};
    bool Query(const contact_melee::Sweep& sweep,contact_melee::Hit& hit) noexcept
    {
        CollisionResult result{};
        contact_melee::Point delta{};
        if (!BuildMeleeContactVector(sweep,unitsPerMetre,delta)) return false;
        meleeQueries.fetch_add(1,std::memory_order_relaxed);
        if (!reinterpret_cast<CollisionFn>(moduleBase+contract::contact::contact_collision)(
            0x1000e9,&sweep.start.x,&delta.x,owner,&result)||result.type!=3||
            result.object==owner||!Object(result.object,1)) return false;
        if (!std::isfinite(result.fraction)||result.fraction<0||result.fraction>1||
            !contact_melee::Finite(result.point)||!contact_melee::Finite(result.normal)) return false;
        hit={result.object,result.point,result.normal,result.fraction,true};
        meleeContacts.fetch_add(1,std::memory_order_relaxed);
        return true;
    }
    bool Apply(uint32_t unit,const contact_melee::Hit& hit,const contact_melee::Sweep& sweep) noexcept
    {
        if (unit!=owner||!Object(owner,1)||!Object(hit.unit,1)) return false;
        // Requery the identical bounded reach segment to retain its first
        // obstruction/material and refuse an occluded or replaced target.
        CollisionResult result{};
        contact_melee::Point delta{};
        if (!BuildMeleeContactVector(sweep,unitsPerMetre,delta)) return false;
        const float length=std::sqrt(contact_melee::Dot(delta,delta));
        if (!std::isfinite(length)||length<=0.000001f||
            !reinterpret_cast<CollisionFn>(moduleBase+contract::contact::contact_collision)(
                0x1000e9,&sweep.start.x,&delta.x,owner,&result)||result.type!=3||result.object!=hit.unit||
            !Object(hit.unit,1)||!std::isfinite(result.fraction)||result.fraction<0||result.fraction>1||
            !contact_melee::Finite(result.point)||!contact_melee::Finite(result.normal)) return false;
        damageScope={true,false,owner,hit.unit,result.point,{delta.x/length,delta.y/length,delta.z/length}};
        __try
        {
            reinterpret_cast<MeleeFn>(moduleBase+contract::contact::contact_player_melee)(
                owner,hit.unit,result.material);
        }
        __finally { damageScope.active=false; }
        assisted=damageScope.applied&&result.fraction*length>
            length-kPhysicalMeleeReachMetres*unitsPerMetre+0.00001f*unitsPerMetre;
        return damageScope.applied;
    }
};

bool SameShape(const contact_melee::Frame& a,const contact_melee::Frame& b) noexcept
{ return a.unit==b.unit&&a.shape==b.shape&&a.referenceEpoch==b.referenceEpoch&&a.count==b.count; }

void WorldTick(int side,const contact_melee::Frame& frame,uint64_t now,uint8_t worldTailPoints)
{
    auto& worker=workers[side];
    const float scale=frame.transform.unitsPerMetre;
    contact_melee::Point desired[contact_melee::kMaxPoints]{};
    for (unsigned i=0;i<frame.count;++i) desired[i]=frame.transform.World(frame.points[i]);
    if (!worker.seeded||!SameShape(worker.frame,frame)||!worker.lastAt||now-worker.lastAt>150||
        Halo4WorldCollisionMovementIsTeleport(&worker.accepted[0].x,&desired[0].x,scale))
    {
        worker.smoothing.Reset();worker.seeded=true;
        std::memcpy(worker.accepted,desired,frame.count*sizeof(desired[0]));
        worker.frame=frame;worker.lastAt=now;
        correction[side].Publish({generation.load(),frame.unit,frame.shape,frame.referenceEpoch,now,desired[0],{}});
        return;
    }
    // Root + six node extrema preserve hand coverage. Every authored weapon
    // corner/face centre gets its own probe; equal-coordinate tie breaking must
    // not hide a gun face behind a hand node or another bounding-box corner.
    unsigned indices[7+kCeWeaponBoundsSamples]{};
    const unsigned tail=worldTailPoints==kCeWeaponBoundsSamples&&frame.count>worldTailPoints?
        worldTailPoints:0;
    const unsigned nodes=frame.count-tail;
    for (unsigned axis=0;axis<3;++axis)
    {
        unsigned low=0,high=0;
        for (unsigned i=1;i<nodes;++i)
        {
            if ((&desired[i].x)[axis]<(&desired[low].x)[axis]) low=i;
            if ((&desired[i].x)[axis]>(&desired[high].x)[axis]) high=i;
        }
        indices[1+axis*2]=low;indices[2+axis*2]=high;
    }
    for (unsigned i=0;i<tail;++i) indices[7+i]=nodes+i;
    float strongest[3]{};float strongestSquared{};
    for (unsigned slot=0;slot<7+tail;++slot)
    {
        const unsigned i=indices[slot];
        bool duplicate=false;
        for (unsigned prior=0;prior<slot;++prior) if (indices[prior]==i) duplicate=true;
        if (duplicate||Halo4WorldCollisionDistanceSquared(&worker.accepted[i].x,&desired[i].x)<1e-10f) continue;
        contact_melee::Point accepted=desired[i];
        const bool resolved=reinterpret_cast<ResolverFn>(moduleBase+contract::contact::contact_resolver)(
            &worker.accepted[i].x,&desired[i].x,&accepted.x,frame.unit)!=0;
        worldQueries.fetch_add(1,std::memory_order_relaxed);
        if (!resolved||!contact_melee::Finite(accepted)) continue;
        const auto delta=contact_melee::Subtract(accepted,desired[i]);
        const float squared=contact_melee::Dot(delta,delta);
        // A rejected correction must not become next tick's known-good point.
        // This is the same presentation bound used by ApplyContactCorrections.
        if (!std::isfinite(squared)||squared>0.75f*0.75f*scale*scale) continue;
        if (squared>strongestSquared)
        { strongestSquared=squared;strongest[0]=delta.x;strongest[1]=delta.y;strongest[2]=delta.z; }
    }
    const bool contact=strongestSquared>1e-10f;
    for (unsigned i=0;i<frame.count;++i)
        worker.accepted[i]={desired[i].x+strongest[0],desired[i].y+strongest[1],desired[i].z+strongest[2]};
    worker.frame=frame;worker.lastAt=now;
    (void)worker.smoothing.Apply(now,contact,scale,strongest);
    correction[side].Publish({generation.load(),frame.unit,frame.shape,frame.referenceEpoch,now,
        desired[0],{strongest[0],strongest[1],strongest[2]}});
    if (contact)
    { worldContacts.fetch_add(1,std::memory_order_relaxed);VR_PulseContactHaptics(side==0,0.18f); }
}

void ProcessWorld(int side,const contact_melee::Frame& frame,uint64_t now,uint8_t worldTailPoints) noexcept
{
    __try { WorldTick(side,frame,now,worldTailPoints); }
    __except(EXCEPTION_EXECUTE_HANDLER)
    { worldFault=true;exceptions.fetch_add(1,std::memory_order_relaxed); }
}
void ProcessMelee(int side,const contact_melee::Frame& frame) noexcept
{
    __try
    {
        Backend backend{frame.unit,frame.transform.unitsPerMetre};
        if (meleeHands[side].Process(frame,g_config.physical_melee_swing_speed,backend)==contact_melee::ContactResult::Applied)
        {
            meleeApplied[side].fetch_add(1,std::memory_order_relaxed);
            if (backend.assisted) meleeReachApplied[side].fetch_add(1,std::memory_order_relaxed);
            VR_PulseContactHaptics(side==0,0.35f);
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    { damageScope.active=false;meleeFault=true;exceptions.fetch_add(1,std::memory_order_relaxed); }
}
void ContactTick(uint32_t unit)
{
    if (!Current()) return;
    HaloCELocalPlayerState state{};RenderContext context{};
    if (!Admitted(unit,state,context)) return;
    bool expected=false;
    if (!processing.compare_exchange_strong(expected,true,std::memory_order_acquire)) return;
    __try
    {
        const uint64_t now=GetTickCount64();ticks.fetch_add(1,std::memory_order_relaxed);
        for (int side=0;side<2;++side)
        {
            ContactMeleePacket packet{},latest{};bool found=false;
            for (unsigned n=0;n<8&&queues[side].Pop(packet);++n)
            {
                if (packet.generation!=state.generation||packet.frame.unit!=unit||
                    packet.frame.referenceEpoch!=ContactReferenceEpoch(context)||
                    packet.frame.serial>context.tracking.serial||
                    packet.publishedAtMs>now||now-packet.publishedAtMs>150||!packet.frame.Valid())
                { meleeHands[side].Reset();workers[side].seeded=false;continue; }
                latest=packet;found=true;
                if (g_config.physical_melee&&meleeReady.load()&&!meleeFault.load()) ProcessMelee(side,packet.frame);
                else meleeHands[side].Reset();
            }
            if (found&&g_config.world_collision&&worldReady.load()&&!worldFault.load())
                ProcessWorld(side,latest.frame,now,latest.worldTailPoints);
            else if (!g_config.world_collision||!worldReady.load()||worldFault.load()||now-workers[side].lastAt>150)
                workers[side].seeded=false;
        }
    }
    __finally { processing.store(false,std::memory_order_release); }
}
__declspec(noinline) uint8_t __fastcall TickHook(uint32_t unit)
{
    callbacks.fetch_add(1,std::memory_order_acq_rel);uint8_t result{};
    __try
    {
        const auto original=reinterpret_cast<TickFn>(tickHook.original);
        if (original) result=original(unit);
        if (result) ContactTick(unit);
    }
    __finally { callbacks.fetch_sub(1,std::memory_order_release); }
    return result;
}
bool Remove() noexcept
{
    active=false;installed=false;worldReady=false;meleeReady=false;retiring=true;
    for (auto* hook:{&tickHook,&damageHook})
        if (hook->target&&hook->enabled)
        {
            const auto result=MCCVR_DisableHookForRetirement(hook->target);
            if (result!=MH_OK&&result!=MH_ERROR_DISABLED) return false;
            hook->enabled=false;
        }
    const void* functions[]{reinterpret_cast<const void*>(&TickHook),reinterpret_cast<const void*>(&ContactTick),
        reinterpret_cast<const void*>(&DamageHook),reinterpret_cast<const void*>(&DamageBody),
        reinterpret_cast<const void*>(&HaloCEContact_ApplyPalette),reinterpret_cast<const void*>(&HaloCEContact_CommitPalette)};
    const void* trampolines[]{tickHook.original,nullptr,damageHook.original,nullptr,nullptr,nullptr};
    if (!WaitForNativeDetourQuiescence(functions,trampolines,6,callbacks)) return false;
    for (auto* hook:{&tickHook,&damageHook})
    {
        if (hook->target&&MH_RemoveHook(hook->target)!=MH_OK) return false;
        *hook={};
    }
    for (int side=0;side<2;++side)
    { queues[side].Reset();meleeHands[side].Reset();workers[side]={};correction[side].Publish({}); }
    if (retained) { FreeLibrary(retained);retained=nullptr; }
    moduleBase=0;generation=0;retiring=false;return true;
}
bool Install(uintptr_t base,size_t size,uint32_t gen) noexcept
{
    // Keep the shared simulation/query admission separate from each optional
    // consumer. A missing melee/damage hook must leave world contact usable;
    // a missing world resolver must leave physical melee usable.
    static_assert(contract::contact::entries.size()==6&&contract::contact::witnesses.size()==23&&
        contract::contact::relatives.size()==7&&
        contract::contact::entries[3].rva==contract::contact::contact_resolver&&
        contract::contact::entries[4].rva==contract::contact::contact_player_melee);
    const auto entries=std::span(contract::contact::entries);
    const auto witnesses=std::span(contract::contact::witnesses);
    const auto relatives=std::span(contract::contact::relatives);
    const NativeContractSet common{entries.first(3),witnesses.first(6),relatives.first(1),{}};
    const NativeContractSet world{entries.subspan(3,1),witnesses.subspan(6,4),relatives.subspan(1,1),{}};
    const NativeContractSet melee{entries.subspan(4),witnesses.subspan(10),relatives.subspan(2),{}};
    const char* failure{};
    if (!VerifyNativeFeatureBindings(base,size,gen,common,failure))
    { LOG("CE contact stock fallback: %s",failure?failure:"native binding verification");return false; }
    const bool haveWorld=VerifyNativeFeatureBindings(base,size,gen,world,failure);
    if (!haveWorld) LOG("CE world contact stock fallback: %s",failure?failure:"native resolver verification");
    const bool haveMelee=VerifyNativeFeatureBindings(base,size,gen,melee,failure);
    if (!haveMelee) LOG("CE physical melee stock fallback: %s",failure?failure:"native damage verification");
    if (!haveWorld&&!haveMelee) return false;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<LPCWSTR>(base),&retained)) return false;
    moduleBase=base;generation=gen;retiring=false;worldFault=false;meleeFault=false;
    const auto create=[&](Hook& hook,uint32_t rva,void* detour) {
        void* target=reinterpret_cast<void*>(base+rva);
        const auto status=MH_CreateHook(target,detour,&hook.original);
        if (status!=MH_OK) return status;
        hook.target=target;
        const auto enabled=MH_EnableHook(hook.target);
        if (enabled==MH_OK) hook.enabled=true;
        return enabled;
    };
    const auto tickStatus=create(tickHook,contract::contact::contact_biped_tick,reinterpret_cast<void*>(&TickHook));
    if (tickStatus!=MH_OK)
    {
        LOG("CE contact stock fallback: biped tick hook status %d",tickStatus);(void)Remove();return false;
    }
    worldReady=haveWorld;
    if (haveMelee)
    {
        const auto status=create(damageHook,contract::contact::contact_damage,reinterpret_cast<void*>(&DamageHook));
        meleeReady=status==MH_OK;
        if (status!=MH_OK) LOG("CE physical melee stock fallback: damage hook status %d",status);
    }
    if (!worldReady.load()&&!meleeReady.load()) { (void)Remove();return false; }
    installed=true;active=true;
    LOG("CE contact installed: native biped update, world clamp=%d physical melee=%d; 12 stock CE authored weapon envelopes in both renderers, exact custom/Anniversary replacement surfaces unproven",worldReady.load(),meleeReady.load());
    LOG("CE physical melee reach: %.0f cm along speed-qualified physical swing; native first obstruction, exact target/material requery and per-hand retraction retained",kPhysicalMeleeReachMetres*100);
    return true;
}
}

bool HaloCEContact_Poll(uintptr_t base,size_t size,uint32_t gen,bool isActive) noexcept
{
    active.store(isActive,std::memory_order_release);
    if (retained&&(!isActive||base!=moduleBase||gen!=generation.load()||retiring.load()))
        if (!Remove()) return false;
    if (!isActive||!base||!gen) return false;
    if (!installed.load()&&gen!=failedGeneration)
        if (!Install(base,size,gen)) failedGeneration=gen;
    const uint64_t now=GetTickCount64();
    if (now-lastReport>=2000)
    {
        lastReport=now;
        LOG("CE contact gen=%u armed=%d world=%d/%d melee=%d/%d worldFault=%d meleeFault=%d publish=%llu ticks=%llu queries=%llu contacts=%llu corrections=%llu meleeQueries=%llu meleeContacts=%llu applied=%llu/%llu drops=%llu exceptions=%llu",
            gen,Current(),g_config.world_collision,worldReady.load(),g_config.physical_melee,meleeReady.load(),worldFault.load(),meleeFault.load(),publications.load(),ticks.load(),
            worldQueries.load(),worldContacts.load(),corrections.load(),meleeQueries.load(),meleeContacts.load(),
            meleeApplied[0].load(),meleeApplied[1].load(),dropped.load(),exceptions.load());
        LOG("CE weapon contact stock-envelope=%llu node-only-fallback=%llu graph=%llX; 14 surface probes plus up to 7 node probes for held hand, unknown graph keeps nodes only",
            weaponEnvelopes.load(),weaponNodeFallbacks.load(),lastWeaponGraph.load());
        LOG("CE physical melee reach applied=%llu/%llu allowance-cm=%.0f (within total native applications)",
            meleeReachApplied[0].load(),meleeReachApplied[1].load(),kPhysicalMeleeReachMetres*100);
    }
    return Current();
}
void HaloCEContact_ApplyPalette(const halo_ce::RenderContext& context,
    const halo_ce::FirstPersonBinding& binding,const halo_ce::NodeMatrix* authored,
    halo_ce::NodeMatrix* staged,HaloCEContactPublication& publication) noexcept
{
    publication={};
    callbacks.fetch_add(1,std::memory_order_acq_rel);
    __try
    {
        if (!Current()||(!g_config.world_collision&&!g_config.physical_melee)) __leave;
        HaloCELocalPlayerState state{};
        if (!HaloCEControls_GetLocalPlayerState(state)||state.generation!=context.tracking.generation||
            !state.hasControlledUnit||!state.onFoot||!state.nativePreparesFirstPerson||
            state.nativeInputBlocked||state.nativeLookBlocked||state.nativePaused||state.nativeCinematicFlag||
            context.tracking.controllers.controlsPresentationBlocked||!HaloCE_RenderContextCurrent(context)) __leave;
        contact_melee::Frame frames[2]{};
        if (!BuildContactFrames(context,binding,staged,state.unit,frames,publication.weaponBoundsSamples)) __leave;
        lastWeaponGraph.store(binding.nodeIdentity,std::memory_order_relaxed);
        if (publication.weaponBoundsSamples[0]||publication.weaponBoundsSamples[1])
            weaponEnvelopes.fetch_add(1,std::memory_order_relaxed);
        else weaponNodeFallbacks.fetch_add(1,std::memory_order_relaxed);
        const uint64_t now=GetTickCount64();Vec3 deltas[2]{};
        for (int side=0;side<2;++side)
        {
            Correction saved{};
            const auto desired=frames[side].transform.World(frames[side].points[0]);
            if (g_config.world_collision&&worldReady.load()&&!worldFault.load()&&correction[side].Read(saved)&&
                saved.generation==state.generation&&saved.unit==state.unit&&saved.shape==frames[side].shape&&
                saved.reference==frames[side].referenceEpoch&&saved.atMs<=now&&now-saved.atMs<150&&
                contact_melee::Finite(saved.delta)&&Halo4WorldCollisionDistanceSquared(&saved.desired.x,&desired.x)<
                    0.25f*context.unitsPerMeter*context.unitsPerMeter)
                deltas[side]={saved.delta.x,saved.delta.y,saved.delta.z};
        }
        if ((Dot(deltas[0],deltas[0])>1e-10f||Dot(deltas[1],deltas[1])>1e-10f)&&
            ApplyContactCorrections(context,binding,authored,deltas,staged))
            corrections.fetch_add(1,std::memory_order_relaxed);
        publication.frames[0]=frames[0];publication.frames[1]=frames[1];
        publication.generation=state.generation;
    }
    __finally { callbacks.fetch_sub(1,std::memory_order_release); }
}
void HaloCEContact_CommitPalette(const halo_ce::RenderContext& context,
    const HaloCEContactPublication& publication) noexcept
{
    callbacks.fetch_add(1,std::memory_order_acq_rel);
    __try
    {
        if (!publication.generation||publication.generation!=context.tracking.generation||
            !Current()||(!g_config.world_collision&&!g_config.physical_melee)||
            !HaloCE_RenderContextCurrent(context)) __leave;
        const uint64_t now=GetTickCount64();
        for (int side=0;side<2;++side)
        {
            const auto& frame=publication.frames[side];
            if (!frame.Valid()||frame.serial!=context.tracking.serial||
                frame.referenceEpoch!=ContactReferenceEpoch(context)) continue;
            const uint8_t tail=publication.weaponBoundsSamples[side];
            if (tail&&tail!=kCeWeaponBoundsSamples) continue;
            if (tail>=frame.count) continue;
            const int result=queues[side].Push({frame,now,publication.generation,tail});
            if (result==2) publications.fetch_add(1,std::memory_order_relaxed);
            else if (!result) dropped.fetch_add(1,std::memory_order_relaxed);
        }
    }
    __finally { callbacks.fetch_sub(1,std::memory_order_release); }
}
