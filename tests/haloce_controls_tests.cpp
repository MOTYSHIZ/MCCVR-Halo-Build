#include "common/haloce_controls_logic.h"
#include <cstdio>
#include <limits>
#define CHECK(condition) do { if (!(condition)) { std::fprintf(stderr,"CE controls check failed line %d: %s\n",__LINE__,#condition);return 1;} } while(false)

using namespace halo_ce;
static bool Near(float a,float b) { return std::fabs(a-b)<0.0001f; }
int main()
{
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
