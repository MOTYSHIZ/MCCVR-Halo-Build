# Halo CE Anniversary material constants: finite native audit

This note records a negative result against the pinned `halo1.dll`, SHA-256
`0A12DC561780F449D3F4D0DF10BB8D3BC7BE7840A5BEB2B236F672EB6CD42E6C`.
Addresses below are module-relative. No material hook or behavioral change
follows from these findings. Halo 3's reference behavior remains one coherent
tracked world in each eye, with controller-directed hands and aiming.

## Registered material classes and dispatch

The constructor/registration path at `0x214E60` populates object table
`0x1BEA8B0`. Constructor literal shader flags establish the following identities;
these are not guesses from table order or unavailable RTTI.

| Table index | Flag family | Constructor | Object | Vtable | Slot `+0x18` | Slot `+0x98` |
| --- | --- | --- | --- | --- | --- | --- |
| 9 | `GLT_*` | `0x259C10` | `0x2ECBB20` | `0x17FF9B0` | `0x264030` thunk to `0x263B10` | `0x264080` |
| 10 | `GLT_TESS_*` | `0x2478F0` | `0x2EB7BA0` | `0x17FCFD8` | `0x24A030` | `0x24A1E0` |
| 28 | `TERR_LM_SRC_TEX` and `TERR_LM_SRC_VERT` | `0x253BB0` | `0x2EC12F0` | `0x17FE630` | `0x256AE0` | `0x2570A0` |

Index 0 / constructor `0x27C300` has the actual native name `base`: its name
getter `0x27C2F0` returns the string at `0x17EF118`. Although its constructor
contains `BBL_RGB_MUL_VERTEX` and related options, those option names do not
limit the class to billboards. Its slot `+0x18` body `0x27EB30` binds resources.
Constructor `0x23D380` is index 17 and carries
`LAVA_EDGE_GLOW` / `LAVA_BLEND_ADD`; it is neither index 2 nor evidence of a
generic base material. Index 2 is constructor `0x2834D0`, with distortion-blur
flags. The mapping artifact's original `writer` column means vtable `+0x18`,
which binds resources; it must not be read as the common-constant writer.

`0x2F3460` chooses the class using draw-packet `+0x4C`, queries size through
vtable `+0x90`, obtains a per-material constant buffer through `0x20A190`,
and dispatches vtable `+0x98`. Its buffer entry is selected from the table
rooted at `0x1C33E30` using packet `+0xF3` and `+0x4C`. This is distinct from
common world CB0.

## What the inspected writers do and do not consume

- GLT `0x264080` writes material colors, lighting and UV parameters. Its
  material flag at argument 3 `+0x28 & 0x10000000` supplies the already traced
  first-person projection selector at output `+0x170`.
- Terrain `0x2570A0` writes material color, detail-scale and specular inputs,
  using material data and global material values rooted at `0x2D60DE0`.
- Tessellated GLT `0x24A1E0` writes an 80-byte material block including
  material parameters and texture dimensions.

None of these three bodies consumes render-options argument 5, including
its `+0x14` field. None constructs or copies world/view/projection matrices
or object transforms. This rules out these specific `+0x98` writers as the
producer of a camera-relative world CB0 model matrix. It does not rule out
the native common-constant or model-pool producers, other classes, or another
consumer selecting a distinct projection. It is a static native negative
result, not proof that the failed headset run used any particular material.

Preserved captures:

- `out/ce-world-material-registration-20260915.txt`
- `out/ce-world-geometry-material-setup-20260915.txt`
- `out/ce-world-material-map-20260915.txt`
- `out/ce-world-base-terrain-constants-20260915.txt`
- `out/ce-world-base-name-20260915.txt`
- `out/ce-world-base-writer-20260915.txt`

## Common CB0 model pool: confirmed staging, bounded upstream result

The separate common-model upload is in `0x30F9A0`. For a draw record with
`+0xCE != -1`, it selects `0x2B0B6E0[record[+0xF3] * 4] +
sign_extend(record[+0xCE]) * 0x40`; otherwise it uses the native default
matrix at `0x1BA9BB0`. It copies the first 48 bytes to common constants
`+0x380..+0x3AF` and marks that buffer dirty. There is no view-origin
subtraction or per-eye selection in this copy.

The pool producer is `0x355170`, invoked by draw-packet builder `0x2F35B0`
once before that object's material/packet loop. Disassembly confirms that
the second argument remains the original object argument to `0x2F35B0`;
the decompiler omits it at this call. Its first argument is builder argument
6. The builder records that argument's low byte at packet `+0xF3` and the
returned pool index at packet `+0xCE`. `0x355170`:

1. Rejects a null object or object `+0x28 & 0xD`.
2. Gets that object's cached matrix through `0x71380`.
3. Appends one 64-byte element to the lane descriptor at
   `*(0x1B7B018) + argument1 * 0x20`.
4. Transposes the full 4-by-4 source matrix into that element.

`0x2F5840` selects `*(0x1B7B018) = *(0x1B7B008)` (the latter initially
points to `0x2B0B6E0`) and resets the eight lane counts. This is descriptor
selection/reset, not a demonstrated per-eye pool swap. `0x2F5980` frees
these pools; it is teardown, not the matrix producer. The eight-lane key
has not been identified as an eye index by this audit.

`0x71380` returns object `+0x70`, lazily allocating an identity matrix when
absent. It does not rebuild the matrix from the active camera. The inspected
node updater `0x2D7760` composes the node's local matrix (`+0x78`, accessor
`0x71410`) with its parent node (`+0x40`) or owner (`+0x20`). Owner-matrix
accessor `0x2D8F80` copies owner `+0x58..+0x97` and applies the owner's
three scale components at `+0x98`. These bodies do not read the render
view list or its current camera origin.

This establishes that **pool staging does not bake the selected eye's
camera origin into the matrix**. It does not prove that every upstream
object matrix is independent of gameplay camera state. A dirty owner with
an attachment at `+0x208` can refresh its matrix through
`0x2D9AE0 -> 0x3651F0`; the latter has a type-dispatched attachment
transform. Its full set of nested target-specific producers and all other
writers of object `+0x70` were outside this bounded audit. No change is
justified by the evidence gathered here, and no failed-run object/pool
contents were captured.

Additional preserved captures:

- `out/ce-world-model-pool-refs-20260915.txt`
- `out/ce-world-model-pool-indirect-refs-20260915.txt`
- `out/ce-world-model-pool-producer-20260915.txt`
- `out/ce-world-model-pool-swap-allocate-20260915.txt`
- `out/ce-world-model-transform-input-20260915.txt`
- `out/ce-world-model-pool-disasm-20260915.txt`
- `out/ce-fp-saber-bone-dirty-20260915.txt` (`0x2D7760` body)
- `out/ce-world-model-node-transform-20260915.txt`
- `out/ce-world-model-owner-dirty-transform-20260915.txt`
- `out/ce-world-model-attachment-transform-20260915.txt`
- `out/ce-world-model-attachment-disasm-20260915.txt`
