// Execute the production CE reader and native turn transaction. The native
// service endpoints are explicit fixtures, never a game process. Pinned
// datum/angle instructions are independently exercised by the Unicorn test.
#include "../src/dll/haloce_controls.cpp"
#include <cstdio>
#include <vector>

namespace
{
constexpr uint32_t playerId=0x23450006,unitId=0x34560007,weaponId=0x45670008;
GameTitle testTitle=GameTitle::HaloCE;
uint32_t testGeneration=3,weaponOwner=unitId;
uintptr_t playerAddress{},unitAddress{},weaponAddress{},playersAddress{};
int16_t perspective{};
bool gameplayAvailable=true,contextCurrent=true,raiseTurn{};
halo_ce::RenderContext gameplay{};
unsigned failures{},turnCalls{};
int32_t lastUser=-1;
float lastYaw{},lastPitch{};
void Check(bool result,const char* message)
{ if (!result) { ++failures;std::fprintf(stderr,"CE native controls: %s\n",message); } }
bool Near(float a,float b) { return std::fabs(a-b)<0.00001f; }
template<class T> void Put(uintptr_t address,T value)
{ std::memcpy(reinterpret_cast<void*>(address),&value,sizeof(value)); }
uintptr_t __fastcall DatumService(uintptr_t table,uint32_t datum)
{ return table==playersAddress&&datum==playerId?playerAddress:0; }
uintptr_t __fastcall ObjectService(uint32_t datum,uint32_t mask)
{ return datum==unitId&&mask==3?unitAddress:datum==weaponId&&mask==4?weaponAddress:0; }
uint32_t __fastcall WeaponOwnerService(uint32_t datum)
{ return datum==weaponId?weaponOwner:0xffffffffu; }
int16_t __fastcall PerspectiveService(int16_t outputUser)
{ return outputUser==0?perspective:int16_t(-1); }
void __fastcall NativeTurnService(int32_t user,float yaw,float pitch)
{
    ++turnCalls;lastUser=user;lastYaw=yaw;lastPitch=pitch;
    if (raiseTurn) RaiseException(0xe0424242,0,0,nullptr);
}
template<class Function> bool InstallService(uintptr_t rva,Function function)
{
    // Sparse executable endpoint in this fixture's owned private image.
    // mov rax, imm64; jmp rax preserves the native x64 argument registers.
    uint8_t stub[]{0x48,0xb8,0,0,0,0,0,0,0,0,0xff,0xe0};
    const uintptr_t entry=reinterpret_cast<uintptr_t>(function);
    std::memcpy(stub+2,&entry,sizeof(entry));
    DWORD before{};
    auto* at=reinterpret_cast<void*>(moduleBase+rva);
    if (!VirtualProtect(at,sizeof(stub),PAGE_EXECUTE_READWRITE,&before)) return false;
    std::memcpy(at,stub,sizeof(stub));
    FlushInstructionCache(GetCurrentProcess(),at,sizeof(stub));
    return true;
}
bool NativeException()
{
    __try { TurnDispatch(1,.25f,.5f,moduleBase+0xa99660); }
    __except(GetExceptionCode()==0xe0424242?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH)
    { return true; }
    return false;
}
}

GameTitle TitleAdapter_GetActiveTitle() { return testTitle; }
uint32_t TitleAdapter_GetGeneration(GameTitle) { return testGeneration; }
bool HaloCE_GetGameplayContext(halo_ce::RenderContext& out) noexcept
{ if (!gameplayAvailable) return false;out=gameplay;return true; }
bool HaloCE_RenderContextCurrent(const halo_ce::RenderContext& value) noexcept
{ return contextCurrent&&value.tracking.generation==testGeneration&&
    value.referenceRevision==gameplay.referenceRevision&&value.rendererEpoch==gameplay.rendererEpoch; }
void Logf(const char*,...) {}
bool WaitForNativeDetourQuiescence(const void* const*,const void* const*,size_t,
    const std::atomic<uint32_t>& count) { return !count.load(); }

int main()
{
    auto* image=static_cast<uint8_t*>(VirtualAlloc(nullptr,halo_ce::contract::imageSize,
        MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    if (!image) return 2;
    moduleBase=reinterpret_cast<uintptr_t>(image);
    if (!InstallService(halo_ce::contract::player_state::state_datum_get,&DatumService)||
        !InstallService(halo_ce::contract::player_state::state_object_try_get,&ObjectService)||
        !InstallService(halo_ce::contract::player_state::state_weapon_owner,&WeaponOwnerService)||
        !InstallService(halo_ce::contract::player_state::state_user_perspective,&PerspectiveService)) return 2;
    std::vector<uint8_t> data(0x10000);
    const uintptr_t arena=reinterpret_cast<uintptr_t>(data.data());
    const uintptr_t mapping=arena,control=arena+0x1000,clock=arena+0x2000,
        cinematic=arena+0x3000,users=arena+0x4000;
    playersAddress=arena+0x5000;playerAddress=arena+0x6000;
    unitAddress=arena+0x8000;weaponAddress=arena+0xa000;
    Put(moduleBase+0x2ea2d90,mapping);Put(moduleBase+0x2d8fe70,control);
    Put(moduleBase+0x1c40480,playersAddress);Put(moduleBase+0x2e9fd68,clock);
    Put(moduleBase+0x2ea0208,cinematic);Put(moduleBase+0x2d9cd90,users);
    Put(moduleBase+0x1b85760,int32_t(-1));Put(clock,uint8_t(1));
    Put(mapping+0xb8,playerId);Put(mapping+8,playerId);
    Put(playersAddress+0x22,uint16_t(0xc20));Put(playerAddress+0x64,unitId);
    Put(control+0x170,unitId);Put(control+0x10+0x58,unitId);
    Put(unitAddress+0x1f8,playerId);Put(unitAddress+0xd8,uint32_t(0xffffffffu));
    Put(users,uint8_t(1));Put(users+8,weaponId);
    const uintptr_t director=moduleBase+0x2d9b960+0xf8;
    active=stateReady=turnReady=true;generation=3;retiring=false;
    turnOriginal=reinterpret_cast<void*>(&NativeTurnService);
    LARGE_INTEGER frequency{};QueryPerformanceFrequency(&frequency);
    qpcSeconds=1.0/double(frequency.QuadPart);
    gameplay.tracking.generation=gameplay.reference.generation=3;
    gameplay.tracking.spaceEpoch=gameplay.reference.spaceEpoch=4;
    gameplay.tracking.serial=1;gameplay.referenceRevision=1;gameplay.rendererEpoch=2;
    gameplay.camera.forward={1,0,0};gameplay.camera.up={0,0,1};gameplay.camera.verticalFov=1;
    gameplay.camera.viewport=gameplay.camera.window={0,0,100,100};
    gameplay.camera.nearPlane=.01f;gameplay.camera.farPlane=100;
    auto& rig=gameplay.tracking.controllers;
    rig.padValid=true;rig.turnSmooth=false;rig.controlsPresentationBlocked=false;
    rig.turnSnapDeg=45;rig.turnSmoothDegS=120;

    HaloCELocalPlayerState player{};halo_ce::RenderContext frame{};
    Check(HaloCEControls_GetLocalPlayerState(player)&&player.player==playerId&&
        player.unit==unitId&&player.inputUser==1&&player.hasControlledUnit&&player.onFoot&&
        player.nativePreparesFirstPerson&&player.firstPersonVisible&&player.weapon==weaponId,
        "actual reader joins output-zero player, input-one control, unit backlink and weapon owner");
    Check(HaloCEControls_GetLocomotionFrame(player,frame),"verified on-foot state admits basic controls");
    for (unsigned blocked=0;blocked<8;++blocked)
    {
        switch(blocked)
        {
        case 0:Put(director+0x58,uint8_t(1));break;
        case 1:Put(director+0x59,uint8_t(1));break;
        case 2:Put(moduleBase+0x1b85760,int32_t(0));break;
        case 3:Put(clock+2,uint8_t(1));break;
        case 4:Put(cinematic+0xa,uint8_t(1));break;
        case 5:perspective=1;break;
        case 6:Put(unitAddress+0xd8,uint32_t(0x11110001));break;
        case 7:rig.controlsPresentationBlocked=true;break;
        }
        Check(!HaloCEControls_GetLocomotionFrame(player,frame),
            "native input/look suppression, pause, cinematic, perspective, vehicle and shared menu gates refuse controls");
        TurnDispatch(1,.25f,.5f,moduleBase+0xa99660);
        Check(Near(lastYaw,.25f)&&Near(lastPitch,.5f)&&!HaloCEControls_OwnsLookStick(),
            "refused native state preserves both original look deltas and stops claiming the stick");
        Put(director+0x58,uint8_t(0));Put(director+0x59,uint8_t(0));
        Put(moduleBase+0x1b85760,int32_t(-1));Put(clock+2,uint8_t(0));
        Put(cinematic+0xa,uint8_t(0));perspective=0;rig.controlsPresentationBlocked=false;
        Put(unitAddress+0xd8,uint32_t(0xffffffffu));
    }
    Put(playerAddress+0x64,uint32_t(0xffffffffu));Put(control+0x170,uint32_t(0xffffffffu));
    Put(control+0x10+0x58,uint32_t(0xffffffffu));
    Check(HaloCEControls_GetLocalPlayerState(player)&&!player.hasControlledUnit&&!player.onFoot&&
        !HaloCEControls_GetLocomotionFrame(player,frame),"native death/no-unit state is readable but cannot turn or move head-relative");
    Put(playerAddress+0x64,unitId);Put(control+0x170,unitId);Put(control+0x10+0x58,unitId);
    Put(users+8,uint32_t(0xffffffffu));
    Check(HaloCEControls_GetLocomotionFrame(player,frame)&&player.weapon==0xffffffffu,
        "unarmed player retains basic locomotion independently of optional weapon ownership");
    Put(users+8,weaponId);weaponOwner=0x22220001;
    Check(HaloCEControls_GetLocalPlayerState(player)&&player.weapon==0xffffffffu,
        "another unit's weapon cannot be published as this player's tracked weapon");
    weaponOwner=unitId;
    Put(mapping+4,playerId);
    Check(!HaloCEControls_GetLocalPlayerState(player),"ambiguous native input mapping is rejected");
    Put(mapping+4,uint32_t(0));Put(unitAddress+0x1f8,uint32_t(0));
    Check(!HaloCEControls_GetLocalPlayerState(player),"missing native player backlink is rejected");
    Put(unitAddress+0x1f8,playerId);Put(mapping+0xb8,uint32_t(6));
    Check(!HaloCEControls_GetLocalPlayerState(player),"unsalted player index never grants ownership");
    Put(mapping+0xb8,playerId);Put(control+0x170,uint32_t(0));
    Check(!HaloCEControls_GetLocalPlayerState(player),"different output unit cannot inherit local control");
    Put(control+0x170,unitId);

    turnState={};rig.turnX=0;++gameplay.tracking.serial;
    TurnDispatch(1,.25f,.5f,moduleBase+0xa99660);
    Check(lastUser==1&&lastYaw==0&&lastPitch==0&&HaloCEControls_OwnsLookStick(),
        "admitted native phase owns yaw and parks native pitch while head tracking supplies pitch");
    rig.turnX=1;++gameplay.tracking.serial;
    TurnDispatch(1,.25f,.5f,moduleBase+0xa99660);
    Check(Near(lastYaw,-.785398163f)&&lastPitch==0,"production native phase submits configured snap delta");
    TurnDispatch(1,.25f,.5f,moduleBase+0xa99660);
    Check(lastYaw==0&&lastPitch==0,"repeated native call cannot apply the same XR turn twice");
    rig.turnSmooth=true;++gameplay.tracking.serial;
    LARGE_INTEGER counter{};QueryPerformanceCounter(&counter);
    turnState.previousSeconds=double(counter.QuadPart)*qpcSeconds-1.0;
    TurnDispatch(1,.25f,.5f,moduleBase+0xa99660);
    Check(Near(lastYaw,-.20943951f)&&lastPitch==0,
        "production native phase applies configured smooth rate with the 100 ms elapsed-time cap");
    rig.turnSmooth=false;
    TurnDispatch(0,.25f,.5f,moduleBase+0xa99660);
    Check(lastUser==0&&Near(lastYaw,.25f)&&Near(lastPitch,.5f),"another native input user retains both deltas");
    TurnDispatch(1,.25f,.5f,moduleBase+0xa99661);
    Check(Near(lastYaw,.25f)&&Near(lastPitch,.5f),"another native caller retains both deltas");
    float x=7,y=8;gameplay.tracking.headOrientation={0,.707106781f,0,.707106781f};
    Check(HaloCEControls_MapMoveStick(0,1,x,y)&&Near(x,-1)&&Near(y,0),
        "production movement uses the same current player and native-center/head receipt");
    contextCurrent=false;
    Check(!HaloCEControls_MapMoveStick(0,1,x,y)&&Near(x,-1)&&Near(y,0)&&
        !HaloCEControls_OwnsLookStick(),"stale ownership cannot rotate movement or suppress stock look");
    contextCurrent=true;turnReady=false;
    Check(HaloCEControls_GetLocomotionFrame(player,frame)&&!HaloCEControls_OwnsLookStick(),
        "optional turn failure preserves native state and head-relative movement");
    turnReady=true;raiseTurn=true;
    const auto priorExceptions=exceptions.load();
    Check(NativeException()&&!callbacks.load()&&!HaloCEControls_OwnsLookStick()&&
        exceptions.load()==priorExceptions+1,
        "native structured exception propagates after retiring the real scope and revoking stale look ownership");
    raiseTurn=false;TurnDispatch(1,.25f,.5f,moduleBase+0xa99660);
    Check(!callbacks.load(),"native callback recovers without stranded retirement ownership");
    testTitle=GameTitle::Halo3;
    Check(!HaloCEControls_GetLocalPlayerState(player)&&!HaloCEControls_MapMoveStick(0,1,x,y),
        "CE never claims another title's state or movement");
    active=stateReady=turnReady=false;moduleBase=0;
    VirtualFree(image,0,MEM_RELEASE);
    return failures?1:0;
}
