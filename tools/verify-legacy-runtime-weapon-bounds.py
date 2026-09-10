"""Offline native proof for H3/ODST/Reach runtime equipped-model bounds."""
from pathlib import Path
import hashlib
import re
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/re'))
from pe import PE

source = (ROOT / 'src/dll/legacy_runtime_weapon_bounds.inl').read_text()
inputs = ROOT / 'out/deps/re-tools/inputs'
titles = [
    ('Halo3', 'halo3.dll', 'halo3_tag_test.exe',
     '59A78F2C96034D7CEB5D710505B2B36813AA141FC81A083E3F952973DBCE4602',
     0x2B58C9, 0x28E8E0),
    ('Halo3ODST', 'halo3odst.dll', 'halo3odst_tag_test.exe',
     '354EC94158AECCE3E9D0F6463023AD5FA6D2AFE49B390E6067EBC17465C63C2D',
     0x2DD7F0, 0x2B9B70),
    ('HaloReach', 'haloreach.dll', 'reach_tag_test.exe',
     'CBDD8448A87A433B0DFFC0DE47D06DB7A18B4BF868B96B057135DAA86790ABA8',
     0x2560D1, 0x284FDC),
]
for title, filename, kitname, kit_hash, call, decoder in titles:
    pe = PE(inputs / filename)
    windows = re.findall(r'\{GameTitle::' + title +
                         r',(0x[0-9A-F]+),"([0-9A-F ]+)"\}', source)
    assert len(windows) == 3
    for rva, pattern in windows:
        matches = [pe.rva(m.start()) for m in
                   re.finditer(re.escape(bytes.fromhex(pattern)), pe.blob)]
        assert matches == [int(rva, 16)], (title, rva, matches)
        print('PASS unique', title, rva)
    offset = pe.off(call)
    assert pe.blob[offset] == 0xE8
    assert call + 5 + struct.unpack_from('<i', pe.blob, offset + 1)[0] == decoder
    assert hashlib.sha256((inputs / kitname).read_bytes()).hexdigest().upper() == kit_hash
    print('PASS native decoder edge and official kit identity:', title)
    print(filename, 'SHA-256:', hashlib.sha256(pe.blob).hexdigest().upper())
print('Read-only file verification; no process access or game writes.')
