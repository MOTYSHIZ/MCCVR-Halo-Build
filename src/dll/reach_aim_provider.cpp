// Reach's aim provider (see reach_aim_provider.h). OBSERVE scaffold: logs what the provider would
// drive, never touches the live aim. Engine-bound (reads VR_GetAimPose, g_config); the pure core it
// feeds is src/common/aim_solve_logic.h + aim_provider.h.

#include "reach_aim_provider.h"

#include "../common/aim_provider.h"
#include "vr.h"
#include "../common/config.h"
#include "../common/log.h"

#include <windows.h>
#include <cstdint>

namespace {
using namespace aim_solve;

// Rotate v by unit quaternion q = (x,y,z,w): q * v * conj(q). Local copy so this TU has no
// dependency on game.cpp's file-local helper.
void quat_rotate(const float q[4], const float v[3], float out[3]) {
    const float tx = 2.0f * (q[1] * v[2] - q[2] * v[1]);
    const float ty = 2.0f * (q[2] * v[0] - q[0] * v[2]);
    const float tz = 2.0f * (q[0] * v[1] - q[1] * v[0]);
    out[0] = v[0] + q[3] * tx + (q[1] * tz - q[2] * ty);
    out[1] = v[1] + q[3] * ty + (q[2] * tx - q[0] * tz);
    out[2] = v[2] + q[3] * tz + (q[0] * ty - q[1] * tx);
}

class ReachAimProvider final : public IAimProvider {
public:
    const char* title() const override { return "reach"; }

    AimCaps caps() const override {
        AimCaps c;
        // Rung 1 (a control record UPSTREAM OF REPLICATION) is NOT yet available: Reach exposes only
        // the derived aim vector (unit+0x214), a mirror. Until the record is located and co-op
        // host-follow-verified, actuation falls to the stick loop.
        c.can_write_state       = false;
        c.has_stick_loop        = true;   // Reach steers via Game_ComputeAimStick, like Halo 3
        c.has_sightline_trace   = false;  // no aim trace into Blam collision yet
        c.has_origin_relocation = true;   // R-V10/R-V27 firing-site eye substitution, foot + vehicle
        c.has_fixed_range_reaim = true;   // ComputeAimStick steers through the crosshair-distance point
        c.has_barrel_marker     = g_config.gun_barrel_aim != 0;
        c.camera_follows_aim    = true;   // Reach writes the aim, so the view follows the hand
        return c;
    }

    // The mount-calibrated aim pose -- the same corrected controller-local ray the visible weapon,
    // muzzle, reticle and bullets consume in Game_ComputeAimStick. Returned in VR space; mapping into
    // the game-aim basis is deferred to the ACTIVE step (see the header), so the OBSERVE log's angles
    // are VR-space and only prove routing/selection, not final aim.
    bool intent_ray(Ray* out) override {
        if (!out) return false;
        float q[4], p[3];
        if (!VR_GetAimPose(q, p)) return false;
        const float fwd[3] = {0.0f, 0.0f, -1.0f};
        float d[3];
        quat_rotate(q, fwd, d);
        out->origin = Vec3{p[0], p[1], p[2]};
        out->dir    = Vec3{d[0], d[1], d[2]};
        return true;
    }

    bool trace_impact_cm(const Ray&, float*) override { return false; }  // no aim trace on Reach
    bool origin_delta(Vec3*) override { return false; }                  // Reach uses origin relocation

    // OBSERVE ONLY: record what would be driven; do not drive (applied = false).
    ActuateResult actuate(Actuation how, const Angles& converged) override {
        s_last_how = how;
        s_last_angles = converged;
        return {};
    }

    void apply_engine_convergence(Converge cv, const Ray&) override { s_last_conv = cv; }

    static Actuation s_last_how;
    static Converge  s_last_conv;
    static Angles    s_last_angles;
};

Actuation ReachAimProvider::s_last_how = Actuation::None;
Converge  ReachAimProvider::s_last_conv = Converge::DirectionOnly;
Angles    ReachAimProvider::s_last_angles{};

} // namespace

void ReachAimProvider_ObserveTick() {
    using namespace aim_solve;
    static ReachAimProvider provider;
    static SolveState state;

    // ~90 Hz aim tick; the range tau matches CampE's convergence smoothing. Result is intentionally
    // discarded -- OBSERVE mode never drives.
    (void)solve_and_actuate(provider, state, ConvergeRails{}, 1.0f / 90.0f, 120.0f);

    static uint64_t last_ms = 0;
    const uint64_t now = GetTickCount64();
    if (now - last_ms >= 2000) {
        last_ms = now;
        const AimCaps c = provider.caps();
        LOG("AIMPROVIDER reach (observe): actuation=%d converge=%d barrel=%d "
            "would-yaw=%.1f would-pitch=%.1f",
            static_cast<int>(select_actuation(c)), static_cast<int>(select_converge(c)),
            c.has_barrel_marker ? 1 : 0,
            ReachAimProvider::s_last_angles.yaw_deg, ReachAimProvider::s_last_angles.pitch_deg);
    }
}
