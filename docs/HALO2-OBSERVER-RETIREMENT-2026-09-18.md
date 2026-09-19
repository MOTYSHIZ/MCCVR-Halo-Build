# Halo 2 observer teardown correction

Halo 3 reference behavior being matched: retire hooks after disabling ingress
and checking both active callbacks and native detour/trampoline instruction
ranges; retain dependencies and retry if cleanup cannot finish.

The H2 observer interpolation-frame path previously ignored disable/removal
errors and cleared its original pointer. Most other observer paths waited for
callback counts alone, leaving the pre-counter entry window unchecked. Several
also ignored removal failure before clearing bookkeeping. H2 does not keep a
loader reference across levels, so absent/replaced mappings are relevant.

`halo2_observer_cleanup.inl` now disables all eleven observer-related entries,
checks each exact compiled detour/original range with its own counter, then
removes them. Failed disable, busy ingress/callback or removal retains required
state for retry. Partial successful removal clears only that hook pair. Shared
addresses, generation and cached native accessors clear only after all hooks
retire. Particle cleanup uses the same policy independently. The previous
counter-only implementation remains inert as `RemoveCoreLegacy` and
`RemoveParticleGateLegacy`; no callers use them.

World-collision cleanup now also checks detour ingress. Collision, contact and
dual-fire cleanup use the existing identity-aware retirement helper rather than
blindly attempting to restore bytes in a mapping MCC may have removed. Foreign
replacement bytes still block cleanup without being overwritten. No additional
module pin, game-memory layout or native binding was introduced.

The original d77c9dd H2-to-H3 log records repeated missing-mapping cleanup errors.
That is supporting lifecycle context, not proof of the later Cairo firing or
Outskirts checkpoint crash. The supplied co-op log has no fault stack; neither
reported crash has been reproduced. This change fixes the demonstrated unsafe
cleanup decisions, not a proven attribution of those reports.

Validation: the production cleanup fixture executes 14,285 assertions over all
twelve hook entries, including disable failure, nonzero callback, zero-count
ingress, removal failure, partial retry and already-absent entries. The existing
MinHook fixture separately verifies live/decommitted/restored/foreign mappings.
Release, all 43 CTests and the Reach consistency gate pass. Logs are under
`out/refinement-20260918/` with suffix `h2-observer-cleanup`; final build log is
`build-h2-observer-cleanup-final.log`. These are local checks, not headset/co-op
acceptance. No ZIP or installation; accepted pointer remains unchanged.
