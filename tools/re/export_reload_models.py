"""Resolve first-person weapon model references from each official weapon tag."""
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import subprocess
import sys

from export_reload_sources import ROOT, KITS, OUT
from reload_tag_xml import read


def export_title(kit):
    working = KITS / kit if kit == "H4EK" else OUT / kit
    tagroot = working / "tags"
    records = json.loads((OUT / (kit + "-weapons.json")).read_text())
    models = {}
    available = list((tagroot / "objects/weapons").rglob("*.render_model"))
    missing = []
    for record in records:
        tree = read(ROOT / record["xml"])
        for field in tree.iter():
            if field.get("name") != "first person model":
                continue
            ref = (field.get("value") or field.text or "").strip()
            if kit == "HREK":
                matches = [p for p in available if p.stem == ref]
                if not ref or ref == "NONE":
                    continue
                if not matches:
                    missing.append(dict(weapon=record["tag"], model=ref))
                    continue
                if len(matches) != 1:
                    raise ValueError((kit, record["tag"], ref, matches))
                path = matches[0]
            else:
                ref = ref.split(",")[0]
                if not ref:
                    continue
                path = tagroot / (ref + ".render_model")
                if not path.is_file():
                    missing.append(dict(weapon=record["tag"], model=ref))
                    continue
            models.setdefault(path, []).append(record["tag"])
    result = []
    for number, (path, weapons) in enumerate(sorted(models.items())):
        relative = path.relative_to(tagroot).as_posix()
        key = hashlib.sha256(relative.encode()).hexdigest()[:16]
        target = OUT / "xml" / kit / (key + ".model.xml")
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        stamp = target.with_suffix(".sha256")
        if not target.exists() or not stamp.exists() or stamp.read_text() != digest:
            completed = subprocess.run([str(KITS / kit / "tool.exe"), "export-tag-to-xml",
                                        str(path.resolve()), str(target.resolve())],
                                       cwd=working, capture_output=True, creationflags=subprocess.CREATE_NO_WINDOW)
            if completed.returncode or not target.exists():
                raise RuntimeError((kit, relative, completed.returncode, completed.stdout, completed.stderr))
            stamp.write_text(digest)
        result.append(dict(tag=relative, sha256=digest, weapons=sorted(set(weapons)),
                           xml=target.relative_to(ROOT).as_posix()))
        if number % 5 == 0:
            print(kit, number + 1, "/", len(models), flush=True)
    (OUT / (kit + "-models.json")).write_text(json.dumps(dict(models=result, missing=missing), indent=2) + "\n")
    return kit, len(result), len(missing)


if __name__ == "__main__":
    with ThreadPoolExecutor(max_workers=5) as pool:
        for result in pool.map(export_title, sys.argv[1:] or ["H2EK", "H3EK", "H3ODSTEK", "HREK", "H4EK"]):
            print("COMPLETE", result, flush=True)
