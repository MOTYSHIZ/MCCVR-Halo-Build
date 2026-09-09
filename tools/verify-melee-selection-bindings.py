"""Offline physical-melee selection proof; no process access or game writes."""
import hashlib
import pathlib
import re
import struct
import sys

root = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / 'out/pydeps'))
import pefile

raw = (root / 'out/deps/re-tools/inputs/halo3.dll').read_bytes()
assert hashlib.sha256(raw).hexdigest().upper() == (
    'B209D8454B12DC77E54CCD2C9924EC8D44B8619D21CF98E36FFAF601E67EFB63'
), 'Pinned Halo 3 module changed'
pe = pefile.PE(data=raw)
image = pe.get_memory_mapped_image()
source = (root / 'src/dll/halo3_melee_selection.inl').read_text()
pattern = re.search(r'constexpr char pattern\[\]="([0-9A-F ]+)"', source).group(1)
matches = [m.start() for m in re.finditer(re.escape(bytes.fromhex(pattern)), image)]
assert matches == [0x35A9A4], matches
extent = [(e.struct.BeginAddress, e.struct.EndAddress) for e in pe.DIRECTORY_ENTRY_EXCEPTION
          if e.struct.BeginAddress == 0x35A9A4]
assert extent == [(0x35A9A4, 0x35AB9A)], extent
for caller, target in [(0x35B74C, 0x35A9A4), (0x35E87E, 0x35A9A4),
                       (0x35AA26, 0x356388)]:
    assert image[caller] == 0xE8
    assert caller + 5 + struct.unpack_from('<i', image, caller + 1)[0] == target
assert struct.unpack_from('<f', image, 0x846EC0)[0] == 1.0
# Instruction-level checks of H3EK-proven fields, including the primary role,
# weapon state, material, charged/normal response and unit fallback.
for address, expected in {
    0x35AA1C: '0F BE 92 62 02 00 00',
    0x35AA79: '8B 81 8C 01 00 00',
    0x35AA8F: 'F3 0F 10 86 8C 01 00 00',
    0x35AAA3: '8A 81 1C 03 00 00',
    0x35AAB3: '48 8D 91 CC 02 00 00',
    0x35AAFE: '48 8D 91 4C 02 00 00',
    0x35AB05: '4C 8D 81 EC 02 00 00',
    0x35AB54: '8B 85 B4 01 00 00',
}.items():
    expected = bytes.fromhex(expected)
    assert image[address:address+len(expected)] == expected, hex(address)
print('PASS: pinned H3 selector, unique entry, unwind extent, native calls, response fields and charged threshold')
