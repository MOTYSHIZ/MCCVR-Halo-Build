#include "../src/common/weapon_interaction_logic.h"
#include <cstdio>
#include <limits>
#include <initializer_list>

using namespace weapon_interaction;
unsigned checks{},failures{};
void Check(bool ok,const char* message)
{ ++checks;if(!ok){++failures;std::printf("FAIL: %s\n",message);} }

struct Rig
{
    State state;
    Settings c;
    Sample s;
    Vec pouch{},holster{};
    Rig(GameTitle title=GameTitle::Halo3,bool left=false,int location=0)
    {
        c.reload=c.holsters=true;c.leftHanded=left;c.holsterLocation=location;
        s.title=title;s.generation=3;s.space=7;s.ready=true;s.now=1000;
        s.head={0,1.65f,0};s.primary={left?-0.22f:0.22f,1.25f,-0.45f};
        s.support={left?0.25f:-0.25f,1.20f,-0.40f};
        Zones(s,c,pouch,holster);state.Update(s,c);
    }
    Output Step(unsigned delta=20) {s.now+=delta;return state.Update(s,c);}
    Output GrabMagazine() {s.support=pouch;s.supportGrip=1;return Step();}
    Output InsertMagazine()
    {
        s.support=s.primary+Rotate(s.primaryRotation,{0,-0.06f,-0.04f});Step(80);Step(80);
        s.supportGrip=0;return Step();
    }
    Output GrabHolster(){s.primary=holster;s.primaryGrip=1;return Step();}
    Output DrawHolster()
    {s.primary={c.leftHanded?-0.2f:0.2f,1.25f,-0.55f};Step(80);return Step(80);}
};

int main()
{
    for(int title=1;title<=6;++title)for(bool left:{false,true})for(int location:{0,1})
        for(bool slide:{false,true})for(bool click:{false,true})
    {
        Rig r(static_cast<GameTitle>(title),left,location);
        r.c.holsterSlide=slide;r.c.holsterClick=click;r.Step();
        auto o=r.GrabHolster();
        Check(o.swapRequested==click&&o.grabbedHolster==(slide||click),
            "holster checkboxes select click, draw, both or neither in all titles and hands");
        Check(r.DrawHolster().swapRequested==(slide&&!click),"both modes cannot produce a second swap on draw");
        for(int n=0;n<30;++n)Check(!r.Step().swapRequested,"held holster click cannot repeat after cooldown");
        r.s.primaryGrip=0;r.Step();
        Check(r.GrabHolster().swapRequested==click,"new released click rearms at holster");
    }
    for(float radius:{0.08f,0.20f,0.40f})
    {
        Rig r;r.c.holsterClick=true;r.c.holsterRadius=radius;r.c.zoneRadius=0.08f;r.Step();
        r.s.primary=r.holster+Vec{radius-0.001f,0,0};r.s.primaryGrip=1;
        Check(r.Step().swapRequested,"holster accepts just inside its independent radius");
        Rig outside;outside.c.holsterClick=true;outside.c.holsterRadius=radius;outside.Step();
        outside.s.primary=outside.holster+Vec{radius+0.001f,0,0};outside.s.primaryGrip=1;
        Check(!outside.Step().swapRequested,"holster rejects just outside radius");
        outside.s.primary=outside.holster;
        Check(!outside.Step().swapRequested,"moving already-held grip into click zone does not switch");
        Rig pouch;pouch.c.zoneRadius=radius;pouch.c.holsterRadius=0.08f;
        pouch.s.support=pouch.pouch+Vec{radius-0.001f,0,0};pouch.s.supportGrip=1;
        Check(pouch.Step().pickedMagazine,"pouch radius independent of holster radius");
    }
    for(float radius:{0.06f,0.18f,0.30f})for(bool inside:{false,true})
    {
        Rig r;r.c.insertRadius=radius;r.GrabMagazine();
        r.s.support=r.s.primary+Vec{radius+(inside?-0.001f:0.001f),-0.06f,-0.04f};
        r.Step(80);r.Step(80);r.s.supportGrip=0;
        Check(r.Step().reloadRequested==inside,"insertion uses the selected independent radius");
    }
    {
        Rig r;r.GrabHolster();r.c.holsterClick=true;
        Check(!r.Step().swapRequested&&!r.DrawHolster().swapRequested,"changing holster mode cancels held draw");
        Rig h;h.c.holsterClick=true;h.Step();h.s.otherAction=true;
        Check(!h.GrabHolster().swapRequested,"click holster cannot chord with a trigger or button");
        Rig shortDraw;shortDraw.c.holsterRadius=0.08f;shortDraw.c.drawDistance=0.10f;
        shortDraw.GrabHolster();shortDraw.s.primary=shortDraw.holster+Vec{0,0,-0.23f};
        Check(shortDraw.Step(160).swapRequested,"small holster admits deliberately shortened draw");
        Rig longDraw;longDraw.c.drawDistance=0.50f;longDraw.GrabHolster();
        longDraw.s.primary=longDraw.holster+Vec{0,0,-0.40f};
        Check(!longDraw.Step(160).swapRequested,"long draw threshold prevents premature switching");
    }
    for(bool left:{false,true})for(unsigned step:{8u,11u,14u})
    {
        Rig r(GameTitle::HaloCE,left);r.c.needleShake=true;r.s.weaponGraph=0x55EA2D6F6C10C375ull;r.Step();
        r.s.primaryGrip=1;Check(r.Step().consumePrimary,"Needler shake owns deliberate weapon grip");
        const float base=r.s.primary.y;
        unsigned requests=0;
        for(int n=1;n<=100;++n)
        {
            r.s.primary.y=base+0.08f*std::sin(n*step*0.020f);
            requests+=r.Step(step).reloadRequested?1:0;
        }
        Check(requests==1,"Needler alternating strokes request exactly one reload at multiple sample rates");
        for(int n=0;n<30;++n)Check(!r.Step().reloadRequested,"held shake cannot repeat");
    }
    for(int reason=0;reason<9;++reason)
    {
        Rig r(GameTitle::HaloCE);r.c.needleShake=true;r.s.weaponGraph=0x55EA2D6F6C10C375ull;r.Step();
        r.s.primaryGrip=1;r.Step();
        switch(reason)
        {
        case 0:r.c.needleShake=false;break;
        case 1:r.s.weaponGraph=0;break;
        case 2:r.s.title=GameTitle::Halo3;break;
        case 3:r.s.otherAction=true;break;
        case 4:r.s.ready=false;break;
        case 5:r.s.primaryGrip=0;break;
        case 6:r.s.now+=250;break;
        case 7:++r.s.space;break;
        case 8:for(int i=0;i<20;++i)r.Step(100);break;
        }
        for(int i=0;i<8;++i)
        {
            r.s.primary.y+=(i%2?-.15f:.15f);
            Check(!r.Step(100).reloadRequested,"shake cancellation cannot become a delayed reload");
        }
    }
    for(int motion=0;motion<4;++motion)
    {
        Rig r(GameTitle::HaloCE);r.c.needleShake=true;r.s.weaponGraph=0x55EA2D6F6C10C375ull;r.Step();
        r.s.primaryGrip=1;r.Step();
        for(int i=0;i<16;++i)
        {
            if(motion==0) {r.s.primary.y+=.04f;r.s.head.y+=.04f;} // walk/bob together
            if(motion==1) r.s.primary.y+=.02f; // single sweep
            if(motion==2) r.s.primary.y+=(i%2?.02f:-.02f); // jitter
            if(motion==3) r.s.primary.y+=(i%2?.15f:-.15f); // implausibly fast
            Check(!r.Step(motion==3?8:100).reloadRequested,"translation, single sweep, jitter and tracking jumps cannot shake-reload");
        }
    }
    for(int title=1;title<=6;++title)
    {
        Rig r(static_cast<GameTitle>(title));r.c.needleShake=true;r.s.weaponGraph=123;r.Step();r.s.primaryGrip=1;
        Check(!r.Step().consumePrimary,"unrecognized model cannot acquire shake grip");
        Check(NeedleWeapon(static_cast<GameTitle>(title),0x55EA2D6F6C10C375ull)==(title==5),
            "CE fingerprint cannot classify a different engine's weapon");
    }
    for(int title=1;title<=6;++title) for(bool left:{false,true}) for(int location:{0,1})
    {
        Rig r(static_cast<GameTitle>(title),left,location);
        auto o=r.GrabMagazine();
        Check(o.pickedMagazine&&o.consumeSupport&&!o.consumePrimary&&o.releaseTwoHand&&o.supportHaptic>0&&!o.buttons,
            "all titles/hands/locations: pouch grab owns only support grip, frees support aim, gives feedback");
        o=r.InsertMagazine();
        Check(o.reloadRequested&&!o.swapRequested&&o.buttons==0x4000&&o.pulseUntil==r.s.now+120,
            "pouch-to-weapon release produces one reload request with finite lifetime");
        Check(o.primaryHaptic>0&&o.supportHaptic>0,"reload completion acknowledges both hands");
        for(int n=0;n<5;++n) Check(!r.Step().reloadRequested,"held completed state never repeats reload edge");
        Check(!r.Step(20).buttons,"reload pulse ends at exact deadline");
        r.Step(100);r.Step(100);r.Step(100);r.Step(100);
        o=r.GrabHolster();
        Check(o.grabbedHolster&&o.consumePrimary&&!o.consumeSupport&&o.releaseTwoHand&&!o.buttons,
            "holster acquisition owns only primary grip and does not switch early");
        o=r.DrawHolster();
        Check(o.swapRequested&&o.buttons==0x8000&&o.primaryHaptic>0,"drawing switches once using native input");
        for(int n=0;n<35;++n) Check(!r.Step().swapRequested,"held grip outside holster cannot repeatedly cycle weapons");
        r.s.primaryGrip=0;Check(r.Step().consumePrimary,"release frame still belongs to claimed holster press");
        Check(!r.Step().consumePrimary,"holster ownership ends after release");
    }
    for(int binding=0;binding<8;++binding)
    {
        Rig r;r.c.reloadButton=Button(binding);r.Step();r.GrabMagazine();
        Check(r.InsertMagazine().buttons==Button(binding),"all configured native reload transports preserved");
        Rig h;h.c.swapButton=Button(binding);h.Step();h.GrabHolster();
        Check(h.DrawHolster().buttons==Button(binding),"all configured weapon switch transports preserved");
    }
    Check(!Button(-1)&&!Button(8)&&!Button(1000),"invalid layouts never invent an input");
    for(bool reload:{false,true})for(bool holsters:{false,true})
    {
        Rig r;r.c.reload=reload;r.c.holsters=holsters;r.Step();
        Check(r.GrabMagazine().pickedMagazine==reload,"manual reload independently opt-in");
        Rig h;h.c.reload=reload;h.c.holsters=holsters;h.Step();
        Check(h.GrabHolster().grabbedHolster==holsters,"holsters independently opt-in");
    }
    {
        Rig r;r.s.supportGrip=1;r.Step();r.s.support=r.pouch;
        Check(!r.Step().pickedMagazine,"entering pouch with already held ordinary grip does not claim it");
        r.s.supportGrip=0;r.Step();Check(r.GrabMagazine().pickedMagazine,"fresh grip after release can acquire pouch");
        r.s.supportGrip=0;auto o=r.Step(160);
        Check(!o.buttons&&!o.reloadRequested,"dropping magazine at pouch cannot reload");
    }
    {
        Rig h;h.GrabHolster();h.s.primaryGrip=0;
        Check(!h.Step(160).buttons,"letting go before draw cancels swap");
        Rig r;r.GrabMagazine();r.s.support={0,0,0};r.s.supportGrip=0;
        Check(!r.Step(160).buttons,"release away from weapon discards gesture without native input");
    }
    // Each unavailable state cancels both partial interactions and blocks the
    // held grip on recovery. These correspond to shipping adapter admission.
    for(int reason=0;reason<14;++reason)for(bool holster:{false,true})
    {
        Rig r;if(holster)r.GrabHolster();else r.GrabMagazine();
        const Sample previous=r.s;const Settings settings=r.c;
        switch(reason)
        {
        case 0:r.s.ready=false;break; // focus, pause, menu, vehicle, death, tracking
        case 1:r.s.dualWield=true;break;
        case 2:++r.s.generation;break;
        case 3:++r.s.space;break;
        case 4:r.s.title=GameTitle::HaloCE;break;
        case 5:r.s.primary.x=std::numeric_limits<float>::quiet_NaN();break;
        case 6:r.s.headRotation.w=0;break;
        case 7:r.s.now+=250;break;
        case 8:r.c.leftHanded=true;break;
        case 9:r.c.reload=r.c.holsters=false;break;
        case 10:r.c.reloadButton=r.c.swapButton=Button(2);break;
        case 11:r.s.primaryGrip=std::numeric_limits<float>::infinity();break;
        case 12:r.s.primaryGrip=2;break;
        case 13:r.s.supportGrip=-1;break;
        }
        auto o=r.Step();Check(!o.buttons&&!o.reloadRequested&&!o.swapRequested,"state invalidation cancels in-flight request");
        const auto now=r.s.now;r.s=previous;r.c=settings;r.s.now=now;r.Step();
        o=holster?r.DrawHolster():r.InsertMagazine();
        Check(!o.buttons&&!o.reloadRequested&&!o.swapRequested,"recovery cannot complete stale gesture");
    }
    {
        Rig r;r.GrabMagazine();r.s.otherAction=true;
        Check(!r.InsertMagazine().buttons,"shooting/button chord cancels manual action");
        Rig h;h.GrabHolster();h.s.otherAction=true;
        Check(!h.DrawHolster().buttons,"shooting/button chord cancels holster action");
    }
    {
        Rig r;r.GrabMagazine();for(int i=0;i<42;++i)r.Step(100);
        Check(!r.InsertMagazine().buttons,"unfinished interaction expires after four seconds");
        Rig h;h.GrabHolster();for(int i=0;i<42;++i)h.Step(100);
        Check(!h.DrawHolster().buttons,"stale holster grab cannot become a delayed switch");
    }
    // Rigid translation/yaw leave both body zones and the physical reload
    // gesture unchanged, including a rolled primary controller.
    for(float yaw:{-2.2f,-0.5f,0.5f,2.8f})
    {
        Rig r;
        const Quat q{0,std::sin(yaw/2),0,std::cos(yaw/2)};
        const Vec offset{3,0.4f,-2};
        r.s.head=Rotate(q,r.s.head)+offset;r.s.primary=Rotate(q,r.s.primary)+offset;
        r.s.support=Rotate(q,r.s.support)+offset;r.s.headRotation=q;r.s.primaryRotation=q;
        Zones(r.s,r.c,r.pouch,r.holster);r.Step();r.GrabMagazine();
        Check(r.InsertMagazine().reloadRequested,"body gestures are tracking-space yaw/translation invariant");
    }
    for(int title=1;title<=6;++title)
    {
        const auto t=static_cast<GameTitle>(title);
        Check(Fresh(1100,1000,t,t,3,3,true),"fresh same-title/generation publication admitted");
        Check(!Fresh(1151,1000,t,t,3,3,true)&&!Fresh(999,1000,t,t,3,3,true),"stale/future sample rejected");
        Check(!Fresh(1100,1000,t,t,3,4,true)&&!Fresh(1100,1000,t,GameTitle::None,3,3,true),"foreign title/generation rejected");
        Check(!Fresh(1100,1000,t,t,3,3,false)&&!Fresh(1100,0,t,t,3,3,true),"unavailable or absent publication rejected");
    }
    for(float pitch:{-1.57079633f,1.57079633f})
    {
        Rig r;r.s.headRotation={std::sin(pitch/2),0,0,std::cos(pitch/2)};
        Check(Zones(r.s,r.c,r.pouch,r.holster),"looking vertically at pouch retains yaw from horizontal head right");
        r.GrabMagazine();Check(r.InsertMagazine().reloadRequested,"looking straight down/up does not lose reload zone");
    }
    {
        Rig r;r.GrabMagazine();r.state.Cancel();
        auto o=r.Step();
        Check(o.consumeSupport&&!o.buttons,"lost XR sync cancels command but retains claimed grip ownership");
        r.c.reload=r.c.holsters=false;o=r.Step();
        Check(o.consumeSupport&&!o.buttons,"disabling options does not turn a held magazine grip into a bumper");
        r.s.supportGrip=0;r.Step();Check(!r.Step().consumeSupport,"cancelled press relinquishes ownership only after release");
    }
    {
        Rig r;r.s.ready=false;r.s.supportGrip=0;r.Step();
        r.s.ready=true;
        Check(!r.GrabMagazine().pickedMagazine,"held grip on restored availability cannot use an earlier unavailable release");
        r.s.supportGrip=0;r.Step();Check(r.GrabMagazine().pickedMagazine,"release after restored availability rearms normally");
        r.s.supportGrip=std::numeric_limits<float>::quiet_NaN();r.s.ready=false;
        Check(r.Step().consumeSupport,"inactive XR action cannot fabricate grip release");
        r.s.ready=true;r.s.supportGrip=1;
        Check(r.Step().consumeSupport&&!r.InsertMagazine().buttons,"inactive-action recovery cannot resurrect magazine transaction");
    }
    std::printf("Weapon interactions: %u checks, %u failures\n",checks,failures);
    return failures?1:0;
}
