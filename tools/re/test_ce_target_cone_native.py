"""Run CE's pinned native target-cone math offline with two aim directions.

This exercises the real eligibility instructions reached by continuous target
acquisition. It does not emulate the world, team service, LOS, or headset.
"""
import hashlib
import math
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'out/pydeps'))
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64
from unicorn.x86_const import (UC_X86_REG_RSP, UC_X86_REG_RCX,
    UC_X86_REG_R8, UC_X86_REG_R9, UC_X86_REG_XMM1, UC_X86_REG_RAX)

BASE, HEAP, STACK, STOP = 0x180000000, 0x60000000, 0x70000000, 0x71000000
raw = (ROOT / 'out/deps/re-tools/inputs/halo1.dll').read_bytes()
assert hashlib.sha256(raw).hexdigest().upper() == '0A12DC561780F449D3F4D0DF10BB8D3BC7BE7840A5BEB2B236F672EB6CD42E6C'
mapped = pefile.PE(data=raw, fast_load=True).get_memory_mapped_image()
uc = Uc(UC_ARCH_X86, UC_MODE_64)
uc.mem_map(BASE, (len(mapped)+4095)&~4095)
uc.mem_write(BASE, mapped)
for address in (HEAP, STACK, STOP):
    uc.mem_map(address, 0x10000)


def query(target, direction, radius=.25, distance=10, angle=.1):
    def write(at, fmt, *values):
        uc.mem_write(at, struct.pack(fmt, *values))
    write(HEAP, '<3f', *target)
    write(HEAP+0x20, '<3f', 0, 0, 0)
    write(HEAP+0x40, '<3f', *direction)
    sp = STACK+0xff08
    write(sp, '<Q', STOP)
    write(sp+0x28, '<f', distance)
    write(sp+0x30, '<f', math.sin(angle))
    write(sp+0x38, '<f', math.cos(angle))
    uc.reg_write(UC_X86_REG_RSP, sp)
    uc.reg_write(UC_X86_REG_RCX, HEAP)
    uc.reg_write(UC_X86_REG_XMM1, struct.unpack('<I', struct.pack('<f', radius))[0])
    uc.reg_write(UC_X86_REG_R8, HEAP+0x20)
    uc.reg_write(UC_X86_REG_R9, HEAP+0x40)
    uc.emu_start(BASE+0xb6e60c, STOP, count=1000)
    return bool(uc.reg_read(UC_X86_REG_RAX)&255)


checks = 0
for direction in ((1,0,0), (0,1,0), (0,0,1), (-1,0,0)):
    perpendicular = (0,1,0) if abs(direction[0]) else (1,0,0)
    target = tuple(v*5 for v in direction)
    assert query(target, direction)
    assert not query(target, perpendicular)
    assert not query(tuple(v*12 for v in direction), direction)
    assert not query(tuple(v*-5 for v in direction), direction)
    assert query(tuple(v*10.1 for v in direction), direction)
    assert not query(tuple(v*10.3 for v in direction), direction)
    checks += 6
print(f'PASS: {checks} native CE target-cone direction/range/radius/behind-player cases')
print('LIMIT: native cone math only; team/LOS/world acquisition and headset appearance require runtime coverage.')
