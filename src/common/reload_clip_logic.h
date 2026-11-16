#pragma once
// THE GUN'S OWN RELOAD ANIMATION, LEARNED FROM THE GAME (Halo 3 / ODST manual reload). Pure.
//
// Campaign Evolved poses the gun from stretches of its reload animation (vr-game-conversion
// references/manual-reload.md section 4): the magazine leaves and arrives along the animation, the charging handle
// is worked along it, and the hand only picks the time. MCC's first-person animations are not decoded here: the
// game computes them, so the palette hook RECORDS them. While the game plays a reload (unseen: the hook shows the
// gun's resting pose instead), each of the gun's nodes is sampled relative to the gun's root node. That is a clip.
// Analysed, it gives the magazine's way out (played at the drop), its way in (scrubbed by how far the hand has
// pushed), and the charging handle's stroke (scrubbed by the hand's pull). Holding the gun's root where it rests
// on the hand keeps the gun from moving in the hand while it is posed (Campaign Evolved's body lock, for free).
//
// Multiplayer-safe by construction: the clip is read from, and posed onto, this frame's private first-person
// palette only (AGENTS.md).
#include <cmath>
#include <cstdint>
#include <cstring>

namespace reload_clip
{
// The palette's node matrix (game.cpp BoneMatrix: the same layout).
struct Bone
{
    float scale = 1.0f;
    float rotation[9]{1, 0, 0, 0, 1, 0, 0, 0, 1}; // columns
    float translation[3]{};
};
static_assert(sizeof(Bone) == 13 * sizeof(float), "Bone is game.cpp's BoneMatrix");

struct V3 { float x = 0, y = 0, z = 0; };
inline V3 Sub(const float a[3], const float b[3]) noexcept { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
inline float Len(V3 v) noexcept { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }
inline V3 Unit(V3 v) noexcept
{
    const float n = Len(v);
    return std::isfinite(n) && n > 1e-6f ? V3{v.x / n, v.y / n, v.z / n} : V3{};
}

inline constexpr int kMaxNodes = 16;     // the kit fp guns have 4-11 nodes
inline constexpr int kMaxFrames = 192;   // >= 4.8 s at the 25 ms sampling below
inline constexpr float kSampleSeconds = 0.025f;
inline constexpr float kMaxSeconds = 5.0f;

enum class Kind : uint8_t { Full = 0, Empty = 1 };   // the gun had rounds left when its reload began, or none

struct Clip
{
    uint64_t identity = 0;   // the fp gun's render model
    uint8_t title = 0;       // GameTitle
    Kind kind = Kind::Full;
    int nodes = 0;
    int frames = 0;
    float time[kMaxFrames]{};
    Bone bone[kMaxFrames][kMaxNodes]{};   // each node relative to node 0 (the gun's root)
};

// What a clip says about the reload, in the clip's own (root-relative) units and seconds.
struct Segments
{
    bool magazine = false;   // the magazine leaves and comes back
    float outStart = 0;      // the magazine starts to leave
    float outEnd = 0;        // ... and is gone (far enough to hide)
    float in = 0;            // it starts its final approach (the push begins here)
    float seat = 0;          // ... and is home
    float window = 0;        // how far from home the approach begins
    V3 approach{};           // unit: the way the magazine travels going in (root frame)
    bool rack = false;       // the rack part makes a stroke
    float rackStart = 0, rackFull = 0;
    float rackTravel = 0;
    V3 rackBack{};           // unit: the way the rack part is pulled (root frame)
};

inline constexpr float kLeftFraction = 0.04f;    // "has left": this fraction of the furthest the part went
inline constexpr float kHomeFraction = 0.02f;    // "home": within this fraction
inline constexpr float kOutFraction = 0.6f;      // eject: gone once this fraction out (then hidden)

inline float NodeDistance(const Clip& c, int frame, int node) noexcept
{
    return Len(Sub(c.bone[frame][node].translation, c.bone[0][node].translation));
}

// Frame 0 is the gun as the reload began: every distance is from there.
inline Segments Analyze(const Clip& c, int magazineNode, int rackNode) noexcept
{
    Segments s{};
    if (c.frames < 4 || c.nodes <= 0 || c.nodes > kMaxNodes) return s;
    if (magazineNode > 0 && magazineNode < c.nodes)
    {
        int farFrame = 0;
        float farthest = 0;
        for (int f = 0; f < c.frames; ++f)
        {
            const float d = NodeDistance(c, f, magazineNode);
            if (d > farthest) { farthest = d; farFrame = f; }
        }
        if (farthest > 1e-4f)
        {
            int left = -1, gone = -1, seat = -1;
            for (int f = 0; f <= farFrame && left < 0; ++f)
                if (NodeDistance(c, f, magazineNode) > farthest * kLeftFraction) left = f;
            for (int f = left < 0 ? 0 : left; f <= farFrame && gone < 0; ++f)
                if (NodeDistance(c, f, magazineNode) >= farthest * kOutFraction) gone = f;
            for (int f = farFrame; f < c.frames && seat < 0; ++f)
                if (NodeDistance(c, f, magazineNode) <= farthest * kHomeFraction) seat = f;
            if (left >= 0 && gone >= 0 && seat > farFrame)
            {
                // Back from the seat while the magazine keeps moving away, smoothly: the final approach. It ends
                // (going back in time) where the magazine stood still or jumped (a fresh magazine appearing in
                // the hand, or held below the well).
                int in = seat;
                while (in > farFrame)
                {
                    const float step = NodeDistance(c, in - 1, magazineNode) - NodeDistance(c, in, magazineNode);
                    if (step <= farthest * 0.001f || step > farthest * 0.2f) break;
                    --in;
                }
                const float window = NodeDistance(c, in, magazineNode);
                if (window > farthest * 0.1f)
                {
                    s.magazine = true;
                    s.outStart = c.time[left];
                    s.outEnd = c.time[gone];
                    s.in = c.time[in];
                    s.seat = c.time[seat];
                    s.window = window;
                    s.approach = Unit(Sub(c.bone[seat][magazineNode].translation, c.bone[in][magazineNode].translation));
                }
            }
        }
    }
    if (rackNode > 0 && rackNode < c.nodes)
    {
        int full = 0;
        float travel = 0;
        for (int f = 0; f < c.frames; ++f)
        {
            const float d = NodeDistance(c, f, rackNode);
            if (d > travel) { travel = d; full = f; }
        }
        int start = -1;
        for (int f = full; f >= 0; --f)
            if (NodeDistance(c, f, rackNode) <= travel * kLeftFraction) { start = f; break; }
        if (start >= 0 && full > start && travel > 1e-4f)
        {
            s.rack = true;
            s.rackStart = c.time[start];
            s.rackFull = c.time[full];
            s.rackTravel = travel;
            s.rackBack = Unit(Sub(c.bone[full][rackNode].translation, c.bone[0][rackNode].translation));
        }
    }
    return s;
}

// The frame pair around time t and the blend between them.
inline bool Locate(const Clip& c, float t, int& a, int& b, float& u) noexcept
{
    if (c.frames <= 0) return false;
    if (!(t > c.time[0])) { a = b = 0; u = 0; return true; }
    if (t >= c.time[c.frames - 1]) { a = b = c.frames - 1; u = 0; return true; }
    int lo = 0, hi = c.frames - 1;
    while (hi - lo > 1)
    {
        const int mid = (lo + hi) / 2;
        (c.time[mid] <= t ? lo : hi) = mid;
    }
    a = lo; b = hi;
    const float span = c.time[b] - c.time[a];
    u = span > 1e-6f ? (t - c.time[a]) / span : 0.f;
    return true;
}

// Re-orthonormalise a blended basis (columns), keeping the first column's direction.
inline void Orthonormalize(float r[9]) noexcept
{
    float* x = r; float* y = r + 3; float* z = r + 6;
    auto norm = [](float* v) {
        const float n = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
        if (n > 1e-8f) for (int i = 0; i < 3; ++i) v[i] /= n;
    };
    norm(x);
    const float d = x[0] * y[0] + x[1] * y[1] + x[2] * y[2];
    for (int i = 0; i < 3; ++i) y[i] -= x[i] * d;
    norm(y);
    z[0] = x[1] * y[2] - x[2] * y[1];
    z[1] = x[2] * y[0] - x[0] * y[2];
    z[2] = x[0] * y[1] - x[1] * y[0];
}

inline Bone Sample(const Clip& c, float t, int node) noexcept
{
    int a = 0, b = 0;
    float u = 0;
    if (node < 0 || node >= c.nodes || !Locate(c, t, a, b, u)) return {};
    const Bone& p = c.bone[a][node];
    const Bone& q = c.bone[b][node];
    Bone o{};
    o.scale = p.scale + (q.scale - p.scale) * u;
    for (int i = 0; i < 9; ++i) o.rotation[i] = p.rotation[i] + (q.rotation[i] - p.rotation[i]) * u;
    for (int i = 0; i < 3; ++i) o.translation[i] = p.translation[i] + (q.translation[i] - p.translation[i]) * u;
    if (u > 0 && u < 1) Orthonormalize(o.rotation);
    return o;
}

// The time in [from, to] at which `node` is `distance` from where it began (frame 0), walking from `from`
// (the part's distance may run either way along the stretch; the first crossing is taken).
inline float TimeAtDistance(const Clip& c, int node, float from, float to, float distance) noexcept
{
    if (node < 0 || node >= c.nodes || c.frames <= 0) return from;
    auto frameAt = [&](float t) {
        int f = 0;
        while (f + 1 < c.frames && c.time[f + 1] <= t + 1e-6f) ++f;
        return f;
    };
    const int f = frameAt(from);
    const int end = frameAt(to);
    const int step = end >= f ? 1 : -1;
    float prevD = NodeDistance(c, f, node);
    float prevT = c.time[f];
    const bool growing = NodeDistance(c, end, node) >= prevD;
    if (growing ? distance <= prevD : distance >= prevD) return prevT;
    for (int g = f + step; step > 0 ? g <= end : g >= end; g += step)
    {
        const float d = NodeDistance(c, g, node);
        if (growing ? d >= distance : d <= distance)
        {
            const float span = d - prevD;
            const float w = std::fabs(span) > 1e-7f ? (distance - prevD) / span : 1.f;
            return prevT + (c.time[g] - prevT) * std::fmin(std::fmax(w, 0.f), 1.f);
        }
        prevD = d;
        prevT = c.time[g];
    }
    return c.time[end];
}

// The recorder: frames are appended at most every kSampleSeconds, up to kMaxSeconds.
inline bool Append(Clip& c, float t, const Bone* nodes, int count) noexcept
{
    if (count <= 0 || count > kMaxNodes || c.frames >= kMaxFrames || t > kMaxSeconds) return false;
    if (c.frames > 0 && (count != c.nodes || t < c.time[c.frames - 1] + kSampleSeconds)) return false;
    c.nodes = count;
    c.time[c.frames] = t;
    std::memcpy(c.bone[c.frames], nodes, sizeof(Bone) * size_t(count));
    ++c.frames;
    return true;
}
} // namespace reload_clip
