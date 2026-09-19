#pragma once

// AIM PROVIDER -- the per-game half of the aim solution. One implementation per MCC title.
//
// The seam: the CORE (aim_solve_logic.h) is engine-free and picks the strategy from caps(); the
// PROVIDER owns every engine-specific fact behind these methods. The core hands down a CONVERGED
// setpoint; the provider actuates it. Direct drive and convergence compose -- actuate the converged
// aim, never the raw hand aim.
//
// Why this exists: MCC spans six Blam generations (CE 2001 .. H4 2012, H4 a 343 fork) plus Saber
// Anniversary renderers on CE/H2, so each title exposes its aim state, shot origin, and collision
// differently. A single hardcoded solution is impossible; this interface absorbs the divergence so
// the core stays one tested body of code. See the Engineering Vault concept
// "multi-engine-aim-provider-interface".
//
// This header is still engine-free: it declares an abstract contract over the pure types in
// aim_solve_logic.h. The concrete providers (e.g. src/dll/reach_aim_provider.*) are engine-bound.

#include "aim_solve_logic.h"

namespace aim_solve {

struct Ray    { Vec3 origin; Vec3 dir; };            // world/game space, `dir` need not be normalized
struct Angles { float yaw_deg = 0.0f; float pitch_deg = 0.0f; };

// Result of an actuation attempt. Reported so a caller can prove the REAL path ran, not merely that a
// hook installed ("installed" is not "running"), and whether the write is the replicated one.
struct ActuateResult {
    bool applied = false;                 // the provider acted this tick
    bool wrote_replicated_state = false;  // true only for a verified rung-1 write on the wire
};

// One implementation per title. Called on the game/aim path: keep it allocation-free, deterministic,
// and fail-closed (return false / applied=false and say so in the log, never write a guessed field).
class IAimProvider {
public:
    virtual ~IAimProvider() = default;

    // Which title this is, for logging/selection (e.g. "reach"). Static string, never null.
    virtual const char* title() const = 0;

    // What this title supports -- drives the core's strategy selection. May change at runtime (a
    // record located mid-session flips can_write_state), so it is queried, not cached by the core.
    virtual AimCaps caps() const = 0;

    // (B) INTENT RAY: the authored muzzle bore rotated by the live pose (has_barrel_marker), else the
    //     controller ray. Origin + direction in game space. False when unavailable this tick.
    virtual bool intent_ray(Ray* out) = 0;

    // (C) TRACE the sightline into THIS engine's own collision. Range in cm on a blocking hit; false
    //     on a miss or when the title has no trace -- the caller then HOLDS the last range. Only
    //     meaningful when caps().has_sightline_trace.
    virtual bool trace_impact_cm(const Ray& sightline, float* out_cm) = 0;

    // (C) Measured eye-minus-shot-origin offset (cm, in the core's basis); false until known. This is
    //     the `delta` converge_trace_reaim() consumes. (MCC: engine eye - base camera; CampE: the VR
    //     stereo callbacks.) Averaged to the cyclopean eye by the provider so half an IPD is not baked
    //     in as a standing bias.
    virtual bool origin_delta(Vec3* out) = 0;

    // (A) ACTUATE the CONVERGED setpoint. WriteState writes the replicated control record; StickLoop
    //     drives the game's own stick input. The core selects exactly one -- never run both at once
    //     (two drivers chasing one setpoint reads as heavy jitter). Reports what it actually did.
    virtual ActuateResult actuate(Actuation how, const Angles& converged) = 0;

    // (C, provider-owned strategies) When the core selects OriginRelocation or FixedRange, the actual
    // reconciliation lives in the engine (a firing-site hook, or the reticle-distance re-aim already
    // in the game code). This hook lets the provider run/confirm that per tick; the trace-and-reaim
    // path instead runs in the core via converge_trace_reaim(). No-op by default.
    virtual void apply_engine_convergence(Converge /*selected*/, const Ray& /*intent*/) {}
};

} // namespace aim_solve
