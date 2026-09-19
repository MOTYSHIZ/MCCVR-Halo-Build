"""Verify H3EK-matched dual-fire bindings against the pinned offline retail PE."""
import hashlib
from pathlib import Path
import re
import struct
import sys

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / 'out/pydeps'))
import pefile

raw = (root / 'out/deps/re-tools/inputs/halo3.dll').read_bytes()
assert hashlib.sha256(raw).hexdigest().upper() == (
    'B209D8454B12DC77E54CCD2C9924EC8D44B8619D21CF98E36FFAF601E67EFB63')
pe = pefile.PE(data=raw)
image = pe.get_memory_mapped_image()
source = (root / 'src/dll/halo3_dual_lifecycle.inl').read_text()
bindings = re.findall(r'\{0x([0-9A-F]+), "([0-9A-F ]+)"\}', source)
assert len(bindings) == 5
for rva, pattern in bindings:
    matches = [m.start() for m in re.finditer(re.escape(bytes.fromhex(pattern)), image)]
    assert matches == [int(rva, 16)], matches
for begin, end in [(0x3683A0, 0x369FB9), (0x3524B0, 0x3527D0),
                   (0x13B08C,0x13B6F8),(0x212198,0x2121EE),(0x13BAD0,0x13C3DD)]:
    assert [(e.struct.BeginAddress, e.struct.EndAddress)
            for e in pe.DIRECTORY_ENTRY_EXCEPTION if e.struct.BeginAddress == begin] == [(begin, end)]
assert image[0x368B92] == 0xE8
assert 0x368B97 + struct.unpack_from('<i', image, 0x368B93)[0] == 0x3524B0
for call,target in [(0x13B1D7,0x212198),(0x13BC08,0x212198),
                    (0xF4CC8,0x13B08C),(0x368DFE,0x13BAD0)]:
    assert image[call]==0xE8 and call+5+struct.unpack_from('<i',image,call+1)[0]==target
for address, pattern in {
    0x3563A4: 'BA 38 00 00 00',  # H3 object TLS slot
    0x3563C0: '48 8B 44 D0 10',  # H3 entry data pointer
    0x3563C5: '8B 84 88 68 02 00 00',  # four-handle inventory
    0x364E48: '80 B9 5D 01 00 00 00',  # owner-valid byte
    0x364E51: '8B 81 68 01 00 00',  # full owner handle
    0x368A6A: '4C 8D AB 18 02 00 00',  # effective unit targeting
    0x368AE5: '44 8B 83 A4 02 00 00',  # controlling parent
    0x368B0B: '8B 90 A4 02 00 00',
    0x368B11: '4C 8D A8 18 02 00 00',
}.items():
    expected = bytes.fromhex(pattern)
    assert image[address:address + len(expected)] == expected, hex(address)
print('PASS: pinned Halo 3 identity, five unique firing/query/camera bindings, unwind extents, native edges and H3 inventory/owner/targeting fields')
