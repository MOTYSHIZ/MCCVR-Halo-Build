"""Locate raw RIP-relative kit string consumers without relying on analysis coverage."""
import bisect
import pathlib
import re
import struct
import sys
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / 'out/pydeps'))
import pefile

root = pathlib.Path(__file__).resolve().parents[2]
fire = '--fire' in sys.argv
needles = ('weapon_barrel_fire', 'barrel_index') if fire else (
    'secondary_weapon', 'can be dual wielded', 'can dual wield', 'can_be_dual_wielded')
for name in ('halo3_tag_test.exe', 'halo3odst_tag_test.exe', 'reach_tag_test.exe', 'halo4_tag_test.exe'):
    pe = pefile.PE(str(root / 'out/deps/re-tools/inputs' / name))
    image = pe.get_memory_mapped_image()
    targets = {}
    for m in re.finditer(rb'[\x20-\x7e]{5,}\x00', image):
        value = m.group()[:-1].decode('ascii')
        if any(s in value.lower() for s in needles):
            targets[m.start()] = value
    entries = [(e.struct.BeginAddress, e.struct.EndAddress) for e in pe.DIRECTORY_ENTRY_EXCEPTION]
    entries.sort()
    starts = [e[0] for e in entries]
    print(name, 'strings', len(targets))
    extra = {}
    for target, value in targets.items():
        if fire or any(s in value for s in ('unit_get_current_secondary_weapon', 'secondary_weapon_index != NONE', 'can be dual wielded', 'can dual wield')):
            pointer = struct.pack('<Q', pe.OPTIONAL_HEADER.ImageBase + target)
            hits = [m.start() for m in re.finditer(re.escape(pointer), image)]
            print('DATA', hex(target), value, [hex(h) for h in hits[:12]])
            for hit in hits:
                for delta in range(0, 40, 8):
                    extra[hit-delta] = 'record for ' + value
    targets.update(extra)
    for section in pe.sections:
        if not section.Characteristics & 0x20000000:
            continue
        offset = section.VirtualAddress
        data = image[offset:offset+section.Misc_VirtualSize]
        for m in re.finditer(rb'[\x48-\x4f]\x8d[\x05\x0d\x15\x1d\x25\x2d\x35\x3d]', data):
            instruction = offset + m.start()
            target = instruction + 7 + struct.unpack_from('<i', image, instruction+3)[0]
            if target not in targets:
                continue
            index = bisect.bisect_right(starts, instruction)-1
            begin, end = entries[index] if index >= 0 else (0, 0)
            print(hex(instruction), 'function', hex(begin) if instruction < end else '?',
                  'string', hex(target), targets[target])
