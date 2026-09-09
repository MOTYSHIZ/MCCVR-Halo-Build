"""Offline kit string and pinned H2 firing-boundary inspection (no process access)."""
import hashlib
import pathlib
import re
import struct
import sys
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / 'out/pydeps'))
import pefile

root = pathlib.Path(__file__).resolve().parents[2]
inputs = root / 'out/deps/re-tools/inputs'
for filename in ('halo2.dll', 'halo3odst_tag_test.exe', 'reach_tag_test.exe', 'halo4_tag_test.exe'):
    raw = (inputs / filename).read_bytes()
    pe = pefile.PE(data=raw)
    print(filename, hashlib.sha256(raw).hexdigest())
    if filename == 'halo2.dll':
        header = (root / 'src/common/halo2_render_logic.h').read_text()
        identities = header.split('kHalo2RetailModuleSha256[]', 1)[1].split('};', 1)[0]
        assert hashlib.sha256(raw).hexdigest().lower() in identities.lower(), 'Unpinned H2 module'
        image = pe.get_memory_mapped_image()
        for rva in (0x8E4940, 0x8F0F70):
            pattern = image[rva:rva+32]
            matches = [m.start() for m in re.finditer(re.escape(pattern), image)]
            print(hex(rva), pattern.hex(' ').upper(), 'matches', [hex(m) for m in matches])
            assert matches == [rva], 'H2 dual-aim binding missing or ambiguous'
        assert image[0x8E4FC8] == 0xE8
        assert 0x8E4FCD + struct.unpack_from('<i', image, 0x8E4FC9)[0] == 0x8F0F70
        print('PASS: pinned H2 identity, unique fire/helper entries and exact firing call edge')
        continue
    for match in re.finditer(rb'[\x20-\x7e]{5,}\x00', raw):
        text = match.group()[:-1].decode('ascii')
        if any(word in text.lower() for word in ('dual_wield', 'dual wield', 'dual-wield', 'secondary_weapon')):
            print(hex(pe.get_rva_from_offset(match.start())), text[:500])
