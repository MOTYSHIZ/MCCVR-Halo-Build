"""Verify read-only H4 seat/input bindings and their tracked generated header."""
import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'out/pydeps'))
import pefile


def main():
    evidence = json.loads((ROOT / 'docs/HALO4-VEHICLE-INPUT-BINDINGS-2026-09-18.json').read_text())
    raw = (ROOT / 'out/deps/re-tools/inputs/halo4.dll').read_bytes()
    if hashlib.sha256(raw).hexdigest().upper() != evidence['sha256']:
        raise ValueError('Pinned H4 module hash mismatch')
    image = pefile.PE(data=raw).get_memory_mapped_image()
    lines = ['// Pinned H4-only read contracts; see HALO4-VEHICLE-INPUT-BINDINGS-2026-09-18.json.',
             'struct Halo4VehicleInputBinding { uintptr_t rva; const char* pattern; };',
             'static constexpr Halo4VehicleInputBinding kHalo4VehicleInputBindings[]{']
    for binding in evidence['bindings']:
        code = bytes.fromhex(binding['signature'])
        rva = int(binding['rva'], 16)
        if image.count(code) != 1 or image[rva:rva + len(code)] != code:
            raise ValueError(f'Missing/ambiguous H4 binding: {binding["name"]}')
        lines.append('    {' + binding['rva'] + ',"' + binding['signature'] + '"},')
    lines.extend(['};', ''])
    expected = '\n'.join(lines)
    actual = (ROOT / 'src/dll/halo4_vehicle_input_bindings.generated.h').read_text()
    if actual != expected:
        raise ValueError('Generated H4 vehicle input header differs from evidence')
    print(f'Verified {len(evidence["bindings"])} unique H4 read contracts and generated header')


if __name__ == '__main__':
    main()
