#include "reload_clip_logic.h"
#include <cstdio>

namespace
{
unsigned checks = 0, failures = 0;
void Check(bool value, const char* name)
{
    ++checks;
    if (!value) { ++failures; std::printf("FAIL: %s\n", name); }
}
bool Near(float a, float b, float tolerance = 1e-3f) { return std::fabs(a - b) <= tolerance; }

using namespace reload_clip;

// A rifle's empty reload: the magazine (node 1) drops 0.30 down, hangs there, and comes back up the last 0.15
// from below; the charging handle (node 2) is pulled 0.06 back and let go.
float MagazineDrop(float t)
{
    if (t < 0.2f) return 0;
    if (t < 0.5f) return 0.30f * (t - 0.2f) / 0.3f;
    if (t < 1.0f) return 0.30f;
    if (t < 1.2f) return 0.15f;   // the fresh magazine appears under the well
    if (t < 1.6f) return 0.15f * (1.6f - t) / 0.4f;
    return 0;
}
float HandlePull(float t)
{
    if (t < 2.0f || t > 2.4f) return 0;
    return t < 2.2f ? 0.06f * (t - 2.0f) / 0.2f : 0.06f * (2.4f - t) / 0.2f;
}
Clip MakeClip()
{
    Clip c{};
    for (float t = 0; t <= 2.8f; t += kSampleSeconds)
    {
        Bone nodes[3]{};
        nodes[1].translation[1] = 0.10f - MagazineDrop(t);   // rest 0.10 above the root, out downwards
        nodes[2].translation[0] = 0.05f - HandlePull(t);     // rest 0.05 ahead, pulled back
        Append(c, t, nodes, 3);
    }
    return c;
}
} // namespace

int main()
{
    const Clip c = MakeClip();
    Check(c.frames > 100 && c.nodes == 3, "the recorder samples every 25 ms");
    {
        Clip tooFast{};
        Bone n[3]{};
        Append(tooFast, 0, n, 3);
        Check(!Append(tooFast, 0.01f, n, 3) && tooFast.frames == 1, "no two samples closer than 25 ms");
        Check(!Append(tooFast, 0.1f, n, 2), "a sample with another node count is refused");
    }
    const Segments s = Analyze(c, 1, 2);
    Check(s.magazine, "the magazine leaves and comes back");
    Check(s.outStart > 0.2f && s.outStart < 0.25f, "it starts to leave at 0.2 s");
    Check(s.outEnd > 0.35f && s.outEnd < 0.42f, "and is gone once 60% out");
    Check(s.window > 0.09f && s.window <= 0.105f + 1e-4f, "the push begins no further than 35% of the way out");
    Check(s.in >= 1.30f && s.in <= 1.37f, "on the way home, at 1.3 s");
    Check(s.seat >= 1.58f && s.seat <= 1.62f, "and is home at 1.6 s");
    Check(Near(s.approach.y, 1.f, 1e-3f), "going in is up");
    Check(s.rack && Near(s.rackTravel, 0.06f, 0.002f), "the handle makes a 6 cm stroke");
    Check(s.rackStart >= 1.98f && s.rackStart <= 2.03f && s.rackFull >= 2.17f && s.rackFull <= 2.23f,
          "from 2.0 s to 2.2 s");
    Check(Near(s.rackBack.x, -1.f, 1e-3f), "pulled back");

    // Scrubbing: half-way in is 0.075 below home.
    const float half = TimeAtDistance(c, 1, s.in, s.seat, 0.075f);
    Check(Near(half, 1.4f, 0.03f), "half-way in is at 1.4 s");
    const Bone at = Sample(c, half, 1);
    Check(Near(at.translation[1], 0.10f - 0.075f, 0.004f), "sampled there, the magazine is 7.5 cm below home");
    Check(Near(TimeAtDistance(c, 1, s.in, s.seat, 0.f), s.seat, 0.03f), "pushed home: the seat");
    Check(Near(TimeAtDistance(c, 1, s.in, s.seat, 1.f), s.in, 1e-4f), "further than the window: the start");
    const float pull = TimeAtDistance(c, 2, s.rackStart, s.rackFull, 0.03f);
    Check(Near(pull, 2.1f, 0.03f), "a half pull is 2.1 s");
    Check(Near(Sample(c, pull, 2).translation[0], 0.02f, 0.004f), "with the handle 3 cm back");

    {
        // HALO 3'S ASSAULT RIFLE, as the game played it (FA 2026-11-16, root frame: x forward, y left, z up): the
        // magazine is pulled down and back ~0.15 units, then brought all the way in again. Only the last straight
        // stretch up into the well is the push.
        static const float kPath[][4]{
            {0.000f, 0, 0, 0}, {0.364f, 0, 0, 0}, {0.397f, -0.0001f, -0.0001f, -0.0021f},
            {0.430f, -0.0030f, -0.0003f, -0.0163f}, {0.464f, -0.0221f, 0.0001f, -0.0447f},
            {0.497f, -0.0639f, -0.0007f, -0.0629f}, {0.531f, -0.1012f, -0.0177f, -0.0664f},
            {0.564f, -0.1107f, -0.0245f, -0.0674f}, {0.598f, -0.1159f, -0.0255f, -0.0695f},
            {0.630f, -0.1203f, -0.0255f, -0.0717f}, {0.665f, -0.1233f, -0.0246f, -0.0739f},
            {0.698f, -0.1244f, -0.0231f, -0.0758f}, {0.730f, -0.1230f, -0.0209f, -0.0774f},
            {0.764f, -0.1181f, -0.0182f, -0.0785f}, {0.797f, -0.1031f, -0.0136f, -0.0784f},
            {0.831f, -0.0706f, -0.0049f, -0.0762f}, {0.865f, -0.0402f, 0.0048f, -0.0676f},
            {0.898f, -0.0290f, 0.0118f, -0.0492f}, {0.933f, -0.0192f, 0.0099f, -0.0351f},
            {0.964f, -0.0013f, 0.0002f, -0.0146f}, {1.000f, 0.0028f, 0.0004f, -0.0036f},
            {1.031f, 0, 0, 0}, {1.064f, 0, 0, 0}, {1.300f, 0, 0, 0}};
        Clip ar{};
        for (const auto& row : kPath)
        {
            Bone n[2]{};
            for (int a = 0; a < 3; ++a) n[1].translation[a] = row[1 + a];
            Append(ar, row[0], n, 2);
        }
        const Segments g = Analyze(ar, 1, -1);
        Check(g.magazine && g.window > 0.03f && g.window < 0.052f, "the assault rifle's push is its last ~4 hundredths");
        Check(g.approach.z > 0.7f, "and goes up into the well");
        Check(g.outStart > 0.39f && g.outStart < 0.44f && g.seat > 1.0f && g.seat < 1.04f, "out at 0.4 s, home at 1.03 s");
    }
    {
        // A clip in which the magazine never comes back (cut short) gives no insertion.
        Clip cut{};
        for (int f = 0; f < 30; ++f)
        {
            Bone n[2]{};
            n[1].translation[1] = -0.01f * float(f);
            Append(cut, kSampleSeconds * float(f), n, 2);
        }
        Check(!Analyze(cut, 1, -1).magazine, "a magazine that never comes home: no insertion");
    }
    {
        // A tactical (full) reload without a rack stroke.
        Clip full{};
        for (float t = 0; t <= 1.8f; t += kSampleSeconds)
        {
            Bone n[3]{};
            n[1].translation[1] = 0.10f - MagazineDrop(t);
            n[2].translation[0] = 0.05f;
            Append(full, t, n, 3);
        }
        const Segments f = Analyze(full, 1, 2);
        Check(f.magazine && !f.rack, "a full reload: the magazine, no rack stroke");
    }
    {
        // Sampling blends between frames and keeps the basis orthonormal.
        Clip turn{};
        Bone a[1]{}, b[1]{};
        b[0].rotation[0] = 0; b[0].rotation[1] = 1; b[0].rotation[3] = -1; b[0].rotation[4] = 0;
        Append(turn, 0, a, 1);
        Append(turn, 0.1f, b, 1);
        const Bone m = Sample(turn, 0.05f, 0);
        const float n0 = std::sqrt(m.rotation[0] * m.rotation[0] + m.rotation[1] * m.rotation[1]);
        Check(Near(n0, 1.f) && Near(m.rotation[8], 1.f), "a blended basis stays a rotation");
    }
    std::printf("Reload clip: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
