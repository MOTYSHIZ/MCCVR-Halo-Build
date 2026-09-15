"""Count saved D3D11 Map/Unmap types in a RenderDoc structured XML export.

Saved retry frames cannot identify the uncaptured Map that failed the prior
frame. This census reports observed types, contexts, and resources only.
"""
import argparse
from collections import Counter
import json
from pathlib import Path
import xml.etree.ElementTree as ET


def census(path):
    counts = Counter()
    resources = Counter()
    phase = "initialisation"
    open_maps = {}
    unpaired_unmaps = []
    for _, chunk in ET.iterparse(path, events=("end",)):
        if chunk.tag != "chunk":
            continue
        kind = chunk.get("name", "").removeprefix("ID3D11DeviceContext::")
        if kind == "Internal::Beginning of Capture":
            phase = "frame"
        if kind in ("Map", "Unmap"):
            fields = {e.get("name"): e for e in chunk}
            map_type = fields["MapType"].get("string")
            context = fields["Context"].text
            resource = fields["pResource"].text
            counts[(phase, kind, map_type)] += 1
            resources[(phase, kind, map_type, context, resource)] += 1
            if phase == "frame":
                key = (context, resource, fields["Subresource"].text)
                row = {"chunk": chunk.get("chunkIndex"), "context": context,
                       "resource": resource, "type": map_type}
                if kind == "Map":
                    open_maps[key] = row
                elif key in open_maps:
                    del open_maps[key]
                else:
                    unpaired_unmaps.append(row)
        chunk.clear()
    return {
        "file": str(path),
        "counts": [{"phase": p, "kind": k, "type": t, "count": n}
                   for (p, k, t), n in sorted(counts.items())],
        "resources": [{"phase": p, "kind": k, "type": t, "context": c, "resource": r, "count": n}
                      for (p, k, t, c, r), n in sorted(resources.items())],
        "unmaps_without_map_in_saved_frame": unpaired_unmaps,
        "maps_left_open_at_frame_end": list(open_maps.values()),
        "limit": "Counts describe saved chunks only, not the preceding failed capture's missing Map.",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("xml", type=Path, nargs="+")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    results = [census(p) for p in args.xml]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(results, indent=2) + "\n")
    for result in results:
        print(result["file"], json.dumps(result["counts"]))


if __name__ == "__main__":
    main()
