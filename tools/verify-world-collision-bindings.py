"""Read-only verification against the pinned editing-kit and retail inputs.

Usage: py tools/verify-world-collision-bindings.py <input-directory>
Requires pefile. Never attaches to MCC or writes game files.
"""
import hashlib
import json
from pathlib import Path
import re
import struct
import sys

import pefile

root = Path(sys.argv[1])
source = (Path(__file__).resolve().parents[1] / 'src/dll/game.cpp').read_text(encoding='utf-8')
identities = {
    'halo3.dll': 'B209D8454B12DC77E54CCD2C9924EC8D44B8619D21CF98E36FFAF601E67EFB63',
    'halo3odst.dll': '5BB20976EFDFD9E1CE59C589339804725FEC239021027C8D65B2733EAB94829A',
    'haloreach.dll': '738DD2D24EA3AEA12E1EE9AA4A61094BF116027D42004C35A19E5048608B0894',
    'reach_tag_test.exe': 'CBDD8448A87A433B0DFFC0DE47D06DB7A18B4BF868B96B057135DAA86790ABA8',
    'halo3_tag_test.exe': '59A78F2C96034D7CEB5D710505B2B36813AA141FC81A083E3F952973DBCE4602',
    'halo3odst_tag_test.exe': '354EC94158AECCE3E9D0F6463023AD5FA6D2AFE49B390E6067EBC17465C63C2D',
}
images = {}
report = {}
for name, expected in identities.items():
    data = (root / name).read_bytes()
    actual = hashlib.sha256(data).hexdigest().upper()
    assert actual == expected, (name, 'pinned identity mismatch', actual)
    images[name] = pefile.PE(data=data, fast_load=True)
    report[name] = {'sha256': actual}

def call_target(image, rva):
    data = image.get_data(rva, 5)
    assert data[0] == 0xE8, ('not a direct call', hex(rva))
    return rva + 5 + struct.unpack_from('<i', data, 1)[0]

for name, symbol, segment_call, vector_rva in [
    ('halo3.dll', 'kHalo3CollisionVectorSignature', 0x1FE63F, 0x1FD748),
    ('halo3odst.dll', 'kOdstCollisionVectorSignature', 0x2307DB, 0x22F80C),
    ('haloreach.dll', 'kReachCollisionVectorSignature', 0x12B046, 0x12969C),
]:
    body = re.search(r'char\s+' + symbol + r'\[\]\s*=([^;]+);', source).group(1)
    tokens = ' '.join(re.findall(r'"([^"]*)"', body)).split()
    pattern = b''.join(b'.' if token == '??' else re.escape(bytes.fromhex(token)) for token in tokens)
    image = images[name]
    matches = [match.start() for match in re.finditer(pattern, image.get_memory_mapped_image(), re.S)]
    assert matches == [vector_rva], (name, 'signature is not unique at pinned entry', matches)
    assert call_target(image, segment_call) == vector_rva
    report[name].update(vector_rva=hex(vector_rva), signature_matches=1,
                        segment_adapter_call=hex(segment_call))

assert call_target(images['haloreach.dll'], 0x12C63D) == 0x12AFCC

for name, call, target in [
    ('halo3_tag_test.exe', 0x64D055, 0x652A10),
    ('halo3odst_tag_test.exe', 0x69E115, 0x6A3B30),
    ('reach_tag_test.exe', 0x41677C, 0x41B960),
]:
    assert call_target(images[name], call) == target
    report[name].update(resolve_to_vector_call=hex(call), vector_rva=hex(target))

h2data = (root / 'halo2.dll').read_bytes()
assert hashlib.sha256(h2data).hexdigest().upper() == 'DE65B4F4FDBF3F0A5EAB7431FE530DA17DD815599182DFD6AE9B7E21CF171946', 'H2 pinned identity mismatch'
h2 = pefile.PE(data=h2data, fast_load=True)
getter = h2.get_data(0x79EEA0, 26)
expected = bytes.fromhex('48 8B 05 89 5C E4 00 0F B7 C9 48 03 C9 48 63 44 C8 08 48 03 05 7F 5C E4 00 C3')
assert getter == expected, 'H2 tag getter differs from exact evidence'
slot = 0x79EEA0 + 25 + struct.unpack_from('<i', getter, 21)[0]
rejected_slot = 0x79EEA0 + 24 + struct.unpack_from('<i', getter, 20)[0]
assert slot == 0x15E4B38 and slot < h2.OPTIONAL_HEADER.SizeOfImage
assert not 0 <= rejected_slot < h2.OPTIONAL_HEADER.SizeOfImage
report['halo2.dll'] = {'sha256': hashlib.sha256(h2data).hexdigest().upper(),
                       'tag_data_slot_rva': hex(slot), 'rejected_slot_rva': hex(rejected_slot)}
print(json.dumps(report, indent=2))
