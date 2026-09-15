"""Check the official HCEEK FP graph evidence and make native-node test fixtures.

Only reads the already-extracted official kit tags; never opens a process or
modifies a game/kit file. Fixture output must be a directory under repo out/.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[2]


def inspect(path):
    data = path.read_bytes()
    count = struct.unpack_from(">I", data, 0xa8)[0]
    assert 1 <= count <= 64, (path, count)
    start = data.index(b"frame bone24\0")
    assert data.find(b"frame bone24\0", start+1) == -1
    names, nodes = [], []
    for index in range(count):
        at = start+index*64
        name = data[at:at+32].split(b"\0", 1)[0].decode("ascii")
        sibling, child, parent = struct.unpack_from(">hhh", data, at+32)
        assert name and (index == 0 or 0 <= parent < count)
        names.append(name)
        nodes.append(data[at:at+32]+struct.pack("<hhh", sibling, child, parent)+bytes(26))
    wrist = [names.index("frame l wriste"), names.index("frame r wriste")]
    root = [i for i,n in enumerate(names) if n in ("frame gun", "frame body", "frame pole", "frame skull")
            and struct.unpack_from("<h", nodes[i], 36)[0] == wrist[1]]
    assert len(root) == 1
    for side,prefix in enumerate(("l", "r")):
        elbow, shoulder = names.index(f"frame {prefix} forearm"), names.index(f"frame {prefix} upperarm")
        assert struct.unpack_from("<h", nodes[wrist[side]], 36)[0] == elbow
        assert struct.unpack_from("<h", nodes[elbow], 36)[0] == shoulder
    assert struct.unpack_from("<h", nodes[root[0]], 36)[0] == wrist[1]
    return {"file": path.relative_to(ROOT).as_posix(), "sha256": hashlib.sha256(data).hexdigest().upper(),
            "nodes": count, "left_wrist": wrist[0], "right_wrist": wrist[1],
            "weapon_root": root[0], "weapon_root_name": names[root[0]]}, struct.pack("<I",count)+b"".join(nodes)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tags", type=Path, default=ROOT/"out/ce-hands-hceek-tags/tags/weapons")
    parser.add_argument("--fixtures", type=Path)
    parser.add_argument("--record", type=Path)
    args = parser.parse_args()
    fixtures = args.fixtures.resolve() if args.fixtures else None
    if fixtures:
        assert fixtures.is_relative_to((ROOT/"out").resolve()), "Fixture directory must be within repo out/"
        fixtures.mkdir(parents=True, exist_ok=True)
    records=[]
    for path in sorted(args.tags.glob("*/fp/*.model_animations")):
        record, fixture=inspect(path)
        records.append(record)
        if fixtures:
            (fixtures/(path.parent.parent.name.replace(" ","_")+".bin")).write_bytes(fixture)
    assert len(records) == 12, f"Expected all 12 official extracted FP graphs, got {len(records)}"
    if args.record:
        args.record.write_text(json.dumps({"source":"Official HCEEK 2023.07.17.176677.1-QFE1 extracted tags", "graphs":records},indent=2)+"\n")
    print(json.dumps({"verified_graphs":len(records),"graphs":records},indent=2))


if __name__ == "__main__": main()
