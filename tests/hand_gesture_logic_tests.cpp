#include "hand_gesture_logic.h"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {
unsigned checks=0,failures=0;
void Check(bool value,const char* name){++checks;if(!value){++failures;std::printf("FAIL: %s\n",name);}}
using namespace hand_gesture;
using hand_track::Shape;

hand_track::PoseTable DefaultPoses()
{
    static char text[8192];
    strcpy_s(text,hand_track::kDefaultText);
    hand_track::Library library{};
    (void)hand_track::Parse(text,library);
    return library.poses;
}
Input Touch(float trigger,float grip,bool triggerTouch,bool thumb)
{
    Input in{};in.valid=true;in.touchKnown=true;in.trigger=trigger;in.grip=grip;
    in.triggerTouch=triggerTouch;in.thumbTouch=thumb;return in;
}
}

int main()
{
    // Campaign Evolved's table, verbatim.
    Check(ShapeFor(true,true,true)==Shape::Fist,"grip + trigger + thumb is a fist");
    Check(ShapeFor(true,true,false)==Shape::ThumbsUp,"grip + trigger, thumb up, is a thumbs-up");
    Check(ShapeFor(true,false,true)==Shape::PointDown,"grip + thumb is point down");
    Check(ShapeFor(true,false,false)==Shape::Point,"grip alone points");
    Check(ShapeFor(false,true,true)==Shape::Ok,"trigger + thumb is OK");
    Check(ShapeFor(false,true,false)==Shape::Index,"trigger alone is the index pose");
    Check(ShapeFor(false,false,true)==Shape::Rest&&ShapeFor(false,false,false)==Shape::Rest,"nothing is the relaxed hand, thumb or not");

    // Trigger: the touch sensor, or a pull.
    Check(TriggerOn(Touch(0,0,true,false)),"a touched trigger counts");
    Check(TriggerOn(Touch(0.2f,0,false,false)),"a pulled trigger counts without touch");
    Check(!TriggerOn(Touch(0.05f,0,false,false)),"a resting finger off the sensor does not");
    // Grip: hysteresis.
    Check(!GripHeld(0.5f,false)&&GripHeld(0.6f,false),"grip closes past 0.55");
    Check(GripHeld(0.4f,true)&&!GripHeld(0.3f,true),"and opens below 0.35");
    Check(!GripHeld(NAN,true),"a broken grip value is open");

    const auto poses=DefaultPoses();
    for(unsigned s=1;s<=unsigned(Shape::Ok);++s)
        Check(poses.set[s],"the built-in file defines every gesture shape");

    // A hand: snaps to its first shape, then eases with a 45 ms time constant.
    Hand hand;
    Check(hand.Update(poses,Touch(0,1,true,true),-1)&&hand.Current()==Shape::Fist,"first frame: fist");
    for(unsigned f=0;f<kFingers;++f)
        Check(std::fabs(hand.Curls()[f]-poses.curls[unsigned(Shape::Fist)][f])<1e-6f,"first frame snaps to the shape");
    Check(hand.Update(poses,Touch(0,1,true,false),0.045f)&&hand.Current()==Shape::ThumbsUp,"thumb lifted: thumbs-up");
    {
        const float from=poses.curls[unsigned(Shape::Fist)][4],to=poses.curls[unsigned(Shape::ThumbsUp)][4];
        const float expected=from+(to-from)*(1.0f-std::exp(-1.0f));
        Check(std::fabs(hand.Curls()[4]-expected)<1e-4f,"one time constant moves the thumb 63% of the way");
    }
    for(int i=0;i<40;++i) hand.Update(poses,Touch(0,1,true,false),0.011f);
    Check(std::fabs(hand.Curls()[4]-poses.curls[unsigned(Shape::ThumbsUp)][4])<1e-3f,"and settles on the shape");
    // A long hitch does not jump past the ease limit.
    Hand slow;slow.Update(poses,Touch(0,0,false,false),-1);
    slow.Update(poses,Touch(0,1,false,false),5.0f);
    const float target=poses.curls[unsigned(Shape::Point)][1],start=poses.curls[unsigned(Shape::Rest)][1];
    Check(std::fabs(slow.Curls()[1]-(start+(target-start)*(1.0f-std::exp(-0.1f/kEaseSeconds))))<1e-4f,
        "a frame longer than 0.1 s eases as if it were 0.1 s");
    // Half-squeezed grip does not flicker.
    Hand steady;steady.Update(poses,Touch(0,0.6f,false,false),-1);
    Check(steady.Current()==Shape::Point,"grip past 0.55 points");
    steady.Update(poses,Touch(0,0.45f,false,false),0.01f);
    Check(steady.Current()==Shape::Point,"easing the grip to 0.45 still points");
    steady.Update(poses,Touch(0,0.3f,false,false),0.01f);
    Check(steady.Current()==Shape::Rest,"letting go relaxes the hand");

    // No touch sensors, no gesture: the caller keeps the plain curl.
    Hand plain;Input noTouch=Touch(1,1,true,true);noTouch.touchKnown=false;
    Check(!plain.Update(poses,noTouch,0.01f)&&!plain.Active(),"no thumb sensor, no gesture");
    Input invalid=Touch(1,1,true,true);invalid.valid=false;
    Check(!plain.Update(poses,invalid,0.01f),"no input, no gesture");
    // A file without the shape: no gesture for that shape.
    hand_track::PoseTable sparse{};sparse.set[unsigned(Shape::Rest)]=true;
    Hand partial;
    Check(partial.Update(sparse,Touch(0,0,false,false),-1),"rest is defined");
    Check(!partial.Update(sparse,Touch(0,1,false,false),0.01f)&&!partial.Active(),"a missing shape falls back");

    // Packing: both hands in one word, about 0.02 of a curl.
    bool set[2]{true,true};
    float curls[2][kFingers]{{-0.25f,0,0.33f,0.8f,1},{1,0.5f,0.1f,-0.1f,0.62f}};
    const uint64_t word=Pack(set,curls);
    for(unsigned side=0;side<2;++side)
    {
        float out[kFingers]{};
        Check(Unpack(word,side,out),"a set hand unpacks");
        for(unsigned f=0;f<kFingers;++f) Check(std::fabs(out[f]-curls[side][f])<=0.0101f,"within half a step");
    }
    set[1]=false;
    float out[kFingers]{};
    const uint64_t supportOnly=Pack(set,curls);
    Check(Unpack(supportOnly,0,out)&&!Unpack(supportOnly,1,out),"an unset hand does not unpack");
    Check(!Unpack(0,0,out)&&!Unpack(word,2,out),"an empty word or a bad side has nothing");
    float extreme[2][kFingers]{{-9,9,NAN,1,-0.25f},{}};
    set[0]=true;
    Check(Unpack(Pack(set,extreme),0,out)&&out[0]==hand_track::kMinCurl&&out[1]==hand_track::kMaxCurl&&
          std::isfinite(out[2]),"out-of-range curls are clamped, a broken one is finite");

    std::printf("Hand gestures: %u checks, %u failures\n",checks,failures);
    return failures?1:0;
}
