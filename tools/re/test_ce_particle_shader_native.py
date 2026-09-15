"""Execute hash-pinned, locally extracted CE particle shaders through D3D11 WARP.

The actual production lens-selector helper feeds both native shader consumers.
No game process or files are touched; bytecode remains an external fixture.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--shader-exe", type=Path,
                    default=ROOT / "out/build/release/Release/halomccvr_ce_particle_shader_tests.exe")
args = parser.parse_args()
records = []
for name, mode, sha in [
    ("0000-000.dxbc", "mesh", "EAEAA5F4C5A00172CBA02951B3AC0A28DE5F12ED14A703C66E4EA590D57E68FC"),
    ("0000-001.dxbc", "sprite", "F6F30FAC1CEE4A32BDD7887E63771C3A465CC3728D610EB24C2FB4D843F73D36"),
]:
    path = ROOT / "out/ce-anniversary-shaders/particles" / name
    assert hashlib.sha256(path.read_bytes()).hexdigest().upper() == sha, "Native CE particle shader hash mismatch"
    result = subprocess.run([str(args.shader_exe), str(path), mode], check=True,
                            capture_output=True, text=True)
    records.append({"file": path.relative_to(ROOT).as_posix(), "sha256": sha,
                    "mode": mode, "result": result.stdout.strip()})
print(json.dumps({"shaders": records, "scope": "Native shader clip-position/lens math and production selector helper; no live firing or headset acceptance"}, indent=2))
