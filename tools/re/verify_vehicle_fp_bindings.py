"""Verify pinned per-title vehicle-camera bindings, including existing muzzle hooks."""
import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'out/pydeps'))
import pefile

contracts = json.loads((ROOT / 'docs/NATIVE-VEHICLE-FIRST-PERSON-CONTRACTS.json').read_text())
header = (ROOT / 'src/dll/native_vehicle_first_person_bindings.generated.h').read_text()
total = 0
for contract in contracts:
    raw = (ROOT / 'out/deps/re-tools/inputs' / contract['module']).read_bytes()
    assert hashlib.sha256(raw).hexdigest() == contract['sha256'], contract['module']
    image = bytearray(pefile.PE(data=raw).get_memory_mapped_image())
    for detoured in (False, True):
        if detoured and contract['title'] in ('Halo2', 'Halo4'):
            # Existing muzzle hooks own only the helper entry. Cold vehicle
            # proofs must remain valid with that entry already redirected.
            start = int(contract['marker_rva'], 16)
            image[start:start + 14] = b'\xcc' * 14
        for binding in contract['bindings']:
            code = bytes.fromhex(binding['signature'])
            rva = int(binding['rva'], 16)
            assert image.count(code) == 1 and image[rva:rva + len(code)] == code, (contract['title'], binding, detoured)
            assert '{' + binding['rva'] + ',"' + binding['signature'] + '"}' in header
            total += 1
print(f'{total} pinned vehicle-camera signature checks passed (stock and muzzle-hook entries)')
