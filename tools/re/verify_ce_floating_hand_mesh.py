"""Verify the CE wrist-local floating filter against the official HCEEK mesh.

Read-only kit/tag inputs, synthetic tracked poses, compiled production palette
builder. No game process, game-file writes or headset acceptance. The official
kit's own tag descriptors establish layouts; no third-party tag definitions.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "out/pydeps"))
import pefile

KIT_SHA = "FC9E2B6193C6F6D9FF988278B0D39A983747F3FDBDECA7CAFADD28F1F0C53E73"
CAMERA = (100, 200, 300)


def add(a, b): return tuple(x+y for x, y in zip(a, b))
def sub(a, b): return tuple(x-y for x, y in zip(a, b))
def mul(a, s): return tuple(x*s for x in a)
def dot(a, b): return sum(x*y for x, y in zip(a, b))
def cross(a, b): return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])
def length(a): return math.sqrt(dot(a, a))
def direction(m, p): return tuple(sum(m[1+c*3+r]*p[c] for c in range(3)) for r in range(3))
def point(m, p): return add(m[10:13], mul(direction(m, p), m[0]))
def inverse(m, p):
    delta = sub(p, m[10:13])
    return tuple(dot(m[1+c*3:4+c*3], delta)/m[0] for c in range(3))


def inspect_official_kit(path):
    raw = path.read_bytes()
    assert hashlib.sha256(raw).hexdigest().upper() == KIT_SHA
    image = pefile.PE(data=raw, fast_load=True).get_memory_mapped_image()
    def u32(va): return struct.unpack_from("<I", image, va-0x400000)[0]
    def text(va): return image[va-0x400000:].split(b"\0", 1)[0].decode()
    def block(va, name, size):
        assert text(u32(va)) == name and u32(va+20) == size
    block(0xc4a168, "gbxmodel", 0xe8)
    block(0xc49368, "model_node_block", 0x9c)
    block(0xc49724, "gbxmodel_geometry_part_block", 0x84)
    block(0xc49470, "model_vertex_uncompressed_block", 0x44)
    block(0xc4993c, "gbxmodel_geometry_block", 0x30)
    # Fields are 16 bytes: type/name/definition/unused. Primary kit descriptors
    # pin the local palette and the two weighted vertex indices explicitly.
    for va, kind, name in (
        (0xc49420, 3, "node0 index*"), (0xc49430, 3, "node1 index*"),
        (0xc49440, 15, "node0 weight*"), (0xc49450, 15, "node1 weight*"),
        (0xc49868, 2, "num_nodes*!"), (0xc49878, 0x27, "local_node_table"),
        (0xc49888, 2, "node index*!")):
        assert u32(va) == kind and text(u32(va+4)) == name
    assert u32(0xc49880) == 24


def read_mesh(path):
    data = path.read_bytes()
    u32 = lambda at: struct.unpack_from(">I", data, at)[0]
    assert data[36:40] == b"mod2" and u32(0x40) & 2  # parts have local nodes
    assert u32(0xec) == 0 and u32(0x104) == 1 and u32(0x110) == 1
    count = u32(0xf8)
    assert count == 37
    cursor = 0x128
    names, parents, rest = [], [], []
    for index in range(count):
        at = cursor+index*0x9c
        names.append(data[at:at+32].split(b"\0", 1)[0].decode())
        parent = struct.unpack_from(">h", data, at+36)[0]
        translation = struct.unpack_from(">3f", data, at+40)
        q = struct.unpack_from(">4f", data, at+52)
        assert abs(dot(q, q)-1) < .001 and (parent == -1 or 0 <= parent < index)
        basis = []
        for axis in ((1, 0, 0), (0, 1, 0), (0, 0, 1)):
            t = mul(cross(q[:3], axis), 2)
            basis.extend(add(add(axis, mul(t, q[3])), cross(q[:3], t)))
        if parent >= 0:
            translation = point(rest[parent], translation)
            basis = [v for i in range(3) for v in direction(rest[parent], basis[i*3:i*3+3])]
        rest.append((1, *basis, *translation))
        parents.append(parent)
    cursor += count*0x9c
    assert u32(cursor+64) == 1
    cursor += 0x4c
    assert u32(cursor+76) == 0
    cursor += 0x58
    part_count = u32(cursor+36)
    assert part_count == 4
    cursor += 0x30
    parts = cursor
    cursor += part_count*0x84
    vertices, arm_wrist_blends, root_weights = [], 0, 0
    for part in range(part_count):
        at = parts+part*0x84
        count, compressed, index_records = u32(at+32), u32(at+44), u32(at+56)
        local_count = data[at+107]
        assert 0 < local_count <= 24
        local = data[at+108:at+108+local_count]
        assert all(i < len(names) for i in local)
        for index in range(count):
            vertex = cursor+index*68
            position = struct.unpack_from(">3f", data, vertex)
            n0, n1 = struct.unpack_from(">hh", data, vertex+56)
            w0, w1 = struct.unpack_from(">ff", data, vertex+60)
            assert 0 <= n0 < local_count and -1 <= n1 < local_count
            assert 0 <= w0 <= 1 and 0 <= w1 <= 1 and abs(w0+w1-1) < .00001
            n0, n1 = local[n0], local[n1] if n1 >= 0 else -1
            root_weights += int(n0 == 0 or (n1 == 0 and w1 > 0))
            if n1 >= 0 and "forearm" in names[n0] and "wriste" in names[n1] and w0*w1 > 0:
                assert names[n0][6] == names[n1][6]
                arm_wrist_blends += 1
            vertices.append((position, n0, n1, w0, w1))
        cursor += count*68+compressed*32+index_records*6
    assert root_weights == 0 and arm_wrist_blends == 71
    return data, names, parents, rest, vertices, arm_wrist_blends


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--kit", type=Path, default=ROOT/"out/deps/re-tools/inputs/halo_tag_test.exe")
    parser.add_argument("--mesh", type=Path, default=ROOT/"out/ce-hands-hceek-tags/tags/characters/cyborg/fp/fp.gbxmodel")
    parser.add_argument("--graph", type=Path, default=ROOT/"out/ce-fp-tag-fixtures/assault_rifle.bin")
    parser.add_argument("--adapter-exe", type=Path, default=ROOT/"out/build/release/Release/halomccvr_ce_first_person_tests.exe")
    args = parser.parse_args()
    inspect_official_kit(args.kit)
    mesh, names, parents, rest, vertices, blend_count = read_mesh(args.mesh)
    graph = args.graph.read_bytes()
    count = struct.unpack_from("<I", graph)[0]
    assert len(graph) == 4+count*64
    graph_names = [graph[4+i*64:4+i*64+32].split(b"\0", 1)[0].decode() for i in range(count)]
    assert all(name in graph_names for name in names)
    source = []
    for name in graph_names:
        m = rest[names.index(name)] if name in names else rest[names.index("frame r wriste")]
        source.append((*m[:10], *add(m[10:13], CAMERA)))
    authored = b"".join(struct.pack("<13f", *m) for m in source)
    arms = {i for i, name in enumerate(graph_names) if "upperarm" in name or "forearm" in name}
    assert len(arms) == 4
    sides = []
    for index in range(len(names)):
        current = index
        while current > 0 and "upperarm" not in names[current]: current = parents[current]
        sides.append(names[current][6] if current > 0 else "root")
    assert set(sides[1:]) == {"l", "r"}
    cases = []
    with tempfile.TemporaryDirectory(prefix="ce-floating-mesh-", dir=ROOT/"out") as scratch:
        before, after = Path(scratch)/"input.bin", Path(scratch)/"output.bin"
        for flags in (0, 2, 6, 8, 14):
            for primary_scale, support_scale in ((1, 1), (.3, 1.7), (3, .3)):
                palettes = []
                for floating in (0, 1):
                    before.write_bytes(graph+authored+struct.pack("<Iff", flags|floating, primary_scale, support_scale))
                    subprocess.run([str(args.adapter_exe), "--floating-mesh-fixture", str(before), str(after)], check=True, capture_output=True)
                    raw = after.read_bytes()
                    assert len(raw) == count*52
                    palettes.append([struct.unpack_from("<13f", raw, i*52) for i in range(count)])
                normal, corrected = palettes
                assert all(normal[i] == corrected[i] for i in range(count) if i not in arms)
                old = list(corrected)
                for i in arms: old[i] = (*normal[i][:10], *CAMERA);old[i] = (.00001, *old[i][1:])
                old_max = fixed_max = hidden_max = 0
                for position, n0, n1, w0, w1 in vertices:
                    side = sides[n0]
                    assert side != "root" and (n1 < 0 or sides[n1] == side or w1 == 0)
                    wrist = corrected[graph_names.index(f"frame {side} wriste")][10:13]
                    def skin(palette):
                        p0 = point(palette[graph_names.index(names[n0])], inverse(rest[n0], position))
                        p1 = point(palette[graph_names.index(names[n1])], inverse(rest[n1], position)) if n1 >= 0 else (0, 0, 0)
                        return add(mul(p0, w0), mul(p1, w1))
                    a, b = skin(old), skin(corrected)
                    old_max = max(old_max, length(sub(a, wrist)))
                    fixed_max = max(fixed_max, length(sub(b, wrist)))
                    hidden = n0 in (1, 2, 3, 4) and (n1 < 0 or n1 in (1, 2, 3, 4) or w1 == 0)
                    if hidden: hidden_max = max(hidden_max, length(sub(b, wrist)))
                # Mesh-space geometry transformed by every influenced hand bone
                # remains near that wrist; pure arm vertices collapse to a point.
                assert hidden_max < .00003, hidden_max
                assert fixed_max < .18*max(primary_scale, support_scale), fixed_max
                assert old_max > fixed_max+.2, (old_max, fixed_max)
                cases.append(dict(flags=flags, primary_scale=primary_scale, support_scale=support_scale,
                                  old_wrist_radius=old_max, corrected_wrist_radius=fixed_max, collapsed_arm_radius=hidden_max))
    print(json.dumps(dict(status="PASS_OFFICIAL_MESH_FLOATING_HANDS", kit_sha256=KIT_SHA,
                         mesh_sha256=hashlib.sha256(mesh).hexdigest().upper(),
                         adapter_sha256=hashlib.sha256(args.adapter_exe.read_bytes()).hexdigest().upper(),
                         parts=4, vertices=len(vertices), forearm_wrist_blended_vertices=blend_count,
                         root_weighted_vertices=0, cases=cases,
                         limit="Official mesh topology, synthetic tracked poses and compiled production palette; no live game or headset."), indent=2))


if __name__ == "__main__": main()
