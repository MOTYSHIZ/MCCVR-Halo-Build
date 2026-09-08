#pragma once

#include <cmath>
#include <cstdint>

// Engine-independent motion and contact selection. Native adapters publish unblocked
// hand/weapon points from the same tracked frame, with stable point identities.
// Native backends own collision queries and damage; this never issues button input.
namespace contact_melee
{
constexpr unsigned kMaxPoints = 80; // Up to 64 hand nodes plus 14 weapon points.
constexpr int64_t kMaxSampleGapNs = 100'000'000;

struct Point { float x = 0, y = 0, z = 0; };
inline bool Finite(Point p) noexcept
{ return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }
inline Point Subtract(Point a, Point b) noexcept
{ return {a.x-b.x, a.y-b.y, a.z-b.z}; }
inline float Dot(Point a, Point b) noexcept
{ return a.x*b.x + a.y*b.y + a.z*b.z; }

struct TrackingToWorld
{
    Point axis[3]{{1,0,0}, {0,1,0}, {0,0,1}};
    Point origin{};
    float unitsPerMetre = 1;

    bool Valid() const noexcept
    {
        if (!Finite(origin) || !std::isfinite(unitsPerMetre) || unitsPerMetre <= 0)
            return false;
        for (unsigned i=0; i<3; ++i)
        {
            if (!Finite(axis[i]) || std::abs(Dot(axis[i],axis[i])-1) > 0.002f)
                return false;
            for (unsigned j=0; j<i; ++j)
                if (std::abs(Dot(axis[i],axis[j])) > 0.002f) return false;
        }
        return true;
    }
    Point World(Point p) const noexcept
    {
        return {origin.x + unitsPerMetre*(axis[0].x*p.x + axis[1].x*p.y + axis[2].x*p.z),
                origin.y + unitsPerMetre*(axis[0].y*p.x + axis[1].y*p.y + axis[2].y*p.z),
                origin.z + unitsPerMetre*(axis[0].z*p.x + axis[1].z*p.y + axis[2].z*p.z)};
    }
    Point Tracking(Point p) const noexcept
    {
        p = Subtract(p,origin);
        return {Dot(p,axis[0])/unitsPerMetre, Dot(p,axis[1])/unitsPerMetre,
                Dot(p,axis[2])/unitsPerMetre};
    }
    bool SetPose(const float q[4],const float p[3]) noexcept
    {
        if(!q || !p) return false;
        const float squared=q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3];
        if(!std::isfinite(squared) || squared<1e-8f) return false;
        const float inverse=1/std::sqrt(squared);
        const float x=q[0]*inverse,y=q[1]*inverse,z=q[2]*inverse,w=q[3]*inverse;
        *this={};
        axis[0]={1-2*(y*y+z*z),2*(x*y+w*z),2*(x*z-w*y)};
        axis[1]={2*(x*y-w*z),1-2*(x*x+z*z),2*(y*z+w*x)};
        axis[2]={2*(x*z+w*y),2*(y*z-w*x),1-2*(x*x+y*y)};
        origin={p[0],p[1],p[2]};
        return Valid();
    }
};

struct Frame
{
    // Predicted display time in nanoseconds, not frame number or wall-clock ms.
    int64_t timeNs = 0;
    uint64_t serial = 0, referenceEpoch = 0, shape = 0;
    uint32_t unit = UINT32_MAX;
    unsigned count = 0;
    TrackingToWorld transform{};
    // With rigidMotion, use current mesh geometry in both controller poses.
    // Authored reload/finger animation alone must never become a VR punch.
    bool rigidMotion=false;
    TrackingToWorld controllerPose{}; // Controller-local to tracking metres.
    Point points[kMaxPoints]{}; // Tracking-space metres, before collision clamp.

    bool Valid() const noexcept
    {
        if (timeNs <= 0 || !serial || !referenceEpoch || unit == UINT32_MAX ||
            !shape || !count || count > kMaxPoints || !transform.Valid() ||
            (rigidMotion && (!controllerPose.Valid() || controllerPose.unitsPerMetre!=1))) return false;
        for (unsigned i=0; i<count; ++i)
            if (!Finite(points[i])) return false;
        return true;
    }
};

struct Sweep
{
    Point start{}, end{};
    Point trackingDirection{};
    float speedMetresPerSecond = 0;
    unsigned pointIndex = 0;
};
struct Sweeps { unsigned count = 0; Sweep values[kMaxPoints]{}; };
enum class AdvanceResult { Rejected, Seeded, Duplicate, Advanced };

class Motion
{
    Frame previous_{};
public:
    void Reset() noexcept { previous_ = {}; }
    AdvanceResult Advance(const Frame& frame, float threshold, Sweeps& output) noexcept
    {
        output = {};
        if (!frame.Valid() || !std::isfinite(threshold) || threshold <= 0)
        { Reset(); return AdvanceResult::Rejected; }
        if (previous_.serial && frame.referenceEpoch == previous_.referenceEpoch &&
            frame.unit == previous_.unit && frame.shape == previous_.shape &&
            frame.count == previous_.count && frame.rigidMotion==previous_.rigidMotion)
        {
            // Repeated eyes/palette callbacks may never count as motion twice.
            if (frame.serial == previous_.serial && frame.timeNs == previous_.timeNs)
                return AdvanceResult::Duplicate;
            if (frame.timeNs <= previous_.timeNs || frame.serial <= previous_.serial)
            { Reset(); return AdvanceResult::Rejected; }
            const int64_t dt = frame.timeNs - previous_.timeNs;
            if (dt <= kMaxSampleGapNs)
            {
                const float seconds = float(double(dt)*1e-9);
                for (unsigned i=0; i<frame.count; ++i)
                {
                    const Point previousPoint=frame.rigidMotion
                        ? previous_.controllerPose.World(frame.controllerPose.Tracking(frame.points[i]))
                        : previous_.points[i];
                    const Point delta = Subtract(frame.points[i],previousPoint);
                    const float speed = std::sqrt(Dot(delta,delta))/seconds;
                    // Discontinuous/invalid tracking invalidates the whole pair,
                    // including any otherwise plausible points earlier in it.
                    if (!std::isfinite(speed) || speed > 100)
                    { previous_ = frame; output = {}; return AdvanceResult::Rejected; }
                    if (speed < threshold) continue;
                    // Transform BOTH endpoints through the current player frame.
                    // Actor locomotion/turning cannot become a physical swing.
                    const Point start = frame.transform.World(previousPoint);
                    const Point end = frame.transform.World(frame.points[i]);
                    if (!Finite(start) || !Finite(end))
                    { Reset(); output = {}; return AdvanceResult::Rejected; }
                    const float length=speed*seconds;
                    output.values[output.count++] = {start,end,
                        {delta.x/length,delta.y/length,delta.z/length},speed,i};
                }
                previous_ = frame;
                return AdvanceResult::Advanced;
            }
        }
        previous_ = frame;
        return AdvanceResult::Seeded;
    }
};

struct Hit
{
    uint32_t unit = UINT32_MAX; // Full native handle, including its salt.
    Point position{}, normal{};
    float fraction = 0;
    bool npc = false;
};
enum class ContactResult { NoStrike, Applied, NativeRejected };

// One instance per physical hand. The native backend owns collision filtering,
// authored damage, authority and effects; no button or reticle exists here.
class Hand
{
    Motion motion_{};
    bool struck_ = false;
    unsigned strikePoint_ = 0;
    int64_t strikeTime_ = 0;
    Point strikeEnd_{}, strikeDirection_{},strikeLocalPoint_{};
public:
    void Reset() noexcept { motion_.Reset(); struck_=false; }

    // Query must return the FIRST obstruction on a segment, including walls.
    // Apply must preserve hit.unit through native parameter construction and
    // return true only after submitting damage for that exact contact.
    template<class Backend>
    ContactResult Process(const Frame& frame, float threshold, Backend& backend) noexcept
    {
        Sweeps sweeps;
        const auto advance=motion_.Advance(frame,threshold,sweeps);
        if (advance==AdvanceResult::Seeded || advance==AdvanceResult::Rejected)
        { struck_=false; return ContactResult::NoStrike; }
        if (advance==AdvanceResult::Duplicate) return ContactResult::NoStrike;
        if (struck_)
        {
            // Retract the striking part at least 25 mm before another punch.
            // Continued penetration or many collider points are one strike.
            // The other hand has its own latch and never shares this interval.
            if (strikePoint_>=frame.count) return ContactResult::NoStrike;
            const Point trackedStrikePoint=frame.rigidMotion
                ? frame.controllerPose.World(strikeLocalPoint_)
                : frame.points[strikePoint_];
            if (frame.timeNs-strikeTime_<60'000'000 ||
                Dot(Subtract(trackedStrikePoint,strikeEnd_),strikeDirection_)>-0.025f)
                return ContactResult::NoStrike;
            struck_=false;
            // Retraction rearms; it must not itself damage a target behind the
            // fist. A following advancing sample can qualify a new impact.
            return ContactResult::NoStrike;
        }
        Hit selected{};
        unsigned selectedSweep=0;
        bool found=false;
        for (unsigned i=0; i<sweeps.count; ++i)
        {
            Hit hit{};
            if (!backend.Query(sweeps.values[i],hit) || !hit.npc ||
                hit.unit==UINT32_MAX || hit.unit==frame.unit ||
                !Finite(hit.position) || !Finite(hit.normal) ||
                !std::isfinite(hit.fraction) || hit.fraction<0 || hit.fraction>1)
                continue;
            if (!found || hit.fraction<selected.fraction)
            { selected=hit; selectedSweep=i; found=true; }
        }
        if (!found) return ContactResult::NoStrike;
        const auto& sweep=sweeps.values[selectedSweep];
        if (!backend.Apply(frame.unit,selected,sweep)) return ContactResult::NativeRejected;
        strikePoint_=sweep.pointIndex;
        strikeTime_=frame.timeNs;
        strikeEnd_=frame.points[strikePoint_];
        strikeLocalPoint_=frame.controllerPose.Tracking(strikeEnd_);
        strikeDirection_=sweep.trackingDirection;
        struck_=true;
        return ContactResult::Applied;
    }
};
}
