#pragma once
// Empty-hand finger gestures, after the Halo: Campaign Evolved VR mod's
// pose_for_inputs: a free hand's grip, trigger and thumb sensor pick one of the
// named hand shapes (the `pose` lines of halomccvr_hand_tracks.txt), and the
// fingers ease between shapes.
//
//   grip + trigger + thumb   fist        no grip + trigger + thumb   ok
//   grip + trigger           thumbsup    no grip + trigger           index
//   grip + thumb             pointdown   no grip                     rest
//   grip                     point
//
// "Trigger" is the trigger's touch sensor, or a pull past kTriggerPull.
// "Thumb" is any thumb sensor: A/B (X/Y) touch, the thumb rest or the stick.
// Without thumb sensors there is no gesture: the caller keeps the plain curl
// (trigger closes the index finger, grip the rest).
//
// Pure and allocation-free. The VR input thread picks and eases the shapes and
// publishes them in one atomic word (Pack); the palette hook unpacks them.
#include "hand_track_logic.h"
#include <cmath>
#include <cstdint>

namespace hand_gesture
{
using hand_track::Shape;
using hand_track::kFingers;

inline constexpr float kEaseSeconds=0.045f;           // Campaign Evolved's shape ease
inline constexpr float kGripOn=0.55f, kGripOff=0.35f; // the same thresholds the weapon gestures use
inline constexpr float kTriggerPull=0.10f;

struct Input
{
    bool valid{};          // trigger and grip were read this frame
    bool touchKnown{};     // this controller reports trigger and thumb touch
    float trigger{}, grip{};
    bool triggerTouch{}, thumbTouch{};
    // A shape the hand is holding something in (the manual reload's magazine or handle, Campaign Evolved's held
    // shapes): it wins over the gesture, and needs no touch sensors.
    Shape held{Shape::None};
};

inline Shape ShapeFor(bool grip,bool trigger,bool thumb) noexcept
{
    if(grip)
    {
        if(trigger) return thumb?Shape::Fist:Shape::ThumbsUp;
        return thumb?Shape::PointDown:Shape::Point;
    }
    if(trigger) return thumb?Shape::Ok:Shape::Index;
    return Shape::Rest;
}

// The grip with hysteresis, so a half-squeezed grip does not flicker between two shapes.
inline bool GripHeld(float grip,bool wasHeld) noexcept
{ return std::isfinite(grip)&&(wasHeld?grip>kGripOff:grip>kGripOn); }
inline bool TriggerOn(const Input& in) noexcept
{ return in.triggerTouch||(std::isfinite(in.trigger)&&in.trigger>kTriggerPull); }

// One hand: which shape it is making and how far its fingers have eased toward it.
class Hand
{
public:
    void Reset() noexcept { *this=Hand{}; }
    bool Active() const noexcept { return active_; }
    Shape Current() const noexcept { return shape_; }
    const float* Curls() const noexcept { return curls_; }
    // False (and the hand reset) when there is no gesture: no input, or the
    // table lacks the shape. A controller without touch sensors uses the same
    // table with the thumb off and the trigger pulled rather than touched
    // (Campaign Evolved). dt in seconds.
    bool Update(const hand_track::PoseTable& table,const Input& in,float dt) noexcept
    {
        const bool holding=in.held!=Shape::None&&unsigned(in.held)<unsigned(Shape::Count)&&table.set[unsigned(in.held)];
        if(!holding&&(!in.valid||!std::isfinite(in.trigger)||!std::isfinite(in.grip))) { Reset();return false; }
        if(!holding) grip_=GripHeld(in.grip,grip_);
        const bool trigger=in.touchKnown?TriggerOn(in):in.trigger>kTriggerPull;
        const Shape shape=holding?in.held:ShapeFor(grip_,trigger,in.touchKnown&&in.thumbTouch);
        const unsigned s=unsigned(shape);
        if(s>=unsigned(Shape::Count)||!table.set[s]) { Reset();return false; }
        const float* target=table.curls[s];
        if(!active_||!std::isfinite(dt)||dt<0)
        {
            for(unsigned f=0;f<kFingers;++f) curls_[f]=target[f];
        }
        else
        {
            const float step=dt>0.1f?0.1f:dt;
            const float k=1.0f-std::exp(-step/kEaseSeconds);
            for(unsigned f=0;f<kFingers;++f) curls_[f]+=(target[f]-curls_[f])*k;
        }
        shape_=shape;active_=true;
        return true;
    }
private:
    float curls_[kFingers]{};
    Shape shape_{Shape::None};
    bool grip_{}, active_{};
};

// Both hands in one 64-bit word: 6 bits per finger over the curl range, five
// fingers a hand (30 bits), side 0 the support hand, side 1 the gun hand; bits
// 60 and 61 say which hands are set. Resolution about 0.02 of a curl.
inline constexpr unsigned kBits=6;
inline constexpr float kSteps=float((1u<<kBits)-1);
inline uint32_t Quantize(float curl) noexcept
{
    if(!std::isfinite(curl)) curl=0;
    const float u=(curl-hand_track::kMinCurl)/(hand_track::kMaxCurl-hand_track::kMinCurl);
    const float c=u<0?0:(u>1?1:u);
    return uint32_t(c*kSteps+0.5f);
}
inline float Dequantize(uint32_t q) noexcept
{ return hand_track::kMinCurl+(hand_track::kMaxCurl-hand_track::kMinCurl)*(float(q&63u)/kSteps); }
inline uint64_t Pack(const bool set[2],const float curls[2][kFingers]) noexcept
{
    uint64_t word=0;
    for(unsigned side=0;side<2;++side)
    {
        if(!set[side]) continue;
        word|=uint64_t(1)<<(60+side);
        for(unsigned f=0;f<kFingers;++f)
            word|=uint64_t(Quantize(curls[side][f]))<<(side*30+f*kBits);
    }
    return word;
}
inline bool Unpack(uint64_t word,unsigned side,float out[kFingers]) noexcept
{
    if(side>1||!(word&(uint64_t(1)<<(60+side)))) return false;
    for(unsigned f=0;f<kFingers;++f) out[f]=Dequantize(uint32_t(word>>(side*30+f*kBits)));
    return true;
}
} // namespace hand_gesture
