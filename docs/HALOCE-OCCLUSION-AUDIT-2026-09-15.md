# CE Anniversary GPU occlusion audit - September 15, 2026

Halo 3's reference behavior is a coherent world in both tracked eyes. The user's
existing Anniversary screenshot and headset report are accepted evidence that
the second view is wrong; no screenshot, game launch, or installation was
performed for this audit.

## Result

Native GPU occlusion is genuinely per view, but a missing second-view result
does **not** default to invisible. The current native frame clears the complete
next result table to `0xFF`, gathers available immediate and worker GPU query
results, and swaps the table. An unknown result is signed `-1`; the world-draw
consumer rejects an exact `0` only. This excludes an uninitialized, zero-filled
secondary GPU query slot as the explanation for the missing world geometry.

The available recordings retain per-eye common constants but do not contain the
broken frame's actual query stream. They therefore cannot establish whether a
real zero-sample query rejected a particular second-eye mesh. No GPU-query
override, global culling disable, or guessed view-index correction follows from
this audit. CPU scene membership and authored visibility are separate upstream
systems and require their own evidence.

## Pinned native evidence

Input: `out/deps/re-tools/inputs/halo1.dll`, SHA-256
`0A12DC561780F449D3F4D0DF10BB8D3BC7BE7840A5BEB2B236F672EB6CD42E6C`.

- Lookup `0x1DDC30` uses a `0x8080`-entry hash table with `0x14`-byte records.
  The first dword holds the key, and `0xFFFFFFFF` marks an unused record.
- Mesh draw `0x30F9A0` resolves a record using draw `+0x88 & ~7` for eligible
  draw flag `+0xDE & 0x20`. If the key is present, it skips the group when the
  signed short at `record + 4 + 4 * key.viewIndex` equals zero. Helper
  `0x30F620` returns one for an absent key and otherwise reads the same slot.
- Depth/material construction `0x2F35B0` examines the two signed shorts at
  `record + 4/6 + 4 * viewIndex`. Values below one select the native depth-query
  material behavior. This is distinct from the later exact-zero world rejection.
- Native draw supplies query-end virtual `+0x158` with the mesh key and slot
  `viewIndex * 2`, or `viewIndex * 2 + 1` for the alternate query draw. The
  apparent subtraction in decompiled text results from a signed byte shift.
- Backend query start/end are `0x1FBB60` / `0x1FBC00`. The end routine combines
  that slot with the key, ends the D3D11 query, and records it in the backend's
  result array. GPU result gather `0x1FBCD0` calls `GetData` without a flush and,
  for an available result, updates `record + 4 + 2 * (encodedKey & 7)`.
- At `0x45667A`, frame routine `0x455A10` clears **all `0xA0A00` bytes** of the
  alternate table to `0xFF`. It then gathers the main backend and every native
  worker backend, resets each query count, and publishes the alternate table.
  Standalone gather routine `0x1DDCE0` has the same clear/gather/swap sequence.
- The frame's primary depth loop selects target bit `0x10000` from each native
  view index and calls `0x457510` and `0x4577B0` for each admitted primary view.
  No source-player predicate or new mono-only GPU query path was found here.

These facts come from native disassembly/decompilation, not modeled D3D results
or a new headset test. They do not establish the broken frame's render targets,
query outcomes, draw counts, or visible scene completeness.

## Preserved records

- `out/ce-occlusion-draw-consumer-20260915.txt`
- `out/ce-occlusion-record-producers-20260915.txt`
- `out/ce-occlusion-query-lifetime-20260915.txt` (the two query routines were
  decoded; a later leaf-address request had no Ghidra function and was inspected
  separately by pinned-image disassembly)
- `out/ce-occlusion-table-references-20260915.txt`
- `out/ce-occlusion-cache-init-20260915.txt`
- Existing complete frame and mesh records:
  `out/ce-resume-saber-frame.txt`,
  `out/ce-world-actual-draw-and-clear-20260915.txt`, and
  `out/ce-world-model-transform-input-20260915.txt`.

No production source or enable flag was changed by this audit.
