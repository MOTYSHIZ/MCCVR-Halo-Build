# CE native HUD target restoration - September 15, 2026

## E-CE-HUD-TARGET-1: actual native binding consumer

This builds on `HALOCE-HUD-LAYOUT-EVIDENCE-2026-09-15.md` target routing.
The pinned retail SHA is
`0A12DC561780F449D3F4D0DF10BB8D3BC7BE7840A5BEB2B236F672EB6CD42E6C`.
Addresses are RVAs; structure offsets below are hexadecimal.

Initialization `1EFA2C` calls context constructor `203580`; `1EFA3D` publishes
the returned context at `2E3BDE0`. The constructor stores final vtable
`17F9D10` at `203643`. Its `+F8` entry (`17F9E08`) points to `205E40`.
That is the concrete implementation used by native target-descriptor binds,
including the Anniversary target-stack pop.

`205E40(context, descriptor)` performs these native operations:

1. Copies the full `0x48` descriptor into `context+18`. The last eight bytes
   are copied at `205E7B` into `context+58`.
2. `1DC120` updates target dimensions/reciprocals, half-pixel values and marks
   the corresponding native shader constants dirty. The binder also resets
   pending native target-related cache ranges when their count is nonzero.
3. Validates the four explicit color texture wrappers, their render-target
   flag (`wrapper+88`, bit 8), and selected resource at `surface+E0`.
4. Selects the depth wrapper from descriptor `+30`, validating flag bit 9.
   Its virtual `+E0` returns an existing DSV; the binder stores it at
   `context+D20`, or stores null when the descriptor has no depth wrapper.
5. Walks up to four color targets. Descriptor flags are four 32-bit words at
   `+0`; wrapper pointers begin at `+10`. A null wrapper with flag bit 1 uses
   the default wrapper at `context+CF0`; a null wrapper without that flag ends
   the list. Selected RTVs are stored at `context+D00`, with the count at
   `context+CF8`.
6. `2059B0` checks six bounded 128-slot shader-resource caches and unbinds
   shader views aliasing a newly selected render target. This is a native
   target-bind side effect, not an OM-only state restoration.
7. Context virtual `+120` (`17F9E30 -> 206160`) sets viewport and scissors.
   `206160` constructs both numeric structures and issues D3D setters 44/45;
   it performs no query or resource allocation.
8. `20613A` **unconditionally** calls D3D11 context slot 33,
   `OMSetRenderTargets`, using `context+CE0`, the cached count, color views and
   depth view. `206148` returns AL true. Missing/unsupported native resources
   return AL false before this call. There is no same-target deduplication.

## Native view ownership and selection

The established texture wrapper vtable is `17FB608`.

- `+D8 -> 22B7B0` selects an existing RTV from `AD5F0(wrapper)+E8` or `+F8`.
  Descriptor flag bit 6 selects the alternate table. For explicit wrappers the
  index uses signed mip at descriptor `+40`, signed face byte `+42`, signed
  array layer byte `+43`, and wrapper mip count at `+1A`:
  `((faces*layer+face)*mipCount)+mip`, where faces is 6 for wrapper flag bit 12,
  otherwise 1. Default-wrapper binds use zero mip/face/layer.
- `+E0 -> 22B830` returns existing depth view `selectedSurface+108` or `+110`,
  selected by descriptor byte `+44`. It does not create a DSV.
- `AD5F0` selects the current existing resource variant from native flags and
  wrapper `+A8/+B0`; it performs no allocation, COM query or reference change.
- Native release `22B0A0` releases/zeros the owned RTV tables, both DSVs and
  underlying resource `+E0`. It iterates both RTV tables using the `+F0` count,
  including aliases. The adapter neither takes nor releases those references.
- Width/height accessors at vtable `+18/+20` (`1F4B80/1F4BC0`) read native
  dimensions, or forward to the selected existing alternate wrapper.

No inspected restore dependency uses a COM getter, explicit AddRef/Release,
allocation, lock or log. Native D3D setters retain their normal API semantics.
This observation does not turn native raw pointers into adapter-owned pointers.

## Optional helper semantics

`haloce_hud_target.cpp` exposes a read/restore helper with no independent hook.
Its caller must retain the verified module and require a current CE render
owner/title/generation. Its optional `hud_target` contract is verified cold.

Read requires the current backend pointer/vtable and its D3D context to match.
It snapshots the exact native descriptor and view caches, then checks every
active color/depth view against its descriptor-selected, bounded native owner
table. Mip/face/layer, view-table count, resource availability and at most four
color targets are checked before using the table. A second descriptor/cache
read rejects a changing publication. Snapshot pointers are identities only;
the helper never AddRefs, Releases or submits them directly to D3D.

Restore first validates the **latest** native descriptor/view ownership for
the same backend/context. If native HUD code deliberately selected a different
target during capture, it replays that latest descriptor and returns `Changed`;
the caller must reject captured art for that transaction. It never overwrites
new native intention with the old entry descriptor. An unchanged replay returns
`Unchanged`; retired/mismatched/unavailable state or failed native binding
returns `Unavailable`. The caller restores its independently observed numeric
viewport/scissors after this native bind.

Native bind replay updates dimension constants and can unbind conflicting SRVs;
it is a deliberate native operation, not asserted to be globally side-effect
free. Module/native-context lifetime and render ownership remain required.
No release ZIP or headset success is implied by these offline checks.

## Validation

`HALOCE-HUD-TARGET-CONTRACTS.json` records nine unique executable entries, six
relative operands, eight body witnesses and six vtable pointers. It is merged
into the central manifest/generated `hud_target` group. Original pinned bytes,
decompilation and relationships are preserved under:

- `out/ce-hud-layout-target-{bind,d3d-consumer,d3d-bytes,d3d-tail,bind-deps,viewport}-20260915.txt`
- `out/ce-hud-layout-view-accessors-20260915.txt`
- `out/ce-hud-target-contract-verification-20260915.txt`

`tools/re/test_ce_hud_target_native.py` executes the actual pinned `205E40`,
surface/view accessors, dimension updates and viewport/scissor construction in
Unicorn. Only D3D setter endpoints and the security-cookie service are stubbed.
Seven cases exercise repeated identical binds, one/four color targets, a depth
target, sRGB selection, mip/cube/array indexing, the default target, depth-only
binding and the native resource failure return. Repeated same-descriptor calls
each reach the OM setter. Output is
`out/ce-hud-target-native-emulation-20260915.json` (`PASS_NATIVE_EMULATION_ONLY`).

`halomccvr_ce_hud_target_tests` exercises the production helper with explicit
native service fixtures and real D3D11 WARP RTV/DSV state. It covers restoring
actual outputs, changed native intention, count/mip/cache mismatch, retired
backend rejection and native-failure recovery. Its native callback fixture is
separate from the pinned-instruction emulator; neither represents native MCC
gameplay or headset acceptance.
