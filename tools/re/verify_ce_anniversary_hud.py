"""Verify the independently pinned Anniversary HUD replay bindings, offline."""
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "out/pydeps"))
from verify_ce_render_evidence import verify

if __name__ == "__main__":
    manifest = json.loads((ROOT / "docs/HALOCE-EVIDENCE-MANIFEST.json").read_text(encoding="utf-8-sig"))
    fragment = json.loads((ROOT / "docs/HALOCE-ANNIVERSARY-HUD-CONTRACTS.json").read_text())
    report = verify(ROOT / manifest["retail"]["pinned_path"], manifest["retail"], fragment["entries"])
    print(json.dumps({"status": "PASS_OFFLINE_BINDINGS_ONLY", "contracts": report}, indent=2))
