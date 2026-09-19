"""Isolate official reload-part triangles into ignored authoring output.

No game process or game installation is touched. Output is an intermediate
authoring asset, not authorization to distribute original kit/game files.
"""
import hashlib
import json
import math
from pathlib import Path
import sys
import subprocess

from export_reload_sources import ROOT, OUT, KITS
from reload_tag_xml import read, block, value, values, descendant_blocks
from verify_ce_weapon_mesh import read_weapon, derive


def floats(text):
    result = [float(x) for x in text.split(",")]
    assert all(math.isfinite(x) for x in result)
    return result


def node_index(element, name, names):
    old = next((x for x in element if x.tag == "block_index" and x.get("type") == name), None)
    if old is not None:
        return int(old.get("index"))
    text = value(element, name)
    if text == "NONE":
        return -1
    if "," in text:
        return int(text.rsplit(",", 1)[1])
    return names.index(text)


def triangles(indices, strip=True):
    if not strip:
        assert len(indices) % 3 == 0
        for at in range(0, len(indices), 3):
            yield indices[at:at+3]
        return
    pending = []
    for index in indices:
        if index in (-1, 65535, 0xFFFFFFFF):
            pending = []
            continue
        pending.append(index)
        if len(pending) >= 3:
            at = len(pending)-3
            tri = pending[-3:]
            if at % 2:
                tri = [tri[1], tri[0], tri[2]]
            if len(set(tri)) == 3:
                yield tri


def xml_model(path, kit):
    tree = read(path)
    nodes = block(tree, "nodes")
    names = [value(n, "name") for n in nodes]
    parents = [node_index(n, "parent node", names) for n in nodes]
    checksum = int(value(tree, "runtime import info checksum", "0")) & 0xFFFFFFFF
    compression_blocks = descendant_blocks(tree, "compression info")
    compression = list(compression_blocks[0])[0] if compression_blocks and len(compression_blocks[0]) else None
    bounds = None
    if compression is not None:
        if kit == "H2EK":
            bounds = [floats(value(compression, "position bounds " + a)) for a in "xyz"]
        else:
            packed = floats(value(compression, "position bounds 0")) + floats(value(compression, "position bounds 1"))
            bounds = [packed[i:i+2] for i in (0, 2, 4)]
    mesh_data = []
    # Use one authored permutation per region. Combining alternative meshes
    # makes interpenetrating magazine copies, even after exact face deduping.
    selected_meshes = set()
    for region in block(tree, "regions"):
        permutations = block(region, "permutations")
        if len(permutations) == 0:
            continue
        permutation = permutations[0]
        if kit == "H2EK":
            selected_meshes.add(int(value(permutation, "L6 section index")))
        else:
            start = int(value(permutation, "mesh index"))
            count = int(value(permutation, "mesh count"))
            selected_meshes.update(range(start, start + count))
    if kit == "H2EK":
        for index, section in enumerate(block(tree, "sections")):
            if index not in selected_meshes:
                continue
            for data in block(section, "section data"):
                mesh_data.append((data, section, True))
    else:
        mesh_blocks = descendant_blocks(tree, "meshes")
        raw_blocks = descendant_blocks(tree, "per mesh temporary")
        map_blocks = descendant_blocks(tree, "per mesh node map")
        meshes = list(mesh_blocks[0]) if mesh_blocks else []
        maps = list(map_blocks[0]) if map_blocks else []
        for i, raw in enumerate(raw_blocks[0] if raw_blocks else []):
            if i not in selected_meshes:
                continue
            assert i < len(meshes)
            mapping = block(maps[i], "node map") if i < len(maps) else []
            mesh_data.append((raw, meshes[i], mapping))
    parts = []
    material_records=list(block(tree,"materials"))
    material_names=[m.get("name",str(i)) for i,m in enumerate(material_records)]
    shaders=[value(m,"shader" if kit=="H2EK" else "render method","") for m in material_records]
    for data, header, mapping in mesh_data:
        h2 = kit == "H2EK"
        vertices = []
        uvs=[]
        node_map = [int(value(n, "node index")) & 0xFF for n in (block(data, "node map") if h2 else mapping)]
        for vertex in block(data, "raw vertices"):
            position = floats(value(vertex, "position"))
            if not h2:
                assert bounds is not None
                flag_text = value(compression, "compression flags", "0")
                compressed = "compressed position" in flag_text or (flag_text.strip().isdigit() and int(flag_text) & 1)
                if compressed:
                    position = [lo + p*(hi-lo) for p, (lo, hi) in zip(position, bounds)]
            indices = [int(x) for x in values(vertex, "node index (NEW)" if h2 else "node index")]
            weights = [float(x) for x in values(vertex, "node_weight" if h2 else "node weight")]
            assert len(indices) == len(weights) == 4
            if h2:
                assert value(vertex, "use new node indices") == "1"
            influences = []
            for index, weight in zip(indices, weights):
                if weight <= 0:
                    continue
                assert index >= 0 and (not node_map or index < len(node_map))
                node = node_map[index] if node_map else index
                assert node < len(names)
                influences.append(node)
            vertices.append((position, influences))
            uv=floats(value(vertex,"texcoord"))
            if not h2 and ("compressed texture" in value(compression,"compression flags","") or (value(compression,"compression flags","0").isdigit() and int(value(compression,"compression flags","0"))&2)):
                uv=[lo+t*(hi-lo) for t,(lo,hi) in zip(uv,[floats(value(compression,"texcoord bounds "+a)) for a in "01"])]
            uvs.append(uv)
        raw = block(data, "strip indices" if h2 else "raw indices")
        indices = [int(value(index, "index" if h2 else "word")) & 0xFFFF for index in raw]
        if h2 and len(raw) and not values(raw[0], "index"):
            raise ValueError("unrecognized H2 strip field")
        strip = h2 or "strip" in value(header, "index buffer type", "")
        materials=[]
        for part in block(data if h2 else header,"parts"):
            materials.append(dict(start=int(value(part,"strip start index" if h2 else "index start","0")),
                count=int(value(part,"strip length" if h2 else "index count","0")),
                material=node_index(part,"material" if h2 else "render method index",material_names)))
        parts.append(dict(vertices=vertices, indices=indices, strip=strip,uvs=uvs,materials=materials))
    return dict(names=names, parents=parents, checksum=checksum, bounds=bounds, parts=parts, shaders=shaders)


def isolate(model, wanted):
    names, parents = model["names"], model["parents"]
    owned = {i for i, name in enumerate(names) if name in wanted}
    for _ in names:
        owned.update(i for i, parent in enumerate(parents) if parent in owned)
    points, faces, seen = [], [], {}
    face_uvs,face_materials=[],[]
    for part in model["parts"]:
        vertices = part["vertices"]
        selected = [bool(influences) and all(n in owned for n in influences) for _, influences in vertices]
        ranges=part.get("materials",[dict(start=0,count=len(part["indices"]),material=-1)])
        for surface in ranges:
          for tri in triangles(part["indices"][surface["start"]:surface["start"]+surface["count"]], part["strip"]):
            assert min(tri) >= 0 and max(tri) < len(vertices)
            if not all(selected[index] for index in tri):
                continue
            face = []
            for index in tri:
                p = tuple(vertices[index][0])
                if p not in seen:
                    seen[p] = len(points)
                    points.append(p)
                face.append(seen[p])
            if len(set(face)) == 3:
                faces.append(face)
                face_uvs.append([part["uvs"][i] for i in tri] if "uvs" in part else [])
                face_materials.append(surface["material"])
    # Different LOD/permutation strips may repeat triangles; select unique
    # geometry, retaining winding from the first occurrence.
    unique = {}
    for face,uv,material in zip(faces,face_uvs,face_materials):
        unique.setdefault(tuple(sorted(face)), (face,uv,material))
    faces = [v[0] for v in unique.values()]
    face_uvs = [v[1] for v in unique.values()]
    face_materials = [v[2] for v in unique.values()]
    if points:
        minimum = [min(p[a] for p in points) for a in range(3)]
        maximum = [max(p[a] for p in points) for a in range(3)]
        assert all(math.isfinite(x) for p in points for x in p)
    else:
        minimum = maximum = [0, 0, 0]
    return dict(vertices=points, triangles=faces, triangle_uvs=face_uvs,
                triangle_materials=face_materials, shaders=model.get("shaders",[]),minimum=minimum, maximum=maximum)


def main():
    kit = sys.argv[1]
    target = OUT / "geometry" / kit
    target.mkdir(parents=True, exist_ok=True)
    if kit == "HCEEK":
        records = []
        for path in sorted((ROOT / "out/ce-hands-hceek-tags/tags/weapons").glob("*/fp/*.gbxmodel")):
            records.append(dict(tag=path.relative_to(ROOT).as_posix(), path=str(path)))
    else:
        records = json.loads((OUT / (kit + "-models.json")).read_text())["models"]
    report = []
    for record in records:
        if kit == "HCEEK":
            raw_parts = []
            path = Path(record.pop("path"))
            data, names, parents, rest, vertices, count = read_weapon(path, raw_parts)
            working=ROOT/"out/ce-hands-hceek-tags"
            xml=OUT/"xml/HCEEK"/(hashlib.sha256(data).hexdigest()+".xml")
            xml.parent.mkdir(parents=True,exist_ok=True)
            if not xml.exists() or not xml.stat().st_size:
                subprocess.run([str(KITS/"HCEEK/tool.exe"),"export-tag-to-xml",
                    str(path.relative_to(working/"tags")),str(xml)],cwd=working,check=True,
                    capture_output=True,creationflags=subprocess.CREATE_NO_WINDOW)
            tree=read(xml)
            authored_parts=[p for b in descendant_blocks(tree,"parts") for p in b]
            assert len(authored_parts)==len(raw_parts)
            for raw,authored in zip(raw_parts,authored_parts):
                authored_vertices=block(authored,"uncompressed vertices")
                assert len(authored_vertices)==len(raw["vertices"])
                # Verify order against the existing exact binary geometry.
                for binary,v in zip(raw["vertices"],authored_vertices):
                    assert all(abs(a-b)<=.000001 for a,b in zip(binary[0],floats(value(v,"position"))))
                raw["uvs"]=[floats(value(v,"texture coords")) for v in authored_vertices]
                raw["materials"]=[dict(start=0,count=len(raw["indices"]),
                    material=node_index(authored,"shader index",[]))]
            proof = derive(path)
            model = dict(names=names, parents=parents, checksum=int(proof["graph_identity"],16), bounds=None,
                         parts=[dict(vertices=[(p,[n for n,w in ((n0,w0),(n1,w1)) if n>=0 and w>0])
                                               for p,n0,n1,w0,w1 in part["vertices"]],
                                     indices=part["indices"],strip=True,uvs=part["uvs"],
                                     materials=part["materials"]) for part in raw_parts],
                         shaders=[value(s,"shader") for s in block(tree,"shaders")])
            record.update(sha256=hashlib.sha256(data).hexdigest(), weapons=[path.parent.parent.name])
        else:
            model = xml_model(ROOT / record["xml"], kit)
        print(kit, Path(record["tag"]).stem, model["names"], flush=True)
        wanted = [n for n in model["names"] if any(word in n.lower() for word in ("magazine", "clip"))]
        # Shotgun 'magazine' is the thin receiver loading gate, not a removable
        # clip. Keep its established gesture without presenting that gate as ammo.
        if "shotgun" in record["tag"]:
            wanted = []
        # Explicit per-title authored reload assemblies, not names inferred
        # from another engine's node indices or structure layout.
        aliases = {
            "H2EK": {"fp_covenant_carbine": "battery"},
            "H3EK": {"fp_covenant_carbine": "battery", "fp_spike_rifle": "ammo_drum", "fp_excavator": "drum"},
            "H3ODSTEK": {"fp_covenant_carbine": "battery", "fp_spike_rifle": "ammo_drum", "fp_excavator": "drum"},
            "HREK": {"spike_rifle": "b_ammo_drum"},
            "H4EK": {"storm_covenant_carbine": "b_ammo"},
        }
        alias = aliases.get(kit, {}).get(Path(record["tag"]).stem)
        if alias:
            assert alias in model["names"]
            wanted = [alias]
        geometry = isolate(model, wanted) if wanted else dict(vertices=[], triangles=[], minimum=[0]*3, maximum=[0]*3)
        name = hashlib.sha256(record["tag"].encode()).hexdigest()[:16]
        data = dict(**record, checksum=model["checksum"], nodes=model["names"], bounds=model["bounds"],
                    selected_nodes=wanted, **geometry)
        (target / (name + ".json")).write_text(json.dumps(data, separators=(",", ":")))
        report.append({k:v for k,v in data.items() if k not in ("vertices", "triangles", "triangle_uvs", "triangle_materials") } |
                      dict(vertices=len(geometry["vertices"]), triangles=len(geometry["triangles"])))
    (OUT / (kit + "-reload-geometry.json")).write_text(json.dumps(report, indent=2)+"\n")


if __name__ == "__main__":
    main()
