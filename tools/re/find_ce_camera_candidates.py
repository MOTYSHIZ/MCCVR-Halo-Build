"""Offline leads for matching the HCEEK camera projection in pinned x64 CE.

Reads PE unwind ranges, then requires the kit's four viewport word reads and
vertical-FOV float read. Matches are candidates, never approved bindings.
"""
import sys
import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64

pe = pefile.PE(sys.argv[1])
decoder = Cs(CS_ARCH_X86, CS_MODE_64)
for entry in pe.DIRECTORY_ENTRY_EXCEPTION:
    start, end = entry.struct.BeginAddress, entry.struct.EndAddress
    if end - start < 180:
        continue
    data = pe.get_data(start, end - start)
    if not all(bytes([v]) in data for v in (0x2c, 0x2e, 0x30, 0x32)):
        continue
    found = set()
    fov = False
    lines = []
    for address, size, mnemonic, operands in decoder.disasm_lite(data, start):
        if 'word ptr' in operands and mnemonic.startswith('movs'):
            for offset in (0x2c, 0x2e, 0x30, 0x32):
                if f' + {offset:#x}]' in operands:
                    found.add(offset)
                    lines.append(f'{address:x}: {mnemonic} {operands}')
        if mnemonic == 'movss' and ' + 0x28]' in operands:
            fov = True
    if len(found) == 4:
        print(f'CANDIDATE {start:x}-{end:x} fov28={fov}')
        print('\n'.join(lines[:16]))
