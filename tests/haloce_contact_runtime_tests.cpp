// Production contact transport, native-call routing and optional failure tests.
// Native services use declared fixtures in an owned private executable image.
#include "../src/dll/haloce_contact.cpp"
#include <cstdio>

namespace
{
constexpr uint32_t testOwner=0x12340007,testTarget=0x34560009;
unsigned failures{},nativeDamageCalls{},nativeTickCalls{},hapticCalls[2]{};
unsigned meshProbeMask{};bool recordMeshProbes{};
bool stateAvailable=true,testCurrent=true,hitEnabled=true,raiseWorld{},raiseMelee{},raiseTick{};
GameTitle testTitle=GameTitle::HaloCE;
uint32_t testGeneration=4,collisionTarget=testTarget;
RenderContext testContext{};
contact_melee::Point damagePosition{},damageDirection{};
void Check(bool value,const char* message)
{ if (!value) { ++failures;std::fprintf(stderr,"CE contact: %s\n",message); } }
bool Near(float a,float b) { return std::fabs(a-b)<1e-5f; }
template<class Function> bool InstallService(uint32_t rva,Function function)
{
    uint8_t code[]{0x48,0xb8,0,0,0,0,0,0,0,0,0xff,0xe0};
    const uintptr_t target=reinterpret_cast<uintptr_t>(function);std::memcpy(code+2,&target,8);
    DWORD before{};auto* at=reinterpret_cast<void*>(moduleBase+rva);
    if (!VirtualProtect(at,sizeof(code),PAGE_EXECUTE_READWRITE,&before)) return false;
    std::memcpy(at,code,sizeof(code));FlushInstructionCache(GetCurrentProcess(),at,sizeof(code));return true;
}
uintptr_t __fastcall TestObject(uint32_t handle,uint32_t mask)
{ return (mask==1&&(handle==testOwner||handle==testTarget))?uintptr_t(0x100):0; }
uint8_t __fastcall TestResolver(const float* start,const float* desired,float* out,uint32_t owner)
{
    Check(owner==testOwner,"resolver keeps full owner handle");
    if (recordMeshProbes)
    {
        const int slot=int(std::round(desired[1]*100));
        if (slot>=1&&slot<=14) meshProbeMask|=1u<<(slot-1);
    }
    if (raiseWorld) RaiseException(0xe0424141,0,0,nullptr);
    std::memcpy(out,desired,12);
    if (desired[0]>.5f&&start[0]<=.5f) out[0]=.49f;
    return 1;
}
uint8_t __fastcall TestCollision(uint32_t flags,const float* start,const float* vector,
    uint32_t owner,void* output)
{
    Check(flags==0x1000e9&&owner==testOwner,"native CE collision ABI/filter");
    if (raiseMelee) RaiseException(0xe0424242,0,0,nullptr);
    if (!hitEnabled||!vector||vector[0]<=0) return 0;
    auto& hit=*static_cast<CollisionResult*>(output);hit.type=3;hit.fraction=.5f;
    hit.object=collisionTarget;hit.material=12;
    hit.point={start[0]+vector[0]*.5f,start[1]+vector[1]*.5f,start[2]+vector[2]*.5f};
    hit.normal={-1,0,0};return 1;
}
void __fastcall TestDamage(void* event,uint32_t target,int16_t,int16_t,int16_t,const void*)
{
    ++nativeDamageCalls;Check(target==testTarget,"physical contact cannot hit another target");
    std::memcpy(&damagePosition,static_cast<uint8_t*>(event)+0x20,12);
    std::memcpy(&damageDirection,static_cast<uint8_t*>(event)+0x38,12);
}
void __fastcall TestMelee(uint32_t owner,uint32_t target,uint16_t material)
{
    Check(owner==testOwner&&target==testTarget&&material==12,"native explicit target/material are retained");
    alignas(16) uint8_t event[0x60]{};std::memcpy(event+0x10,&owner,4);
    DamageBody(event,target,-1,-1,-1,nullptr,moduleBase+0xb0c8a8);
}
uint8_t __fastcall TestTick(uint32_t)
{
    ++nativeTickCalls;
    if (raiseTick) RaiseException(0xe0424343,0,0,nullptr);
    return 1;
}
bool CatchNativeTick()
{
    __try { TickHook(testOwner); }
    __except(GetExceptionCode()==0xe0424343?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH) { return true; }
    return false;
}
contact_melee::Frame Frame(uint64_t serial,float x)
{
    contact_melee::Frame f{};f.timeNs=1'000'000'000+int64_t(serial)*10'000'000;
    f.serial=serial;f.referenceEpoch=ContactReferenceEpoch(testContext);f.shape=8;
    f.unit=testOwner;f.count=2;f.transform.unitsPerMetre=1;
    f.points[0]={x,0,0};f.points[1]={x,.1f,0};return f;
}
void Publish(const contact_melee::Frame& f,int side=0)
{ Check(queues[side].Push({f,GetTickCount64(),testGeneration})>0,"fixture publication"); }
}

Config::Config() {}
Config g_config;
GameTitle TitleAdapter_GetActiveTitle() { return testTitle; }
uint32_t TitleAdapter_GetGeneration(GameTitle) { return testGeneration; }
bool HaloCEControls_GetLocalPlayerState(HaloCELocalPlayerState& out) noexcept
{
    out={};out.generation=testGeneration;out.unit=testOwner;out.player=0x23450008;
    out.hasControlledUnit=out.onFoot=out.nativePreparesFirstPerson=true;
    out.nativeInputBlocked=out.nativeLookBlocked=false;return stateAvailable;
}
bool HaloCEControls_GetLocomotionFrame(HaloCELocalPlayerState& state,RenderContext& out) noexcept
{ out=testContext;return HaloCEControls_GetLocalPlayerState(state)&&stateAvailable; }
bool HaloCE_RenderContextCurrent(const RenderContext& c) noexcept
{ return testCurrent&&c.tracking.generation==testGeneration&&c.referenceRevision==testContext.referenceRevision&&
    c.rendererEpoch==testContext.rendererEpoch; }
void VR_PulseContactHaptics(bool left,float) { ++hapticCalls[left?0:1]; }
void Logf(const char*,...) {}
bool WaitForNativeDetourQuiescence(const void* const*,const void* const*,size_t,const std::atomic<uint32_t>& count)
{ return count.load()==0; }

int main()
{
    auto* image=VirtualAlloc(nullptr,contract::imageSize,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    if (!image) return 2;moduleBase=reinterpret_cast<uintptr_t>(image);
    if (!InstallService(contract::contact::contact_resolver,&TestResolver)||
        !InstallService(contract::contact::contact_collision,&TestCollision)||
        !InstallService(contract::contact::contact_player_melee,&TestMelee)||
        !InstallService(contract::player_state::state_object_try_get,&TestObject)) return 2;
    generation=testGeneration;active=installed=worldReady=meleeReady=true;damageHook.original=reinterpret_cast<void*>(&TestDamage);
    tickHook.original=reinterpret_cast<void*>(&TestTick);
    testContext.tracking.generation=testContext.reference.generation=testGeneration;
    testContext.tracking.spaceEpoch=testContext.reference.spaceEpoch=1;
    testContext.tracking.serial=100;testContext.referenceRevision=1;testContext.rendererEpoch=1;
    testContext.tracking.controllers.controlsPresentationBlocked=false;
    g_config.world_collision=true;g_config.physical_melee=false;
    HaloCEContactPublication publication{};
    publication.generation=testGeneration;
    publication.frames[0]=publication.frames[1]=Frame(testContext.tracking.serial,.3f);
    ContactMeleePacket committed{};
    auto queuesEmpty=[&] { return !queues[0].Pop(committed)&&!queues[1].Pop(committed); };
    ++publication.generation;HaloCEContact_CommitPalette(testContext,publication);
    Check(queuesEmpty(),"foreign generation cannot publish contact");--publication.generation;
    --publication.frames[0].serial;--publication.frames[1].serial;
    HaloCEContact_CommitPalette(testContext,publication);
    Check(queuesEmpty(),"stale tracking receipt cannot publish contact");
    ++publication.frames[0].serial;++publication.frames[1].serial;
    ++testContext.rendererEpoch;HaloCEContact_CommitPalette(testContext,publication);
    Check(queuesEmpty(),"graphics-switch receipt cannot publish contact");--testContext.rendererEpoch;
    testCurrent=false;HaloCEContact_CommitPalette(testContext,publication);
    Check(queuesEmpty(),"revoked palette ownership cannot publish contact");testCurrent=true;
    g_config.world_collision=false;HaloCEContact_CommitPalette(testContext,publication);
    Check(queuesEmpty(),"both independent toggles disabled suppress publication");g_config.world_collision=true;
    HaloCEContact_CommitPalette(testContext,publication);HaloCEContact_CommitPalette(testContext,publication);
    for (int side=0;side<2;++side)
    {
        Check(queues[side].Pop(committed)&&committed.generation==testGeneration&&
            committed.frame.serial==testContext.tracking.serial&&!queues[side].Pop(committed),
            "committed palette queues each physical hand once across duplicate eyes");
        queues[side].Reset();
    }
    Publish(Frame(1,.3f));TickHook(testOwner);
    Publish(Frame(2,.8f));TickHook(testOwner);
    Correction response{};Check(correction[0].Read(response)&&Near(response.delta.x,-.31f),
        "world collision clamps visible palette root using CE native resolver");
    Check(nativeDamageCalls==0&&hapticCalls[0]==1&&hapticCalls[1]==0,"collision toggle does not create melee and haptics route left");
    g_config.world_collision=false;g_config.physical_melee=true;g_config.physical_melee_swing_speed=5;
    Publish(Frame(3,0));TickHook(testOwner);Publish(Frame(4,.1f));TickHook(testOwner);
    Check(nativeDamageCalls==1&&Near(damagePosition.x,.05f)&&Near(damageDirection.x,1),
        "physical motion hits actual swept target and replaces native impact position/direction");
    Publish(Frame(4,.1f));TickHook(testOwner);Publish(Frame(5,.2f));TickHook(testOwner);
    Check(nativeDamageCalls==1,"duplicate frame and continued penetration do not strike again");
    Publish(Frame(6,0),1);TickHook(testOwner);Publish(Frame(7,.1f),1);TickHook(testOwner);
    Check(nativeDamageCalls==2&&meleeApplied[0]==1&&meleeApplied[1]==1,"other physical hand has independent impact latch");
    const auto before=ticks.load();stateAvailable=false;Publish(Frame(8,.2f));TickHook(testOwner);
    Check(ticks.load()==before,"missing/dead/blocked local state cannot enter contact tick");stateAvailable=true;
    ++testContext.rendererEpoch;TickHook(testOwner);
    Check(nativeDamageCalls==2,"old renderer/reference frame is discarded before native damage");
    // A gun face can share its x extrema with a hand node and still needs a
    // native probe. The exact fourteen-point receipt survives queue transport.
    contact_melee::Frame mesh=Frame(20,.3f);mesh.count=15;mesh.shape=0x9876;
    for (unsigned i=1;i<15;++i) mesh.points[i]={.3f,float(i)*.01f,0};
    workers[1]={};recordMeshProbes=true;meshProbeMask=0;
    const uint64_t meshNow=GetTickCount64();WorldTick(1,mesh,meshNow,14);
    mesh.serial++;for (unsigned i=0;i<15;++i) mesh.points[i].x=.6f;
    const uint64_t queriesBefore=worldQueries.load();WorldTick(1,mesh,meshNow+1,14);
    Check(meshProbeMask==0x3fff&&worldQueries.load()-queriesBefore==15,
        "every weapon corner/face is probed despite node extrema ties; bounded fifteen calls for one-node fixture");
    recordMeshProbes=false;
    ContactMeleePacket meshPacket{mesh,meshNow,testGeneration,14},copied{};
    queues[1].Reset();Check(queues[1].Push(meshPacket)==2&&queues[1].Pop(copied)&&copied.worldTailPoints==14,
        "weapon sample receipt remains attached to its exact immutable frame");queues[1].Reset();
    meleeHands[0].Reset();raiseMelee=true;Publish(Frame(9,0));TickHook(testOwner);Publish(Frame(10,.1f));TickHook(testOwner);
    Check(meleeFault.load()&&Current()&&!worldFault.load()&&callbacks.load()==0&&!processing.load(),
        "native melee exception isolates feature and balances callback/tick ownership");
    raiseMelee=false;g_config.world_collision=true;raiseWorld=true;
    Publish(Frame(11,0));TickHook(testOwner);Publish(Frame(12,.1f));TickHook(testOwner);
    Check(worldFault.load()&&Current()&&callbacks.load()==0,"native collision fault leaves CE camera ownership independent");
    raiseTick=true;Check(CatchNativeTick()&&callbacks.load()==0,"original native tick SEH propagates with balanced callback lifetime");
    active=installed=false;moduleBase=0;VirtualFree(image,0,MEM_RELEASE);
    if (failures) return 1;
    std::puts("PASS: production CE contact native routing, toggles, hand latches, stale-frame rejection and SEH isolation");return 0;
}
