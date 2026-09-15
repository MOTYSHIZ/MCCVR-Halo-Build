#include "common/haloce_controls_logic.h"
#include "common/haloce_pause_logic.h"
#include <cstdio>
#include <limits>
#define CHECK(condition) do { if (!(condition)) { std::fprintf(stderr,"CE controls check failed line %d: %s\n",__LINE__,#condition);return 1;} } while(false)

using namespace halo_ce;
static bool Near(float a,float b) { return std::fabs(a-b)<0.0001f; }
int main()
{
    // Reproduce the reported native-pause/render mismatch. The engine has
    // stopped, but the compositor still targets stereo until reconciled.
    NativePausePresentation pause;
    CHECK(pause.Observe(3,true,false,false,1)==PauseRequest::None);
    CHECK(pause.Observe(3,true,true,false,10)==PauseRequest::None);
    CHECK(pause.Observe(3,true,true,false,59)==PauseRequest::None);
    CHECK(pause.Observe(3,true,true,false,60)==PauseRequest::Enter);
    CHECK(pause.Observe(3,true,true,true,61)==PauseRequest::None);
    CHECK(pause.Observe(3,true,true,true,261)==PauseRequest::None);
    // Unavailable player/clock state does not invent a native unpause.
    CHECK(pause.Observe(3,false,false,true,300)==PauseRequest::None);
    CHECK(pause.Observe(3,true,false,true,350)==PauseRequest::None);
    CHECK(pause.Observe(3,true,false,true,400)==PauseRequest::Exit);
    CHECK(pause.Observe(3,true,false,false,401)==PauseRequest::None);
    CHECK(pause.Observe(3,true,true,false,500)==PauseRequest::None);
    CHECK(pause.Observe(4,true,true,false,551)==PauseRequest::None);
    CHECK(pause.Observe(4,true,true,false,601)==PauseRequest::Enter);
    CHECK(pause.Observe(4,true,false,true,650)==PauseRequest::None);
    CHECK(pause.Observe(4,false,false,true,690)==PauseRequest::None);
    CHECK(pause.Observe(4,true,false,true,700)==PauseRequest::None);
    CHECK(pause.Observe(4,true,false,true,750)==PauseRequest::Exit);
    CHECK(pause.Observe(0,true,true,false,900)==PauseRequest::None);
    // e524d21 Save & Quit: native pause succeeds, then camera ownership
    // expires while halo1.dll and its generation remain in the MCC shell.
    // Losing the native clock is unknown during a level, but losing the
    // level's presentation ownership must release its head-locked screen.
    NativePausePresentation levelPause;
    CHECK(levelPause.ObserveOwned(7,true,true,true,false,1000)==PauseRequest::None);
    CHECK(levelPause.ObserveOwned(7,true,true,true,false,1050)==PauseRequest::Enter);
    CHECK(levelPause.ObserveOwned(7,true,false,false,true,1100)==PauseRequest::None);
    CHECK(levelPause.ObserveOwned(7,false,false,false,true,1600)==PauseRequest::Exit);
    CHECK(levelPause.ObserveOwned(7,false,false,false,false,1601)==PauseRequest::None);
    // Residual native paused data cannot reacquire a detached presentation.
    CHECK(levelPause.ObserveOwned(7,false,true,true,false,1800)==PauseRequest::None);
    CHECK(levelPause.ObserveOwned(7,false,true,true,false,1900)==PauseRequest::None);
    // Same-generation re-entry starts its own full native mismatch delay.
    CHECK(levelPause.ObserveOwned(7,true,true,true,false,2000)==PauseRequest::None);
    CHECK(levelPause.ObserveOwned(7,true,true,true,false,2049)==PauseRequest::None);
    CHECK(levelPause.ObserveOwned(7,true,true,true,false,2050)==PauseRequest::Enter);
    // If Save & Quit interrupts entry's debounce, it cannot carry forward.
    NativePausePresentation interruptedPause;
    CHECK(interruptedPause.ObserveOwned(8,true,true,true,false,3000)==PauseRequest::None);
    CHECK(interruptedPause.ObserveOwned(8,false,false,false,false,3100)==PauseRequest::None);
    CHECK(interruptedPause.ObserveOwned(8,true,true,true,false,3200)==PauseRequest::None);
    CHECK(interruptedPause.ObserveOwned(8,true,true,true,false,3250)==PauseRequest::Enter);
    CHECK(interruptedPause.ObserveOwned(0,false,true,true,true,3300)==PauseRequest::Exit);
    CHECK(!AllowStockScreen(true,true,false)); // retain failed-frame isolation
    CHECK(AllowStockScreen(true,true,true)); // visible immediately after fade
    CHECK(AllowStockScreen(true,false,false)); // ordinary shell
    CHECK(!AllowStockScreen(false,true,true)); // preserve other-title admission

    ControlAdmission admitted{true,true,true,false,false,false,false,false};
    CHECK(OnFootControls(admitted));
    for (int index=0;index<8;++index)
    {
        auto rejected=admitted;
        switch (index)
        {
        case 0:rejected.hasControlledUnit=false;break;
        case 1:rejected.onFoot=false;break;
        case 2:rejected.nativeFirstPerson=false;break;
        case 3:rejected.inputBlocked=true;break;
        case 4:rejected.lookBlocked=true;break;
        case 5:rejected.paused=true;break;
        case 6:rejected.cinematic=true;break;
        case 7:rejected.presentationBlocked=true;break;
        }
        CHECK(!OnFootControls(rejected));
    }
    RenderContext context{};
    context.tracking.generation=context.reference.generation=7;
    context.tracking.spaceEpoch=context.reference.spaceEpoch=4;
    context.tracking.serial=1;context.referenceRevision=1;
    auto& rig=context.tracking.controllers;
    rig.padValid=true;rig.turnSmooth=false;rig.turnSnapDeg=45;
    rig.turnX=1;
    ControlTurnState turn;
    CHECK(turn.Step(context,true,1)==0); // deflected takeover is consumed
    rig.turnX=0;++context.tracking.serial;
    CHECK(turn.Step(context,true,1.01)==0);
    rig.turnX=1;++context.tracking.serial;
    CHECK(Near(turn.Step(context,true,1.02),-0.785398163f));
    CHECK(turn.Step(context,true,1.021)==0); // repeated native call, same XR sample
    --context.tracking.serial;CHECK(turn.Step(context,true,1.022)==0);
    ++context.tracking.serial;
    ++context.tracking.serial;
    CHECK(turn.Step(context,true,1.03)==0); // held stick is not another snap
    rig.turnX=0;++context.tracking.serial;CHECK(turn.Step(context,false,1.04)==0);
    rig.turnX=-1;++context.tracking.serial;CHECK(turn.Step(context,false,1.05)==0);
    ++context.tracking.serial;CHECK(turn.Step(context,true,1.06)==0);
    rig.turnX=0;++context.tracking.serial;CHECK(turn.Step(context,true,1.07)==0);
    rig.turnX=-1;++context.tracking.serial;
    CHECK(Near(turn.Step(context,true,1.08),0.785398163f));
    rig.turnX=0;++context.tracking.serial;CHECK(turn.Step(context,true,1.09)==0);
    rig.padValid=false;++context.tracking.serial;CHECK(turn.Step(context,true,1.10)==0);
    rig.padValid=true;rig.turnX=1;++context.tracking.serial;
    CHECK(turn.Step(context,true,1.11)==0); // stale-to-held recovery requires centering
    ++context.referenceRevision;++context.tracking.serial;
    CHECK(turn.Step(context,true,1.12)==0);
    rig.turnSmooth=true;rig.turnSmoothDegS=120;
    ++context.tracking.serial;
    CHECK(Near(turn.Step(context,true,1.13),-0.020943951f));
    CHECK(turn.Step(context,true,1.134)==0);
    CHECK(turn.Step(context,true,1.136)==0);
    ++context.tracking.serial;
    CHECK(Near(turn.Step(context,true,1.14),-0.020943951f)); // elapsed time retained
    ++context.tracking.serial;CHECK(Near(turn.Step(context,true,3),-0.20943951f));
    rig.turnX=0.15f;++context.tracking.serial;CHECK(turn.Step(context,true,3.01)==0);
    rig.turnX=std::numeric_limits<float>::quiet_NaN();++context.tracking.serial;
    CHECK(turn.Step(context,true,3.02)==0);
    rig.turnX=1;rig.turnSmooth=false;++context.tracking.serial;
    CHECK(turn.Step(context,true,3.03)==0);

    auto& camera=context.camera;
    camera.forward={1,0,0};camera.up={0,0,1};camera.verticalFov=1;
    camera.viewport=camera.window={0,0,100,100};camera.nearPlane=.01f;camera.farPlane=100;
    float x=9,y=9;
    CHECK(HeadRelativeMovement(context,0,1,x,y));CHECK(Near(x,0)&&Near(y,1));
    // XR +90deg about up faces native +Y (left); forward input follows it.
    context.tracking.headOrientation={0,.707106781f,0,.707106781f};
    CHECK(HeadRelativeMovement(context,0,1,x,y));CHECK(Near(x,-1)&&Near(y,0));
    CHECK(HeadRelativeMovement(context,.3f,.4f,x,y));
    CHECK(Near(std::sqrt(x*x+y*y),.5f));
    context.reference.orientation=context.tracking.headOrientation;
    CHECK(HeadRelativeMovement(context,0,1,x,y));CHECK(Near(x,0)&&Near(y,1));
    const float savedX=x,savedY=y;
    ++context.reference.spaceEpoch;
    CHECK(!HeadRelativeMovement(context,0,1,x,y));CHECK(x==savedX&&y==savedY);
    context.reference.spaceEpoch=context.tracking.spaceEpoch;
    context.reference.orientation={};context.tracking.headOrientation={.707106781f,0,0,.707106781f};
    CHECK(!HeadRelativeMovement(context,0,1,x,y));CHECK(x==savedX&&y==savedY);
    std::puts("CE controls: native admission, shared snap transitions, smooth elapsed time, head-relative movement and stale rejection passed");
    return 0;
}
