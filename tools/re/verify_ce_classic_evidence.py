"""Verify CE Classic output-routing evidence against the pinned offline files.

This grants no runtime-hook approval. Camera math and unique byte signatures
alone cannot prove a complete eye source or safe scene repetition.
"""
import argparse
import json
from pathlib import Path

from verify_ce_render_evidence import verify


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path,
                        default=Path("docs/HALOCE-CLASSIC-CONTRACTS.json"))
    parser.add_argument("--retail", type=Path)
    parser.add_argument("--kit", type=Path)
    args = parser.parse_args()
    manifest = json.loads(args.manifest.read_text(encoding="utf-8-sig"))
    core = json.loads(Path("docs/HALOCE-EVIDENCE-MANIFEST.json").read_text(encoding="utf-8-sig"))
    result = [verify(args.kit or Path(core["official_kit"]["pinned_path"]),
                     core["official_kit"], []),
              verify(args.retail or Path(core["retail"]["pinned_path"]),
                     core["retail"], manifest["offline_render_contracts"])]
    print(json.dumps({"result": "PASS_OFFLINE_ONLY", "inputs": result}, indent=2))


if __name__ == "__main__":
    main()
