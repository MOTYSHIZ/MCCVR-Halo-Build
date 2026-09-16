"""Derive CE contact envelopes from every vertex of the official HCEEK FP models.

Only primary kit descriptors and extracted official tags are read. Generated
bounds are model-node-local, selected by the complete native graph name/parent
identity; they are not generic sizes for an arbitrary custom weapon.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import subprocess
import tempfile
import sys

from verify_ce_floating_hand_mesh import inspect_official_kit, inverse, point, direction, add, mul, cross, dot
from verify_ce_first_person_tags import inspect

ROOT = Path(__file__).resolve().parents[2]


def inspect_extra_descriptors():
    # These additional blocks skip authored permutations/markers on the way
    # to geometry. Their sizes/names also come from the official executable.
    sys.path.insert(0, str(ROOT/"out/pydeps"))
    import pefile
    image = pefile.PE(str(ROOT/"out/deps/re-tools/inputs/halo_tag_test.exe"), fast_load=True).get_memory_mapped_image()
    u32 = lambda va: struct.unpack_from("<I", image, va-0x400000)[0]
    for va, name, size in ((0xc49a20, "model_region_permutation_marker_block", 0x50),
                           (0xc49bf8, "gbxmodel_region_permutation_block", 0x58),
                           (0xc49ce0, "gbxmodel_region_block", 0x4c)):
        at = u32(va)-0x400000
        assert image[at:image.find(b"\0", at)].decode() == name and u32(va+20) == size


def graph_identity(nodes):
    value = 14695981039346656037
    def append(byte):
        nonlocal value
        value = ((value ^ byte) * 1099511628211) & ((1 << 64)-1)
    append(len(nodes))
    for node in nodes:
        # Bytes after the first NUL are not semantic and need not match retail.
        for byte in node[:32].split(b"\0", 1)[0]+b"\0": append(byte)
        for byte in node[36:38]: append(byte)
    return value or 1


def read_weapon(path, mesh_parts=None):
    data = path.read_bytes()
    u32 = lambda at: struct.unpack_from(">I", data, at)[0]
    assert data[36:40] == b"mod2" and u32(0xec) == 0
    local_nodes = bool(u32(0x40) & 2)
    count, regions, geometries = u32(0xf8), u32(0x104), u32(0x110)
    assert 0 < count <= 64 and regions == 1 and 0 < geometries <= 32
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
    permutations = u32(cursor+64)
    assert 0 < permutations <= 32
    cursor += 0x4c
    markers = [u32(cursor+i*0x58+76) for i in range(permutations)]
    cursor += permutations*0x58+sum(markers)*0x50
    geometry_headers = cursor
    cursor += geometries*0x30
    vertices, part_count = [], 0
    for geometry in range(geometries):
        parts = u32(geometry_headers+geometry*0x30+36)
        assert 0 < parts <= 64
        part_headers = cursor
        cursor += parts*0x84
        part_count += parts
        for part in range(parts):
            at = part_headers+part*0x84
            uncompressed, compressed, triangles = u32(at+32), u32(at+44), u32(at+56)
            assert uncompressed > 0 and compressed in (0, uncompressed) and uncompressed < 65536
            local_count = data[at+107]
            local = data[at+108:at+108+local_count] if local_nodes else range(count)
            assert not local_nodes or 0 < local_count <= 24
            assert all(i < count for i in local)
            first_vertex = len(vertices)
            for vertex in range(uncompressed):
                address = cursor+vertex*68
                position = struct.unpack_from(">3f", data, address)
                n0, n1 = struct.unpack_from(">hh", data, address+56)
                w0, w1 = struct.unpack_from(">ff", data, address+60)
                assert 0 <= n0 < len(local) and -1 <= n1 < len(local)
                assert 0 <= w0 <= 1 and 0 <= w1 <= 1 and abs(w0+w1-1) < .00001
                vertices.append((position, local[n0], local[n1] if n1 >= 0 else -1, w0, w1))
            if mesh_parts is not None:
                indices_at = cursor + uncompressed*68 + compressed*32
                indices = struct.unpack_from(f">{triangles*3}H", data, indices_at)
                assert all(i == 65535 or i < uncompressed for i in indices)
                mesh_parts.append(dict(vertices=vertices[first_vertex:], indices=indices))
            cursor += uncompressed*68+compressed*32+triangles*6
    assert cursor <= len(data)
    return data, names, parents, rest, vertices, part_count


def derive(path):
    record, fixture = inspect(path.with_suffix(".model_animations"))
    nodes = [fixture[i:i+64] for i in range(4, len(fixture), 64)]
    graph_names = [node[:32].split(b"\0", 1)[0].decode() for node in nodes]
    data, names, parents, rest, vertices, parts = read_weapon(path)
    assert len(set(names)) == len(names) and len(set(graph_names)) == len(graph_names)
    mapping = [graph_names.index(name) for name in names]
    for node, parent in enumerate(parents):
        if parent >= 0:
            assert struct.unpack_from("<h", nodes[mapping[node]], 36)[0] == mapping[parent], (path, names[node], "parent mapping")
    def weapon_node(index):
        for _ in nodes:
            if index == record["weapon_root"]: return True
            index = struct.unpack_from("<h", nodes[index], 36)[0]
            if index < 0: return False
        raise AssertionError("cyclic graph")
    bounds, weapon_vertices, excluded_vertices = {}, [], 0
    for vertex in vertices:
        position, n0, n1, w0, w1 = vertex
        influences = [(n, w) for n, w in ((n0, w0), (n1, w1)) if n >= 0 and w > 0]
        owns = [weapon_node(mapping[n]) for n, w in influences]
        assert all(owns) or not any(owns), (path, "weapon/hand blend")
        if not any(owns):
            excluded_vertices += 1
            continue
        weapon_vertices.append(vertex)
        for node, weight in influences:
            local = inverse(rest[node], position)
            entry = bounds.setdefault(mapping[node], [list(local), list(local)])
            for axis in range(3):
                entry[0][axis] = min(entry[0][axis], local[axis])
                entry[1][axis] = max(entry[1][axis], local[axis])
    assert bounds and weapon_vertices
    # A positive-weight skinned vertex is a convex combination of these node
    # boxes, so the combined current-pose box contains it for every animation.
    for position, n0, n1, w0, w1 in weapon_vertices:
        for node, weight in ((n0, w0), (n1, w1)):
            if node < 0 or weight == 0: continue
            local = inverse(rest[node], position)
            lo, hi = bounds[mapping[node]]
            assert all(lo[a] <= local[a] <= hi[a] for a in range(3))
    record.update(model_file=path.relative_to(ROOT).as_posix(),
                  model_sha256=hashlib.sha256(data).hexdigest().upper(),
                  graph_identity=f"{graph_identity(nodes):016X}", model_nodes=len(names),
                  parts=parts, vertices=len(vertices), weapon_vertices=len(weapon_vertices),
                  excluded_hand_vertices=excluded_vertices,
                  bounds=[dict(graph_node=node, name=graph_names[node], minimum=lo, maximum=hi)
                          for node, (lo, hi) in sorted(bounds.items())])
    return record


def header(records):
    def number(x):
        value = f"{x:.9g}"
        return value+("f" if "." in value or "e" in value else ".0f")
    output = ["// Generated by tools/re/verify_ce_weapon_mesh.py from official HCEEK tags.",
              "// Runtime graph identity and every positively weighted weapon node are required.",
              "#pragma once", "namespace halo_ce", "{"]
    for index, record in enumerate(records):
        output.append(f"inline constexpr WeaponNodeBounds kCeWeaponNodeBounds{index}[]{{")
        for bound in record["bounds"]:
            low = ",".join(map(number, bound["minimum"]))
            high = ",".join(map(number, bound["maximum"]))
            output.append(f"    {{{bound['graph_node']},{{{low}}},{{{high}}}}}, // {bound['name']}")
        output.append("};")
    output.append("inline constexpr WeaponMeshBounds kCeWeaponMeshBounds[]{")
    for index, record in enumerate(records):
        name = Path(record["model_file"]).parent.parent.name
        output.append(f'    {{0x{record["graph_identity"]}ull,{record["nodes"]},{record["weapon_root"]},'
                      f'kCeWeaponNodeBounds{index},"{name}"}},')
    output.extend(["};", "}", ""])
    return "\n".join(output)


def exercise_adapter(records, adapter):
    cases, vertices_checked = 0, 0
    with tempfile.TemporaryDirectory(prefix="ce-weapon-mesh-", dir=ROOT/"out") as scratch:
        before, after = Path(scratch)/"before.bin", Path(scratch)/"after.bin"
        for record in records:
            path = ROOT/record["model_file"]
            _, fixture = inspect(path.with_suffix(".model_animations"))
            nodes = [fixture[i:i+64] for i in range(4, len(fixture), 64)]
            graph_names = [node[:32].split(b"\0", 1)[0].decode() for node in nodes]
            _, names, _, rest, vertices, _ = read_weapon(path)
            mapping = [graph_names.index(name) for name in names]
            weapon_nodes = {bound["graph_node"] for bound in record["bounds"]}
            for pose in range(4):
                palette = []
                for index in range(len(nodes)):
                    angle = index*pose*.17
                    scale = .5*pose if pose else 1
                    palette.append((scale, math.cos(angle), math.sin(angle), 0,
                                    -math.sin(angle), math.cos(angle), 0, 0, 0, 1,
                                    index*.012, pose*.031, -index*.006))
                before.write_bytes(fixture+b"".join(struct.pack("<13f", *m) for m in palette))
                result = subprocess.run([str(adapter), "--weapon-mesh-fixture", str(before), str(after)],
                                        capture_output=True, text=True)
                assert result.returncode == 0, (path, pose, result.stdout, result.stderr)
                raw = after.read_bytes()
                assert len(raw) == 14*12
                samples = [struct.unpack_from("<3f", raw, i*12) for i in range(14)]
                carrier = palette[record["weapon_root"]]
                low, high = inverse(carrier, samples[0]), inverse(carrier, samples[7])
                for position, n0, n1, w0, w1 in vertices:
                    influences = [(n, w) for n, w in ((n0, w0), (n1, w1)) if n >= 0 and w > 0]
                    if not all(mapping[n] in weapon_nodes for n, _ in influences): continue
                    skinned = (0, 0, 0)
                    for node, weight in influences:
                        skinned = add(skinned, mul(point(palette[mapping[node]], inverse(rest[node], position)), weight))
                    p = inverse(carrier, skinned)
                    assert all(low[axis]-.00001 <= p[axis] <= high[axis]+.00001 for axis in range(3)), (path, pose, p, low, high)
                    vertices_checked += 1
                cases += 1
    return dict(compiled_adapter_cases=cases, vertex_pose_checks=vertices_checked,
                compiled_checks="all stock graph fingerprints, both handedness modes, append routing, unknown fallback, animated-only no-melee, physical motion and shape reseeding")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--write", action="store_true")
    parser.add_argument("--record", type=Path)
    parser.add_argument("--adapter-exe", type=Path)
    args = parser.parse_args()
    inspect_official_kit(ROOT/"out/deps/re-tools/inputs/halo_tag_test.exe")
    inspect_extra_descriptors()
    records = [derive(path) for path in sorted((ROOT/"out/ce-hands-hceek-tags/tags/weapons").glob("*/fp/*.gbxmodel"))]
    assert len(records) == 12 and len({r["graph_identity"] for r in records}) == 12
    generated = header(records)
    target = ROOT/"src/common/haloce_weapon_bounds.generated.h"
    if args.write: target.write_text(generated)
    else: assert target.read_text() == generated, "Generated CE weapon geometry is stale"
    report = dict(source="Official HCEEK 2023.07.17.176677.1-QFE1", weapons=records,
                  coverage="All positive weapon vertex influences; per-node local envelopes, not triangle-exact collision",
                  limit="Stock CE model geometry; Anniversary replacement mesh shape and custom model geometry are not established by these tags")
    if args.adapter_exe: report.update(exercise_adapter(records, args.adapter_exe.resolve()))
    if args.record: args.record.write_text(json.dumps(report, indent=2)+"\n")
    print(json.dumps(dict(weapons=len(records), nodes=sum(len(r["bounds"]) for r in records),
                         weapon_vertices=sum(r["weapon_vertices"] for r in records), generated=str(target))))


if __name__ == "__main__": main()
