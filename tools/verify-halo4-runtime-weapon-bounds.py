"""Verify the read-only H4 runtime-bounds proof against the pinned local images."""
from pathlib import Path
import hashlib
import re
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/re'))
from pe import PE

inputs = ROOT / 'out/deps/re-tools/inputs'
retail = PE(inputs / 'halo4.dll')
kit = PE(inputs / 'halo4_tag_test.exe')
source = (ROOT / 'src/dll/halo4_runtime_weapon_bounds.inl').read_text()
windows = re.findall(r'\{(0x[0-9A-Fa-f]+),\s*((?:"[0-9A-Fa-f ]+"\s*)+)\}', source)
assert len(windows) == 3
for address, fragments in windows:
    pattern = bytes.fromhex(''.join(re.findall(r'"([0-9A-Fa-f ]+)"', fragments)))
    matches = [retail.rva(m.start()) for m in re.finditer(re.escape(pattern), retail.blob)]
    assert matches == [int(address, 16)], (address, matches)
    print(f'PASS unique native bounds proof at {address}')

# Exact renderer call edge and packed-pointer table identity.
def signed(rva):
    return struct.unpack_from('<i', retail.blob, retail.off(rva))[0]

assert retail.blob[retail.off(0x34466E)] == 0xE8
assert 0x344673 + signed(0x34466F) == 0x387AB8
assert 0x344663 + signed(0x34465F) == 0x496A180
assert hashlib.sha256(kit.blob).hexdigest().upper() == (
    'B7468DB9FD160B035C329540EE0B0D47BCF609E1BA6E85AE4F204B70661113A6')
assert b'_compression_optimized_bit' in kit.blob
print('PASS native decoder call edge, packed table, official H4EK identity')
print('Retail SHA-256:', hashlib.sha256(retail.blob).hexdigest().upper())
print('No process access, injection, engine calls or writes performed.')
