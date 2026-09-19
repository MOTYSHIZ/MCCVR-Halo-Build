# Gameplay HUD draw coverage

The requested reference behavior is hidden gameplay HUD, including the VR
reticle, with menus and world rendering preserved. Existing native title scopes
still run their original callbacks and bracket GPU suppression, preserving
native update work and exception-safe scope unwinding. H2 uses its independently
recognized gameplay HUD/crosshair shader roles and existing admission gates.

The previous shared GPU coverage intercepted Draw and DrawIndexed only. The
additional DrawIndexedInstanced, DrawInstanced, DrawAuto,
DrawIndexedInstancedIndirect and DrawInstancedIndirect methods now observe the
same thread-local suppression scope. H2 additionally checks shader availability,
current title, HUD option, head tracking and locomotion before recognizing a
shader. Each optional hook failure logs and leaves that draw variant stock;
none gates camera ownership.

The production include is exercised by 86 fixture checks: SDK vtable layout,
complete argument forwarding (including signed base vertex and indirect-buffer
identity), nested scope suppression, recovery, thread isolation and independent
hook installation failures. Release, all 44 CTest suites and the Reach
consistency check pass. Logs are under out/refinement-20260918 with the
hud-draw-coverage suffix. Headset visibility and any deferred/compute HUD path
not submitted within these scopes remain unverified; no runtime acceptance is
claimed.
