// Executes the shipping XInput -> ownership policy -> OpenXR haptic functions.
// Only the clock, menu/session state, and OpenXR endpoint are simulated. The
// title registry, runtime publication/resolution and amplitude helpers are real.
// No MCC process, controller driver, or OpenXR session is opened.
#include <Windows.h>
#include <Xinput.h>
#include <openxr/openxr.h>
#include "../src/common/config.h"
#include "../src/common/input_logic.h"
#include "../src/dll/title_adapter.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

namespace
{
unsigned checks{},failures{};
uint64_t testNow=10000;
bool testMenu{};
GameTitle testTitle=GameTitle::HaloCE;
RuntimeMode testMode=RuntimeMode::Gameplay;
std::unique_ptr<TitleRuntimeState> testRuntime=std::make_unique<TitleRuntimeState>();
XrSession g_session=reinterpret_cast<XrSession>(uintptr_t{1});
XrAction g_hapticAction=reinterpret_cast<XrAction>(uintptr_t{2});
XrPath g_leftHandPath=11,g_rightHandPath=22;
XrSessionState g_sessionState=XR_SESSION_STATE_FOCUSED;
std::atomic<bool> g_capturedLeftHanded{};
std::atomic<float> g_requestedHaptics{},g_peakHaptics{},g_contactHaptics[2]{};
struct Output { XrPath path{};float amplitude{};XrDuration duration{};float frequency{};bool stop{}; };
std::vector<Output> outputs;
void Check(bool ok,const char* message)
{
    ++checks;
    if (!ok) { ++failures;std::fprintf(stderr,"FAIL: %s\n",message); }
}
bool Near(float a,float b) { return std::fabs(a-b)<0.00001f; }
uint64_t TestGetTickCount64() { return testNow; }
TitleAdapterRuntimeSnapshot RuntimeSnapshot(uint64_t now)
{ return {testRuntime->Resolve(now,MakeTitleRuntimeHeartbeatPolicy()),false}; }
}

RuntimeMode TitleAdapter_GetRuntimeMode() { return testMode; }
bool Menu_IsOpen() { return testMenu; }
extern "C" XRAPI_ATTR XrResult XRAPI_CALL xrApplyHapticFeedback(
    XrSession session,const XrHapticActionInfo* info,const XrHapticBaseHeader* feedback)
{
    Check(session==g_session&&info&&info->action==g_hapticAction&&feedback&&
        feedback->type==XR_TYPE_HAPTIC_VIBRATION,"OpenXR output has the actual session/action/vibration type");
    const auto& pulse=*reinterpret_cast<const XrHapticVibration*>(feedback);
    outputs.push_back({info->subactionPath,pulse.amplitude,pulse.duration,pulse.frequency,false});
    return XR_SUCCESS;
}
extern "C" XRAPI_ATTR XrResult XRAPI_CALL xrStopHapticFeedback(
    XrSession session,const XrHapticActionInfo* info)
{
    Check(session==g_session&&info&&info->action==g_hapticAction,"OpenXR stop has the actual session/action");
    outputs.push_back({info->subactionPath,0,0,0,true});
    return XR_SUCCESS;
}

#define GetTickCount64 TestGetTickCount64
#include "haptics_runtime_functions.inl"
#undef GetTickCount64

namespace
{
uint32_t PublishedCapabilities(GameTitle title)
{ return title==GameTitle::HaloCE?kCePublishedCapabilities:kHalo3RuntimeCapabilities; }
void Publish(bool armed=true,bool retiring=false)
{
    ++testNow; // Runtime publication deliberately refuses repeated heartbeats.
    const auto gen=testRuntime->Generation(testTitle);
    Check(testRuntime->PublishLifecycle(testTitle,gen,
        {true,armed,retiring,PublishedCapabilities(testTitle)}),"Publish current title lifecycle");
    Check(testRuntime->PublishMode(testTitle,gen,testMode)==!retiring,
        "Runtime mode publication respects current title retirement");
    Check(testRuntime->PublishHeartbeat(testTitle,gen,testNow-1),"Publish current camera heartbeat");
}
void Begin(GameTitle title=GameTitle::HaloCE)
{
    testRuntime=std::make_unique<TitleRuntimeState>();
    testMode=RuntimeMode::Shell;
    ApplyControllerHaptics(false); // Also resets the production function's static active flags.
    outputs.clear();testNow+=1000;testTitle=title;testMenu=false;
    g_sessionState=XR_SESSION_STATE_FOCUSED;g_capturedLeftHanded=false;
    g_config.haptic_intensity=0.86f;
    TitleRuntimeModuleSet modules{};
    modules.availabilityMask=TitleRuntimeAvailabilityBit(title);
    modules.moduleBases[TitleRuntimeSlotIndex(title)]=0x100000;
    Check(testRuntime->PublishModuleSet(modules,testNow-100),"Publish actual title module generation");
    testMode=RuntimeMode::Gameplay;Publish();
}
DWORD Send(WORD low,WORD high,DWORD user=0)
{
    XINPUT_VIBRATION vibration{low,high};
    return ProcessSetState(ERROR_DEVICE_NOT_CONNECTED,user,&vibration);
}
void Frame(uint64_t elapsed=41,bool tracking=true,bool refresh=true)
{
    testNow+=elapsed;
    if (refresh) testRuntime->PublishHeartbeat(testTitle,testRuntime->Generation(testTitle),testNow-1);
    ApplyControllerHaptics(tracking);
}
void Pair(float expected,const char* message)
{
    Check(outputs.size()==2&&!outputs[0].stop&&!outputs[1].stop&&
        outputs[0].path==g_leftHandPath&&outputs[1].path==g_rightHandPath&&
        Near(outputs[0].amplitude,expected)&&Near(outputs[1].amplitude,expected)&&
        outputs[0].duration==50000000&&outputs[1].duration==50000000&&
        outputs[0].frequency==XR_FREQUENCY_UNSPECIFIED&&
        outputs[1].frequency==XR_FREQUENCY_UNSPECIFIED,message);
}
bool HasApply()
{ return std::any_of(outputs.begin(),outputs.end(),[](const Output& value){return !value.stop;}); }
void StoppedPair(const char* message)
{
    Check(outputs.size()==2&&outputs[0].stop&&outputs[1].stop&&
        outputs[0].path==g_leftHandPath&&outputs[1].path==g_rightHandPath,message);
}
void TestGunRumble(GameTitle title)
{
    Begin(title);
    const auto* descriptor=TitleRegistry_Find(title);
    Check(descriptor&&(descriptor->capabilities&TitleCapability_Haptics),
        "Title descriptor admits the common haptic output");
    Check(Game_HasTitleCapability(TitleCapability_Haptics),
        "Actual armed title publication admits game and contact haptics");
    Check(Send(65535,0)==ERROR_SUCCESS,"Virtual slot zero remains connected with no physical gamepad");
    Frame();Pair(0.65f*0.86f,"Low motor gun rumble reaches both controllers at shared intensity");
    Begin(title);Send(0,65535);Frame();Pair(0.35f*0.86f,"High motor gun rumble reaches both controllers");
    Begin(title);Send(65535,65535);Frame();Pair(0.86f,"Both gun motor bands blend to full amplitude");
    outputs.clear();Frame(20);Check(outputs.empty(),"Held rumble respects the 40 ms OpenXR reapply interval");
    Frame(20);Pair(0.86f,"Held rumble is renewed after the interval");
    Send(0,0);Frame(); // The last carried held peak may be consumed once before the zero sample.
    outputs.clear();Frame();StoppedPair("A zero motor update stops both controllers");
    outputs.clear();Send(65535,0);Send(0,0);Frame();
    Pair(0.65f*0.86f,"A gunshot that starts and stops between VR frames retains its peak on both hands");
    outputs.clear();Frame();StoppedPair("A captured short gunshot stops after one applied pulse");
}
void TestIntensityAndContacts()
{
    Begin();Send(65535,65535);g_config.haptic_intensity=0;Frame();
    Check(!HasApply(),"Controller vibration zero disables both game and contact output");
    Begin();Send(65535,65535);g_config.haptic_intensity=2;Frame();Pair(1,"Intensity above one clamps safely");
    Begin();Send(65535,65535);g_config.haptic_intensity=-1;Frame();Check(!HasApply(),"Negative intensity clamps off");
    Begin();VR_PulseContactHaptics(true,0.18f);Frame();
    Check(outputs.size()==1&&outputs[0].path==g_leftHandPath&&Near(outputs[0].amplitude,0.18f*0.86f),
        "CE left-hand contact pulse reaches the left controller");
    Begin();VR_PulseContactHaptics(false,0.35f);Frame();
    Check(outputs.size()==1&&outputs[0].path==g_rightHandPath&&Near(outputs[0].amplitude,0.35f*0.86f),
        "CE right-hand contact pulse reaches the right controller");
    Begin();g_capturedLeftHanded=true;VR_PulseContactHaptics(true,0.18f);Frame();
    Check(outputs.size()==1&&outputs[0].path==g_rightHandPath,"Left-handed role mapping preserves actual physical hand output");
    Begin();Send(65535,0);VR_PulseContactHaptics(false,0.2f);Frame();
    Pair(0.65f*0.86f,"Contact feedback cannot suppress the game's stronger gun rumble");
    Begin();Send(0,65535);VR_PulseContactHaptics(true,0.8f);Frame();
    Check(outputs.size()==2&&Near(outputs[0].amplitude,0.8f*0.86f)&&Near(outputs[1].amplitude,0.35f*0.86f),
        "Contact merges independently per hand without replacing game feedback");
}
void TestStopsAndOwnership()
{
    const RuntimeMode forbidden[]{RuntimeMode::Shell,RuntimeMode::Loading,RuntimeMode::Paused,
        RuntimeMode::Cutscene,RuntimeMode::Dead,RuntimeMode::Unsupported};
    for (auto mode:forbidden)
    {
        Begin();Send(65535,65535);Frame();outputs.clear();testMode=mode;Frame();
        StoppedPair("Leaving gameplay stops both controllers for each disallowed runtime mode");
    }
    for (auto mode:{RuntimeMode::Vehicle,RuntimeMode::Turret})
    { Begin();testMode=mode;Send(65535,65535);Frame();Pair(0.86f,"Vehicle and turret modes preserve native rumble"); }
    Begin();Send(65535,65535);Frame();outputs.clear();testMenu=true;Frame();
    StoppedPair("Opening the VR menu stops both controllers");
    Begin();Send(65535,65535);Frame();outputs.clear();Frame(41,false);
    StoppedPair("Lost tracking stops both controllers");
    Begin();Send(65535,65535);Frame();outputs.clear();g_sessionState=XR_SESSION_STATE_VISIBLE;Frame();
    StoppedPair("Lost session focus stops both controllers");
    Begin();VR_PulseContactHaptics(true,0.8f);testMenu=true;Frame();testMenu=false;outputs.clear();Frame();
    Check(!HasApply(),"A contact pulse raised under the menu never replays after closing it");
    Begin();Send(65535,65535);Frame();outputs.clear();Publish(false);Frame();
    StoppedPair("An unarmed camera owner immediately stops both controllers");
    Check(Send(65535,65535)==ERROR_SUCCESS&&!g_requestedHaptics.load(),
        "Unarmed title policy clears rumble while preserving virtual gamepad connection");
    Publish();outputs.clear();Frame();Check(!HasApply(),"Rearming cannot replay a prior title's rumble");
    Send(65535,65535);outputs.clear();Frame();Pair(0.86f,"A fresh gunshot works after rearming");
    Begin();Send(65535,65535);Frame();outputs.clear();Publish(true,true);Frame();
    StoppedPair("Retiring title ownership stops both controllers");
    Begin();Send(65535,65535);Frame();outputs.clear();Frame(500,true,false);
    StoppedPair("An expired title heartbeat stops both controllers");
    Begin();Check(Send(65535,65535,1)==ERROR_DEVICE_NOT_CONNECTED,
        "Another physical controller slot keeps its native result");Frame();Check(!HasApply(),"Other slots cannot drive virtual-controller haptics");
    Check(ProcessSetState(ERROR_DEVICE_NOT_CONNECTED,0,nullptr)==ERROR_DEVICE_NOT_CONNECTED,
        "Null vibration requests preserve their native result");
}
}

int main()
{
    TestGunRumble(GameTitle::Halo3);
    // Original and Anniversary share this exact CE ownership/capability path;
    // the rendering mode never participates in any extracted haptic function.
    TestGunRumble(GameTitle::HaloCE);
    TestIntensityAndContacts();TestStopsAndOwnership();
    std::printf("Haptics production runtime: %u checks, %u failures\n",checks,failures);
    return failures?1:0;
}
