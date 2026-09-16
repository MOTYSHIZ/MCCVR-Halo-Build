"""Read-only CE magazine/Needler evidence from official editing-kit models.

This audits weighted vertices, not a runtime detached-mesh renderer. It does
not extract or distribute game artwork and never opens a game process.
"""
import hashlib
import json
from pathlib import Path
import sys

from verify_ce_weapon_mesh import (ROOT, derive, read_weapon,
                                  inspect_extra_descriptors, inspect_official_kit)


def main():
    inspect_official_kit(ROOT / "out/deps/re-tools/inputs/halo_tag_test.exe")
    inspect_extra_descriptors()
    records = []
    for path in sorted((ROOT / "out/ce-hands-hceek-tags/tags/weapons").glob("*/fp/*.gbxmodel")):
        data, names, parents, rest, vertices, parts = read_weapon(path)
        evidence = derive(path)
        magazines = []
        for node, name in enumerate(names):
            if "magazine" not in name:
                continue
            owned = {node}
            for child, parent in enumerate(parents):
                if parent in owned:
                    owned.add(child)
            isolated = mixed = 0
            for _, n0, n1, w0, w1 in vertices:
                membership = [n in owned for n, w in ((n0, w0), (n1, w1)) if n >= 0 and w > 0]
                isolated += bool(membership) and all(membership)
                mixed += any(membership) and not all(membership)
            magazines.append(dict(node=name, exclusively_weighted_vertices=isolated,
                                  vertices_blended_with_gun=mixed))
        records.append(dict(weapon=path.parent.parent.name,
                            model_sha256=hashlib.sha256(data).hexdigest().upper(),
                            graph_identity=evidence["graph_identity"],
                            magazines=magazines,
                            needle_nodes=sum("needle" in n for n in names)))
    assert len(records) == 12
    needle = next(r for r in records if r["weapon"] == "needler")
    assert needle["graph_identity"] == "55EA2D6F6C10C375" and needle["needle_nodes"] == 16
    assert not needle["magazines"]
    output = dict(source="Official HCEEK tags; descriptor and model readers reverified",
                  records=records,
                  finding="Four CE models have magazine-named nodes with separately weighted vertices; Needler has 16 needle nodes.",
                  limits=["A node name does not establish a detachable magazine; shotgun needs reload/visual interpretation.",
                          "Triangle isolation, materials, UVs, native resource lifetime and an independent stereo draw are not established by this audit.",
                          "CE Original artwork does not establish Anniversary replacement artwork or another title's models.",
                          "No detached magazine meshes are shipped in this candidate."])
    target = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "out/reload-model-audit.json"
    target.write_text(json.dumps(output, indent=2) + "\n")
    print(json.dumps(dict(models=len(records), magazine_nodes=sum(len(r["magazines"]) for r in records),
                          needle_nodes=needle["needle_nodes"], report=str(target))))


if __name__ == "__main__":
    main()
