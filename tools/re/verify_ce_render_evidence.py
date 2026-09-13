"""Verify CE render evidence against file-backed PE code; never opens a process.

Requires the existing pefile dependency (out/pydeps). This is an offline binding
check, not permission to install hooks or a claim that Anniversary stereo works.
"""
import argparse
import hashlib
import json
import re
import struct
from pathlib import Path

import pefile


def matches(data, pattern):
    tokens = pattern.split()
    if not tokens or all(token == "??" for token in tokens):
        raise ValueError("empty or unconstrained signature")
    expression = b"".join(b"." if token == "??" else
                          re.escape(bytes([int(token, 16)])) for token in tokens)
    return [m.start() for m in re.finditer(b"(?=" + expression + b")", data, re.DOTALL)]


def verify(path, identity, contracts):
    raw = path.read_bytes()
    digest = hashlib.sha256(raw).hexdigest().upper()
    if digest != identity["sha256"] or len(raw) != identity["bytes"]:
        raise ValueError(f"{path.name}: pinned SHA-256/size mismatch")
    pe = pefile.PE(data=raw)
    if (pe.FILE_HEADER.Machine != int(identity["machine"], 16) or
            pe.OPTIONAL_HEADER.SizeOfImage != int(identity["image_size"], 16) or
            pe.FILE_HEADER.TimeDateStamp != int(identity["timestamp"], 16)):
        raise ValueError(f"{path.name}: pinned PE identity mismatch")
    sections = [s for s in pe.sections if s.Characteristics & 0x20000000]
    entries = {e.struct.BeginAddress for e in getattr(pe, "DIRECTORY_ENTRY_EXCEPTION", [])}
    reports = []
    for contract in contracts:
        rva = int(contract["rva"], 16)
        hits = [s.VirtualAddress + offset for s in sections
                for offset in matches(s.get_data(), contract["signature"])]
        if hits != [rva]:
            raise ValueError(f'{contract["name"]}: expected unique {rva:#x}, got {hits}')
        if contract.get("function_entry") and rva not in entries:
            raise ValueError(f'{contract["name"]}: no x64 unwind entry at {rva:#x}')
        for witness in contract.get("instruction_witnesses", []):
            instruction = int(witness["rva"], 16)
            length = len(witness["signature"].split())
            section = pe.get_section_by_rva(instruction)
            if (section not in sections or instruction + length >
                    section.VirtualAddress + section.SizeOfRawData):
                raise ValueError("witness is outside file-backed executable code")
            if matches(pe.get_data(instruction, length), witness["signature"]) != [0]:
                raise ValueError(f'{contract["name"]}: body witness changed at {instruction:#x}')
        for pointer in contract.get("image_pointers", []):
            location = int(pointer["rva"], 16)
            section = pe.get_section_by_rva(location)
            if (section is None or location + 8 >
                    section.VirtualAddress + section.SizeOfRawData):
                raise ValueError("pointer is outside file-backed image data")
            target = struct.unpack("<Q", pe.get_data(location, 8))[0]
            expected = pe.OPTIONAL_HEADER.ImageBase + int(pointer["target_rva"], 16)
            if target != expected:
                raise ValueError(f'{contract["name"]}: image pointer changed at {location:#x}')
        for edge in contract.get("relative_operands", []):
            instruction = int(edge["instruction_rva"], 16)
            expected = int(edge["target_rva"], 16)
            length = edge["instruction_bytes"]
            offset = edge["displacement_offset"]
            section = pe.get_section_by_rva(instruction)
            if (section not in sections or instruction + length >
                    section.VirtualAddress + section.SizeOfRawData):
                raise ValueError("operand is outside file-backed executable code")
            code = pe.get_data(instruction, length)
            if matches(code, edge["instruction_signature"]) != [0]:
                raise ValueError(f'{contract["name"]}: instruction changed at {instruction:#x}')
            target = instruction + length + struct.unpack_from("<i", code, offset)[0]
            if target != expected:
                raise ValueError(f'{contract["name"]}: {instruction:#x} resolves {target:#x}, expected {expected:#x}')
        reports.append({"name": contract["name"], "rva": hex(rva),
                        "unique": True, "relative_operands": len(contract.get("relative_operands", [])),
                        "body_witnesses": len(contract.get("instruction_witnesses", [])),
                        "image_pointers": len(contract.get("image_pointers", []))})
    return {"file": str(path), "sha256": digest, "checks": reports}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=Path("docs/HALOCE-EVIDENCE-MANIFEST.json"))
    parser.add_argument("--retail", type=Path)
    parser.add_argument("--kit", type=Path)
    args = parser.parse_args()
    manifest = json.loads(args.manifest.read_text(encoding="utf-8-sig"))
    # Hash the official semantic source as well as retail: this must not turn
    # into a retail-only discovery tool when evidence inputs move.
    kit = args.kit or Path(manifest["official_kit"]["pinned_path"])
    retail = args.retail or Path(manifest["retail"]["pinned_path"])
    contracts = manifest["offline_render_contracts"]
    if not contracts:
        raise ValueError("no render contracts recorded")
    result = [verify(kit, manifest["official_kit"], []),
              verify(retail, manifest["retail"], contracts)]
    print(json.dumps({"result": "PASS_OFFLINE_ONLY", "inputs": result}, indent=2))


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, KeyError, pefile.PEFormatError, struct.error) as error:
        raise SystemExit(f"CE evidence verification FAILED: {error}")
