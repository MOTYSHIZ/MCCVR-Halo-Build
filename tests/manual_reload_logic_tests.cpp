#include "manual_reload_logic.h"
#include <cmath>
#include <cstdio>

namespace {
unsigned checks=0,failures=0;
void Check(bool value,const char* name){++checks;if(!value){++failures;std::printf("FAIL: %s\n",name);}}
using namespace manual_reload;
using weapon_interaction::Vec;

// A stand-in for Halo's magazine: a shot takes a loaded round; a reload moves min(capacity-loaded, reserve)
// across after a delay.
struct Game
{
    int loaded=32,reserve=96,capacity=32;
    bool reloading=false;uint64_t reloadDoneAt=0;
    bool deaf=false;int presses=0;
    reload_ammo::Sim Sim() const {return {loaded,reserve,capacity,reloading};}
    void Shoot(int n){loaded=loaded>n?loaded-n:0;}
    void Press(uint64_t now){++presses;if(!deaf&&!reloading&&loaded<capacity&&reserve>0){reloading=true;reloadDoneAt=now+600;}}
    void Tick(uint64_t now)
    {
        if(reloading&&now>=reloadDoneAt)
        {
            int take=capacity-loaded;if(take>reserve)take=reserve;
            loaded+=take;reserve-=take;reloading=false;
        }
    }
};

const Vec kWell{0,1.0f,-0.4f},kAxis{0,1,0},kRack{0,1.1f,-0.3f},kRackBack{0,0,1},kPouch{-0.25f,1.15f,0.06f};
const Vec kMagOffset{0,-0.015f,-0.035f};
Vec HandForMagazine(Vec magazine){return magazine-kMagOffset;}

struct Rig
{
    Engine engine;Settings c;Game game;Input in;Output out;uint64_t now=1000;
    explicit Rig(const char* weapon="fp_assault_rifle")
    {
        in.ready=true;in.weaponId=0xE1230042u;in.weapon=BuiltInWeapon(weapon);in.simValid=true;
        in.support={-0.2f,1.1f,-0.3f};in.supportRotation={};in.supportGrip=0;
        in.pouch=kPouch;in.pouchValid=true;
        in.geometry={true,kWell,kAxis,true,kRack,kRackBack};
        Step();
    }
    Output Step(unsigned ms=11)
    {
        now+=ms;game.Tick(now);
        in.now=now;in.sim=game.Sim();
        out=engine.Update(in,c);
        in.reloadTap=false;
        if(out.pressed)game.Press(now);
        return out;
    }
    void Wait(unsigned ms){for(unsigned t=0;t<ms;t+=11)Step();}
    void Tap(){in.reloadTap=true;Step();}
    void Grip(bool held){in.supportGrip=held?1.f:0.f;Step();}
    void MoveMagazine(Vec m,int steps=6)
    {
        const Vec from=in.support+kMagOffset;
        for(int i=1;i<=steps;++i){const float u=float(i)/float(steps);in.support=HandForMagazine(from+(m-from)*u);Step();}
    }
    void FetchAndSeat()
    {
        in.support=kPouch;Step();Grip(false);Grip(true);
        MoveMagazine(kWell+kAxis*-0.12f);
        MoveMagazine(kWell+kAxis*-0.005f,4);
    }
};
}

int main()
{
    {
        Rig rig;
        Check(rig.out.owned&&!rig.out.triggerHeld&&rig.out.loaded==32&&rig.out.chamber==1,"a fresh rifle is the game's: full and chambered");
        rig.game.Shoot(5);rig.Step();
        Check(rig.out.loaded==27&&!rig.out.triggerHeld,"shots counted from the game's count falling");

        // TAP: the magazine drops, the game reloads unseen.
        rig.Tap();
        Check(rig.out.dropped&&rig.out.phase==Phase::MagOut&&rig.out.hideMagazine,"a tap drops the magazine");
        Check(rig.out.pressed&&rig.out.buttons==rig.c.reloadButton,"and presses the game's reload at once");
        Check(rig.out.loaded==1&&rig.out.chamber==1,"the chambered round stays in the gun");
        Check(!rig.out.triggerHeld,"the chambered round can still be fired");
        rig.Wait(700);
        Check(rig.game.loaded==32&&rig.out.loaded==1,"the game ran ahead; the player's gun did not");
        Check(rig.out.reserve==rig.game.loaded+rig.game.reserve-1,"rounds the game loaded show as reserve");
        rig.game.Shoot(1);rig.Step();
        Check(rig.out.loaded==0&&rig.out.triggerHeld,"the last chambered round fired: the trigger is held");
        Check(rig.out.blockTwoHand,"no two-handed grab while a magazine is to be fetched");

        // FETCH at the pouch: a fresh grip only.
        rig.in.supportGrip=1;rig.Step();rig.Wait(400);rig.in.support=kPouch;rig.Step();
        Check(rig.out.phase==Phase::MagOut,"a grip already closed before reaching the pouch long ago does not take one");
        rig.Grip(false);rig.Grip(true);
        Check(rig.out.grabbed&&rig.out.phase==Phase::MagHeld&&rig.out.magazineInHand&&rig.out.consumeSupport,"a fresh grip at the pouch takes a magazine");

        // SEAT: travel, then the axis, then home.
        rig.MoveMagazine(kWell+kAxis*-0.12f+Vec{0.25f,0,0});
        Check(rig.out.phase==Phase::MagHeld,"beside the gun, off the axis, it is not taken in");
        rig.MoveMagazine(kWell+kAxis*-0.12f);
        Check(rig.out.phase==Phase::Pushing&&rig.out.magazineOnAxis,"under the well it locks onto the axis");
        Check(std::fabs(rig.out.magazine.x-kWell.x)<1e-4f&&std::fabs(rig.out.magazine.z-kWell.z)<1e-4f,"drawn on the axis");
        rig.MoveMagazine(kWell+kAxis*-0.005f,4);
        Check(rig.out.seated&&rig.out.phase==Phase::Idle&&!rig.out.hideMagazine,"pushed home, it seats");
        Check(rig.out.chamber==0&&rig.out.triggerHeld,"a gun that ran dry is not chambered by the seat");
        Check(rig.out.loaded==31,"it holds what the game loaded, less the round fired since");
        Check(rig.out.consumeSupport,"the still-closed grip stays the reload's");
        rig.Grip(false);
        Check(!rig.out.consumeSupport,"until let go");

        // RACK: pull the handle back.
        rig.in.support=kRack;rig.Step();
        Check(rig.out.blockTwoHand,"a hand in a rack zone with work is not the two-handed hold's");
        rig.Grip(true);
        Check(rig.out.phase==Phase::Racking&&rig.out.consumeSupport,"a fresh grip on the handle takes it");
        bool racked=false;
        for(int i=1;i<=6;++i){rig.in.support=kRack+kRackBack*(0.012f*float(i));rig.Step();racked|=rig.out.racked;}
        Check(racked&&rig.out.chamber==1&&!rig.out.triggerHeld,"pulled back, it chambers: the gun fires");
        Check(rig.out.rackPull>0.99f,"drawn fully back");
        rig.Grip(false);
        Check(rig.out.phase==Phase::Idle&&rig.out.rackPull==0,"let go, it springs home");
        rig.in.support=kRack;rig.Step();
        Check(!rig.out.blockTwoHand,"with no rack work the two-handed grip wins");
    }
    {
        // Racking off: seating chambers the round.
        Rig rig;rig.c.rack=false;
        rig.game.Shoot(32);rig.Step();
        Check(rig.out.triggerHeld&&rig.out.loaded==0,"empty");
        rig.Tap();rig.Wait(700);rig.FetchAndSeat();
        Check(rig.out.seated&&rig.out.chamber==1&&!rig.out.triggerHeld,"with racking off the seat chambers");
    }
    {
        // Battle rifle: the burst stays in the gun.
        Rig rig("fp_battle_rifle");rig.game.loaded=36;rig.game.capacity=36;rig.engine.Reset();rig.Step();
        Check(rig.out.chamber==3,"a burst weapon keeps a whole burst chambered");
        rig.Tap();
        Check(rig.out.loaded==3,"its magazine out, three rounds remain");
    }
    {
        // A weapon without an entry is the game's.
        Rig rig("fp_shotgun");
        Check(!rig.out.owned&&!rig.out.triggerHeld,"a shotgun reloads the game's way");
        rig.Tap();
        Check(!rig.out.dropped&&!rig.out.pressed,"its tap does nothing here");
    }
    {
        // Let go of a magazine short of the gun: it is gone, fetch another.
        Rig rig;rig.Tap();rig.in.support=kPouch;rig.Step();rig.Grip(false);rig.Grip(true);
        rig.MoveMagazine(kWell+kAxis*-0.30f);
        rig.Grip(false);
        Check(rig.out.lost&&rig.out.phase==Phase::MagOut,"a magazine let go short of the gun is dropped");
    }
    {
        // A late grip: closed just before reaching the pouch.
        Rig rig;rig.Tap();rig.Grip(false);rig.Grip(true);
        rig.in.support=kPouch;rig.Step(100);
        Check(rig.out.grabbed,"a grip closed a moment before arriving still takes the magazine");
    }
    {
        // Turned upside down it does not go in.
        Rig rig;rig.Tap();rig.in.support=kPouch;rig.Step();rig.Grip(false);rig.Grip(true);
        rig.in.supportRotation={1,0,0,0}; // 180 degrees about x
        rig.MoveMagazine(kWell+kAxis*-0.12f);
        Check(rig.out.phase==Phase::MagHeld,"a magazine upside down is not taken in");
    }
    {
        // Not ready (a menu, a seat): the hand lets go; the magazine stays out on that gun.
        Rig rig;rig.Tap();rig.in.support=kPouch;rig.Step();rig.Grip(false);rig.Grip(true);
        rig.in.ready=false;rig.Step();
        Check(rig.out.lost&&rig.out.phase==Phase::MagOut&&rig.out.hideMagazine,"a menu lets go of the magazine; it stays out");
        rig.in.ready=true;rig.Grip(false);rig.FetchAndSeat();
        Check(rig.out.seated,"and the reload finishes afterwards");
    }
    {
        // Each gun keeps its own count across a switch.
        Rig rig;rig.Tap();
        const uint32_t rifle=rig.in.weaponId;
        rig.in.weaponId=0xE1240043u;rig.in.weapon=BuiltInWeapon("fp_magnum");rig.game=Game{8,24,8};rig.Step();
        Check(rig.out.phase==Phase::Idle&&!rig.out.hideMagazine&&rig.out.loaded==8,"the magnum is its own gun");
        rig.in.weaponId=rifle;rig.in.weapon=BuiltInWeapon("fp_assault_rifle");rig.game=Game{32,64,32};rig.Step();
        Check(rig.out.phase==Phase::MagOut&&rig.out.hideMagazine,"back to the rifle: its magazine is still out");
    }
    {
        // A press that never takes is retried once; a game left behind is caught up, then clamped.
        Rig rig;rig.game.Shoot(10);rig.Step();
        Game stuck=rig.game;
        rig.Tap();rig.game=stuck;rig.game.reloading=false; // the game ignored the press
        rig.Wait(450);
        Check(rig.game.reloading,"an ignored press is sent again");
        rig.Wait(700);rig.FetchAndSeat();
        Check(rig.out.seated&&!rig.out.triggerHeld,"seated after the game caught up");
        // Now a game that never reloads: ours ahead, the trigger waits, presses, then takes the game's count.
        Rig dry;dry.c.rack=false;dry.game.Shoot(31);dry.Step();
        dry.Tap();dry.game.reloading=false;dry.game.reserve=0;dry.Step(); // reserve vanished (a host correction)
        dry.game.reserve=96;dry.in.support=kPouch;dry.Step();
        dry.game.reserve=0;dry.Grip(false);dry.Grip(true);
        dry.MoveMagazine(kWell+kAxis*-0.12f);dry.MoveMagazine(kWell+kAxis*-0.005f,4);
        dry.Wait(6000);
        Check(dry.engine.Ammo().Behind()==0,"a game that will not catch up is taken as it is");
    }
    {
        // A game that never answers the button: the drop's press and ONE retry, not a press every 400 ms
        // (FA 2026-11-15: 141 presses in a minute).
        Rig rig;rig.game.Shoot(5);rig.Step();rig.game.deaf=true;rig.game.presses=0;
        rig.Tap();rig.Wait(3000);
        Check(rig.game.presses==2,"one press at the drop and one retry while the magazine is out");
    }
    {
        // A short gap (a gun swapped away and back, a hitch): the magazine is still out when it comes back.
        Rig rig;rig.Tap();
        rig.in.simValid=false;rig.Step();rig.Wait(500);rig.in.simValid=true;rig.Step();
        Check(rig.out.phase==Phase::MagOut&&rig.out.hideMagazine,"after a short gap the magazine is still out");
        rig.in.support=kPouch;rig.Step();rig.Grip(false);rig.Grip(true);
        Check(rig.out.grabbed,"and a magazine can be fetched (the phase came back with the gun)");
        // A death and checkpoint revert: the game's gun comes back full; the player's count is forgotten.
        Rig dead;dead.Tap();dead.Wait(700);
        dead.in.simValid=false;dead.Wait(2500);
        dead.game=Game{32,96,32};dead.in.simValid=true;dead.Step();
        Check(dead.out.phase==Phase::Idle&&!dead.out.hideMagazine&&dead.out.loaded==32&&!dead.out.triggerHeld,
            "after a long gap (a death, a cinematic) the gun is taken as the game has it");
    }
    {
        // THE LEARNED ANIMATION. A Covenant gun is the game's until its reload has been learned.
        Rig carbine("fp_covenant_carbine");
        Check(!carbine.out.owned, "the carbine reloads the game's way until its animation is learned");
        carbine.in.geometry.learned = true; carbine.Step();
        Check(carbine.out.owned, "and by hand once it is");
        carbine.Tap();
        Check(carbine.out.dropped && carbine.out.droppedAt == carbine.now && carbine.out.triggerHeld,
              "its magazine drops (nothing stays in the gun) and the drop time goes to the palette");
        // Pushing in along a learned animation: the push depth goes to the palette and the drawn magazine goes.
        Rig rifle;
        rifle.in.geometry.learned = true; rifle.in.geometry.insertDepth = 0.15f;
        rifle.Tap();
        rifle.in.support = kPouch; rifle.Step(); rifle.Grip(false); rifle.Grip(true);
        rifle.MoveMagazine(kWell + kAxis * -0.20f);
        Check(rifle.out.phase == Phase::Pushing, "a learned 15 cm way in takes the magazine 20 cm below the well");
        Check(std::fabs(rifle.out.pushDepth - 0.20f) < 0.02f, "with 20 cm still to go");
        Check(!rifle.out.magazineInHand, "the gun's own magazine is drawn on its way in, not the mod's");
        Check(rifle.out.followValid && Length(rifle.out.follow) < 0.05f, "the hand rides the magazine");
        // The rack: a learned stroke sets the travel.
        Rig dry;
        dry.in.geometry.rackTravel = 0.04f;
        dry.Step();
        Check(std::fabs(dry.out.rackTravel - 0.04f) < 1e-6f, "a learned 4 cm stroke is the rack's travel");
    }
    {
        // THE SHOTGUN: a shell at a time, and the pump after every shot.
        Rig sg("fp_shotgun");
        sg.game = Game{6, 18, 6};
        sg.Step();
        Check(!sg.out.owned, "the shotgun is the game's until its reload has been learned");
        sg.in.geometry.learned = true; sg.in.geometry.insertDepth = 0.06f; sg.Step();
        Check(sg.out.owned && sg.out.loaded == 6 && !sg.out.triggerHeld, "then it is the player's: 6 in, chambered");
        sg.Tap();
        Check(!sg.out.dropped && sg.out.phase == Phase::Idle, "the reload button drops nothing from a tube");
        sg.game.Shoot(1); sg.Step();
        Check(sg.out.triggerHeld && sg.out.loaded == 5, "a shot empties the chamber: nothing fires until it is pumped");
        // Pumped from the two-handed hold, which keeps the hand.
        sg.in.twoHandHeld = true; sg.in.support = kRack; sg.Step();
        Check(sg.out.phase == Phase::Racking && !sg.out.consumeSupport && !sg.out.blockTwoHand,
              "the hold's hand works the pump and keeps holding");
        sg.in.support = kRack + kRackBack * 0.065f; sg.Step();
        Check(sg.out.racked && !sg.out.triggerHeld, "drawn back: a shell is chambered");
        sg.in.support = kRack; sg.Step(); sg.Step();
        Check(sg.out.phase == Phase::Idle, "pushed home: the hold is a hold again");
        // A shell from the pouch: the game is asked to load at once.
        sg.in.twoHandHeld = false; sg.in.support = kPouch; sg.Step(); sg.Grip(false); sg.Grip(true);
        Check(sg.out.grabbed && sg.out.phase == Phase::MagHeld && sg.out.pressed, "a shell is taken and the game loads");
        sg.MoveMagazine(kWell + kAxis * -0.05f);
        Check(sg.out.phase == Phase::Pushing, "at the port it goes onto its way in");
        sg.MoveMagazine(kWell + kAxis * -0.005f, 3);
        Check(sg.out.phase == Phase::Pushing && sg.out.loaded == 5, "home, it waits for the game to load a round for it");
        sg.Wait(700);
        Check(sg.out.loaded == 6 && sg.out.phase == Phase::Idle, "and is counted once the game has");
        sg.Grip(false); sg.Grip(true);
        Check(sg.out.phase == Phase::Idle && !sg.out.grabbed, "a full tube takes no more");
        Rig drop("fp_shotgun");
        drop.game = Game{3, 18, 6}; drop.in.geometry.learned = true; drop.Step();
        drop.in.support = kPouch; drop.Step(); drop.Grip(false); drop.Grip(true); drop.Grip(false);
        Check(drop.out.lost && drop.out.phase == Phase::Idle, "a shell let go of is gone; the tube is as it was");
    }
    {
        // A GRIP THAT TAKES NOTHING says why, once the late-grip window has passed.
        GripMiss seen{};
        auto missesOver=[&seen](Rig& rig,unsigned ms){int n=0;for(unsigned t=0;t<ms;t+=11){rig.Step();if(rig.out.gripMissed){++n;seen=rig.out.miss;}}return n;};
        Rig rig;rig.Tap();
        rig.in.support=kPouch+Vec{0.25f,0,0};rig.Step();rig.Grip(false);rig.Grip(true);
        Check(!rig.out.gripMissed,"not judged at the press (a hand still arriving can take one)");
        Check(missesOver(rig,500)==1,"just outside the pouch's reach: one miss");
        Check(seen.phase==Phase::MagOut&&seen.magazineOut&&seen.blockTwoHand&&
            std::fabs(seen.pouchDistance-0.25f)<1e-3f&&std::fabs(seen.pouchReach-0.20f)<1e-6f,
            "it gives the phase, the distance and the reach");
        Rig taken;taken.Tap();
        taken.in.support=kPouch+Vec{0.25f,0,0};taken.Step();taken.Grip(false);taken.Grip(true);
        taken.in.support=kPouch;
        Check(missesOver(taken,500)==0&&taken.out.phase==Phase::MagHeld,"a grip that arrives in time took one: no miss");
        Rig far;far.Tap();
        far.in.support=kPouch+Vec{0.6f,0,0};far.Step();far.Grip(false);far.Grip(true);
        Check(missesOver(far,500)==0,"a grip far from every zone is not the reload's");
        Rig two;  // no rack work: a grip on the gun is the two-handed hold's
        two.in.support=kRack+Vec{0.03f,0,0};two.Step();two.Grip(false);two.in.twoHandHeld=true;two.Grip(true);
        Check(missesOver(two,500)==0,"a grip the two-handed hold took is no miss");
        Rig lost;lost.Tap();
        lost.in.support={};lost.Step();lost.Grip(false);lost.Grip(true);
        Check(missesOver(lost,500)==1&&seen.trackingLost,"a hand reading 0,0,0 says its tracking was lost");
    }
    {
        // Not owned (no simulation read): nothing is held back.
        Rig rig;rig.in.simValid=false;rig.Step();
        Check(!rig.out.owned&&!rig.out.triggerHeld&&!rig.out.hideMagazine,"no reading of the gun: the game's reload");
    }
    {
        // THE RELOAD BUTTON: tap versus hold.
        TapFilter tap;
        auto r=tap.Update(0,true,true);Check(r.swallow&&!r.tap,"a press is held back");
        r=tap.Update(200,true,true);Check(r.swallow,"still held back");
        r=tap.Update(300,false,true);Check(r.tap&&!r.swallow,"let go quickly: a tap");
        r=tap.Update(1000,true,true);r=tap.Update(1600,true,true);
        Check(!r.swallow&&!r.tap,"held past half a second: the game's (pick up)");
        r=tap.Update(1700,false,true);Check(!r.tap,"its release is not a tap");
        r=tap.Update(2000,true,false);Check(!r.swallow,"not ours: straight to the game");
        r=tap.Update(2100,true,true);Check(!r.swallow,"a press begun as the game's stays the game's");
        r=tap.Update(2200,false,true);Check(!r.tap,"and is no tap");
    }
    std::printf("Manual reload: %u checks, %u failures\n",checks,failures);
    return failures?1:0;
}
