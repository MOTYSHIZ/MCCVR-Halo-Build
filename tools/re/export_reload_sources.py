"""Export read-only official weapon evidence into ignored out/reload-kits.

Uses each title's own tool.exe. Original kit/game files never enter source or
distribution archives. Run after selective extraction of weapon tags to out.
"""
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
KITS = Path("D:/SteamLibrary/steamapps/common")
OUT = ROOT / "out/reload-kits"


def export_title(kit):
    working = KITS / kit if kit == "H4EK" else OUT / kit
    (working / "data").mkdir(exist_ok=True)
    destination = OUT / "xml" / kit
    destination.mkdir(parents=True, exist_ok=True)
    records = []
    paths = sorted((working / "tags/objects/weapons").rglob("*.weapon"))
    print(kit, "weapon definitions", len(paths), flush=True)
    for number, path in enumerate(paths):
        relative = path.relative_to(working / "tags").as_posix()
        key = hashlib.sha256(relative.encode()).hexdigest()[:16]
        target = destination / (key + ".weapon.xml")
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        stamp = target.with_suffix(".sha256")
        if not target.exists() or not stamp.exists() or stamp.read_text() != digest:
            completed = subprocess.run([str(KITS / kit / "tool.exe"), "export-tag-to-xml",
                                        str(path.resolve()), str(target.resolve())],
                                       cwd=working, capture_output=True, creationflags=subprocess.CREATE_NO_WINDOW)
            if completed.returncode or not target.exists():
                raise RuntimeError((kit, relative, completed.returncode, completed.stdout, completed.stderr))
            stamp.write_text(digest)
        records.append(dict(tag=relative, sha256=digest, xml=target.relative_to(ROOT).as_posix()))
        if number % 20 == 0:
            print(kit, number + 1, "/", len(paths), flush=True)
    (OUT / (kit + "-weapons.json")).write_text(json.dumps(records, indent=2) + "\n")
    return kit, len(records)


if __name__ == "__main__":
    selected = sys.argv[1:] or ["H2EK", "H3EK", "H3ODSTEK", "HREK", "H4EK"]
    with ThreadPoolExecutor(max_workers=5) as pool:
        for result in pool.map(export_title, selected):
            print("COMPLETE", result, flush=True)
