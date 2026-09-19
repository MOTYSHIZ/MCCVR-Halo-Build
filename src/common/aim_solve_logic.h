#pragma once

// AIM SOLVE -- the engine-free heart of the "overall aim solution", fed by a per-game provider.
//
// The solution has three parts (direct drive + barrel/muzzle origin + convergence). Each has a
// game-AGNOSTIC half, which lives here as pure math + policy, and a game-SPECIFIC half, which lives
// behind IAimProvider (aim_provider.h). This file has ZERO engine/game dependencies so it can be
// unit-tested off the game, the way aim_servo_logic.h is.
//
// THE LADDER (the rule the caps below encode). Find the single place the game keeps its own aim --
// AFTER input conditioning (acceleration/deadzone/magnetism), BEFORE the aim vector is derived --
// and write THAT. Rank a candidate by four tests: (1) after conditioning, so a write is 1:1 with the
// hand; (2) before the direction is derived, so every mirror recomputes from it; (3) persistent --
// write once, stop, does it hold; (4) on the wire -- does a co-op HOST follow it. The best rung is a
// state write; a closed loop around the game's own stick is the replication-safe fallback; a derived
// mirror is a measurement source, never a write target.
//
// CONVERGENCE. A direction alone cannot express "hit that": a shot from origin C parallel to a
// sightline from eye E is displaced by (E-C) at every range. Reconcile it, best-first: trace the
// sightline to its impact and re-aim through it (origin cancels; below); relocate the shot origin
// onto the sight ray at the firing site (engine hook, provider-owned); re-aim through the reticle at
// a fixed range (exact at one range only); or write the direction and let native aim-assist converge.
// The core owns the trace-and-reaim math and the strategy choice; the provider owns the trace and the
// origin hook.
//
// Reference: the vr-game-conversion playbook (references/aim.md) and CampE's src/AimConverge.cpp,
// from which the trace-and-reaim math below is ported.

#include <cmath>
#include <cstdint>

namespace aim_solve {

// ---- capabilities a per-game provider reports (the ladder, made data) ----------------------------
struct AimCaps {
    // Actuation -- which writable aim state this title exposes (best-first).
    bool can_write_state       = false; // rung 1: a control record UPSTREAM OF REPLICATION
    bool has_stick_loop        = false; // rung 2: a closed loop around the game's own stick input
    // A derived aim vector is NOT a foundation (a mirror the owner re-stamps); it is never a cap.

    // Convergence -- how the shot origin is reconciled with the sightline (best-first).
    bool has_sightline_trace   = false; // trace-and-reaim: range measured in the engine's own collision
    bool has_origin_relocation = false; // firing-site substitution: shot leaves the sight ray
    bool has_fixed_range_reaim = false; // re-aim through the reticle at a fixed distance (one-range-exact)
    // else the floor is DirectionOnly: write the direction, let the game's native aim-assist converge

    // Intent ray -- source of the aim direction.
    bool has_barrel_marker     = false; // aim from the authored muzzle bore, not the raw hand ray

    // CONSEQUENCE, not a strategy input: writing the aim moves the game camera, so view-keyed systems
    // (audio, aim-culling, HUD projection, arm IK) follow the HAND. The CORE DOES NOT CONSUME THIS --
    // it is a note the provider/integration must act on; it is surfaced here so the fact travels with
    // the caps rather than being rediscovered per title.
    bool camera_follows_aim    = false;
};

// Actuation FOUNDATIONS. None = no writable state and no stick loop -> the caller must leave the
// game's own aim alone (rungs 3/4, synth input / mesh cosmetic, are not foundations and not here).
enum class Actuation { WriteState, StickLoop, None };
// Convergence strategies, best-first. DirectionOnly is the universal floor (needs nothing but the
// direction write plus the game's native aim-assist).
enum class Converge  { TraceReaim, OriginRelocation, FixedRange, DirectionOnly };

// Best-available with a guaranteed floor. The core branches on caps, never on the game.
inline Actuation select_actuation(const AimCaps& c) {
    if (c.can_write_state) return Actuation::WriteState;
    if (c.has_stick_loop)  return Actuation::StickLoop;
    return Actuation::None;
}
inline Converge select_converge(const AimCaps& c) {
    if (c.has_sightline_trace)   return Converge::TraceReaim;
    if (c.has_origin_relocation) return Converge::OriginRelocation;
    if (c.has_fixed_range_reaim) return Converge::FixedRange;
    return Converge::DirectionOnly;
}

// ---- pure geometry -------------------------------------------------------------------------------
struct Vec3 { float x = 0.0f, y = 0.0f, z = 0.0f; };

// ANGLE/VECTOR CONVENTION -- a PRECONDITION, not a suggestion. The core works in the single basis:
//   unit(yaw,pitch) = { cos(yaw)cos(pitch), sin(yaw)cos(pitch), sin(pitch) }   [yaw about +Z, x fwd]
// `delta` handed to converge_trace_reaim MUST live in this same basis, and the yaw/pitch read back
// are in it. Mapping the game's own frame into and out of this basis is the PROVIDER'S job. A delta
// supplied in a different frame produces a plausible-but-wrong correction that the magnitude rail
// below cannot catch -- the classic frame error whose tell is a value near a right angle, not a bad
// hand pose. Nothing pure can validate the caller's basis, so this contract is enforced by the
// provider (and its unit test), not here.

struct ConvergeRails {
    float min_divergence_cm  = 3.0f;   // below this eye-vs-origin offset: do NOTHING (leashed no-op)
    float max_correction_deg = 20.0f;  // above this: decline (wrong range/delta, not extreme aim)
    float range_floor_cm     = 30.0f;  // near-contact: 1/range would blow up; nothing to aim at anyway
};

// Range smoothing. The correction goes as 1/range, so sweeping off a near wall onto a far one would
// snap the aim; EMA it. A MISS must hold the last range (sky has none) -- the caller skips feeding on
// a miss rather than passing a fabricated range here.
inline float ema_range(float cur_cm, float sample_cm, float alpha) {
    if (!(cur_cm > 0.0f)) return sample_cm;
    return cur_cm + (sample_cm - cur_cm) * alpha;
}
// Standard time-constant EMA weight for a per-tick feed (tau in ms, dt in seconds).
inline float ema_alpha(float tau_ms, float dt_s) {
    if (!(tau_ms > 0.0f) || !(dt_s > 0.0f)) return 1.0f;
    const float a = 1.0f - std::exp(-(dt_s * 1000.0f) / tau_ms);
    return a < 0.0f ? 0.0f : (a > 1.0f ? 1.0f : a);
}

// Recover (yaw,pitch) [degrees] from a direction in the core's basis. False on a degenerate vector.
inline bool angles_from_dir(const Vec3& d, float& yaw_deg, float& pitch_deg) {
    const float len = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
    if (!std::isfinite(len) || !(len > 1.0e-6f)) return false;
    constexpr float R2D = 57.295779513082320f;
    yaw_deg = std::atan2(d.y, d.x) * R2D;
    float c = d.z / len;
    if (c < -1.0f) c = -1.0f; else if (c > 1.0f) c = 1.0f;
    pitch_deg = std::asin(c) * R2D;
    return true;
}

// TRACE-AND-REAIM. Bend (yaw,pitch) [degrees] so a shot from the shot origin passes through the point
// the sightline hits: aim = normalize(delta + range * sightline), delta = eye - shot_origin (cm, in
// the basis above). The shot origin cancels, so nothing depends on two coordinate spaces agreeing.
// Returns false and LEAVES THE ANGLES UNTOUCHED whenever the correction is not live or is implausible,
// so every caller degrades to the uncorrected setpoint -- safe to apply unconditionally.
inline bool converge_trace_reaim(float& yaw_deg, float& pitch_deg,
                                 const Vec3& delta, float range_cm, const ConvergeRails& r) {
    const float m2 = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
    if (!(m2 > r.min_divergence_cm * r.min_divergence_cm)) return false; // leashed: exact no-op
    if (!std::isfinite(range_cm) || !(range_cm > r.range_floor_cm))      return false;

    constexpr float D2R = 0.017453292519943295f;
    constexpr float R2D = 57.295779513082320f;
    const float cp = std::cos(pitch_deg * D2R);
    const float ux = cp * std::cos(yaw_deg * D2R);
    const float uy = cp * std::sin(yaw_deg * D2R);
    const float uz = std::sin(pitch_deg * D2R);

    const float tx = delta.x + ux * range_cm;
    const float ty = delta.y + uy * range_cm;
    const float tz = delta.z + uz * range_cm;
    const float len = std::sqrt(tx * tx + ty * ty + tz * tz);
    if (!std::isfinite(len) || !(len > 1.0f)) return false;

    const float ny = std::atan2(ty, tx) * R2D;
    float c = tz / len;
    if (c < -1.0f) c = -1.0f; else if (c > 1.0f) c = 1.0f;
    const float np = std::asin(c) * R2D;
    if (!std::isfinite(ny) || !std::isfinite(np)) return false;

    // Sanity rail: a large correction means the range or the delta is wrong (a wrong big correction in
    // VR is a weapon that fires sideways), so decline rather than swing the aim.
    float dy = ny - yaw_deg;
    while (dy > 180.0f)  dy -= 360.0f;
    while (dy < -180.0f) dy += 360.0f;
    if (std::fabs(dy) > r.max_correction_deg || std::fabs(np - pitch_deg) > r.max_correction_deg)
        return false;

    yaw_deg = ny;
    pitch_deg = np;
    return true;
}

} // namespace aim_solve
