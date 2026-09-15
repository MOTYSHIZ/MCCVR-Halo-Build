// Exercise the actual camera -> atomic command -> XInput consumer transport.
// Only the clock, tracking freshness and title service are fakes. No MCC process.
#include <windows.h>
#include <cmath>
#include <iostream>
#include "../src/common/input_logic.h"
#include "../src/common/config.h"
#include "../src/dll/vr.h"
#include "../src/dll/title_adapter.h"

namespace {
uint64_t testNow=1000;
GameTitle testTitle=GameTitle::Halo3;
uint32_t testGeneration=1;
bool testTracking=true;
uint64_t RoomscaleTestNow() noexcept { return testNow; }
}
GameTitle TitleAdapter_GetActiveTitle() { return testTitle; }
uint32_t TitleAdapter_GetGeneration(GameTitle title)
{ return title==testTitle ? testGeneration : 0; }
bool VR_RoomscaleTrackingFresh() noexcept { return testTracking; }

// Windows headers were already included: replace call sites, not WinAPI declarations.
#define GetTickCount64 RoomscaleTestNow
#include "../src/dll/roomscale.cpp"
#undef GetTickCount64

int RunRoomscaleInputTests()
{
    int failures=0;
    auto check=[&](bool ok,const char* message) {
        if (!ok) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
    };
    const bool saved=g_config.roomscale_movement;
    g_config.roomscale_movement=true;
    for (GameTitle title : {GameTitle::Halo2,GameTitle::Halo3,GameTitle::Halo3ODST,
                           GameTitle::HaloReach,GameTitle::Halo4})
    {
        testTitle=title; ++testGeneration; testNow+=1000; testTracking=true;
        const float q[4]{0,0,0,1}, forward[3]{1,0,0};
        float body[3]{},head[3]{0,1.7f,0},ref[3]{0,1.7f,0};
        Roomscale_Input(false,0,0);
        const auto camera=[&](bool native=true) {
            Roomscale_Camera(title,native,body,head,q,forward,ref,0.328084f);
        };
        const bool eligible=RoomscaleGameplayEligible(title,RuntimeMode::Gameplay);
        Roomscale_Input(eligible,0,0); camera();
        testNow+=16; head[2]=-0.2f; Roomscale_Input(eligible,0,0); camera();
        float x=0,y=0;
        check(Roomscale_Move(x,y) && y>0 && x==0,
            "every supported title transports physical steps to native walking");

        // Import shim -> export -> forwarded export. Inner wrappers must not
        // merge; otherwise the outer merge detects its own output as manual
        // input, invalidates the epoch, and destroys the follow command.
        unsigned depth=0,merges=0;
        auto poll=[&](auto&& self,int wrappers,float physicalY) -> float {
            InputPollMergeScope scope(depth);
            float output=wrappers ? self(self,wrappers-1,physicalY) : physicalY;
            if (scope.IsOutermost()) {
                ++merges;
                Roomscale_Input(eligible && std::fabs(output)<0.24f,0,0);
                float rx=0,ry=output;
                if (Roomscale_Move(rx,ry)) output=ry;
            }
            return output;
        };
        for (int wrappers : {0,1,2}) {
            merges=0;
            check(poll(poll,wrappers,0)>0 && merges==1 && depth==0,
                "nested XInput paths preserve follow demand and merge exactly once");
        }
        check(poll(poll,2,0.75f)==0.75f,
            "nested XInput preserves an actual physical movement stick");
        x=y=0;
        check(!Roomscale_Move(x,y),"physical input cancels already-published follow immediately");
        Roomscale_Input(eligible,0,0); testNow+=16; camera();
        x=y=0;
        check(!Roomscale_Move(x,y),"physical stick release cannot replay old follow demand");
        testNow+=16; head[2]-=0.2f; camera();
        x=y=0; check(Roomscale_Move(x,y),"a fresh physical step resumes body follow");
        body[0]+=0.1f*0.328084f; testNow+=16; camera();
        check(std::fabs(ref[2]+0.1f)<1e-5f && ref[1]==1.7f,
            "only observed native horizontal travel consumes tracking offset");
        testNow+=101; x=y=0;
        check(!Roomscale_Move(x,y),"expired input/command cannot walk unattended");
        Roomscale_Input(eligible,0,0); camera();
        testTracking=false; x=y=0;
        check(!Roomscale_Move(x,y),"tracking interruption cancels body follow before another camera");
        testTracking=true; Roomscale_Input(false,0,0); camera(false);
        x=y=0; check(!Roomscale_Move(x,y),"native admission loss cancels follow without tearing down VR");
        Roomscale_Input(eligible,0,0); camera(); testNow+=16; head[0]+=0.15f; camera();
        ++testGeneration; x=y=0;
        check(!Roomscale_Move(x,y),"new generation rejects an old movement packet");
        camera(); testNow+=16; head[0]+=0.15f; camera();
        g_config.roomscale_movement=false; x=y=0;
        check(!Roomscale_Move(x,y),"toggle off cancels movement immediately");
        g_config.roomscale_movement=true;
        for (RuntimeMode mode : {RuntimeMode::Shell,RuntimeMode::Loading,RuntimeMode::Paused,
             RuntimeMode::Cutscene,RuntimeMode::Vehicle,RuntimeMode::Turret,RuntimeMode::Dead,
             RuntimeMode::Unsupported})
            check(!RoomscaleGameplayEligible(title,mode),"roomscale never admits non-gameplay modes");
    }
    // Closed-loop synthetic native walker: physical motion must end up in the
    // body, while body + remaining lean stays exactly one tracked displacement.
    // This validates feedback arithmetic, not Halo's unmeasured acceleration.
    for (int hz : {60,90,120}) for (float scale : {0.164f,0.328084f,0.656f})
    {
        testTitle=GameTitle::Halo3; ++testGeneration; testNow+=1000;
        Roomscale_Input(false,0,0);
        const float q[4]{0,0,0,1},forward[3]{1,0,0};
        float head[3]{0,1.7f,0},ref[3]{0,1.7f,0},body[3]{};
        float mx=0,my=0; bool singleMotion=true;
        for (int frame=0;frame<hz*4;++frame)
        {
            body[0]+=my*2.0f/hz*scale;
            body[1]-=mx*2.0f/hz*scale;
            const float t=std::min(1.0f,float(frame)/hz);
            head[0]=0.15f*t; head[2]=-0.3f*t;
            head[1]=1.7f-0.2f*t; // crouching must not alter the height reference
            testNow+=1000/hz;
            Roomscale_Input(true,0,0);
            Roomscale_Camera(testTitle,true,body,head,q,forward,ref,scale);
            singleMotion &= std::fabs(body[0]/scale-head[2]+ref[2]-0.3f*t)<0.0001f &&
                std::fabs(-body[1]/scale+head[0]-ref[0]-0.15f*t)<0.0001f && ref[1]==1.7f;
            mx=my=0; Roomscale_Move(mx,my);
        }
        check(singleMotion,"60/90/120 Hz follow consumes native travel once without changing height");
        check(std::hypot(body[0]/scale-0.3f,-body[1]/scale-0.15f)<=0.021f && mx==0 && my==0,
            "body reaches the physical step and stays stopped within the 2 cm deadband");
    }
    for (GameTitle title : {GameTitle::None,GameTitle::Unknown,GameTitle::HaloCE})
        check(!RoomscaleGameplayEligible(title,RuntimeMode::Gameplay),
            "roomscale cannot acquire an unsupported title");
    // A saved experimental setting cannot inject body-follow movement into
    // CE's basic VR bring-up, even with fresh tracking and physical motion.
    testTitle=GameTitle::HaloCE; ++testGeneration; testNow+=1000;
    Roomscale_Input(false,0,0);
    const float ceOrientation[4]{0,0,0,1},ceForward[3]{1,0,0};
    float ceBody[3]{},ceHead[3]{0,1.7f,0},ceReference[3]{0,1.7f,0};
    for (int sample=0;sample<3;++sample) {
        Roomscale_Input(RoomscaleGameplayEligible(testTitle,RuntimeMode::Gameplay),0,0);
        Roomscale_Camera(testTitle,true,ceBody,ceHead,ceOrientation,ceForward,ceReference,0.328084f);
        float x=0,y=0;
        check(!Roomscale_Move(x,y)&&x==0&&y==0,
            "CE experimental roomscale stays deferred with saved setting enabled");
        testNow+=16; ceHead[2]-=0.2f;
    }
    g_config.roomscale_movement=saved;
    return failures;
}
