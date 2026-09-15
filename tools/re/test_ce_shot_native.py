"""Execute pinned CE shot-adjustment instructions against isolated fake memory.

Read-only game-file input; no process access, injection or game-file writes.
Requires pefile and Unicorn. External engine services (unit origin, collision,
velocity, security-cookie check) are explicit deterministic stubs. The native
adjustment/projection/vector arithmetic itself executes without replacement.
This verifies the hook ABI/boolean meaning, not a headset or live weapon result.
"""
import argparse
import hashlib
import math
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "out/pydeps"))
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64, UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_RSP, UC_X86_REG_RIP,
    UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_R9,
    UC_X86_REG_RAX, UC_X86_REG_XMM0)

PINNED_SHA = "0A12DC561780F449D3F4D0DF10BB8D3BC7BE7840A5BEB2B236F672EB6CD42E6C"
BASE, HEAP, STACK, STOP = 0x180000000, 0x60000000, 0x70000000, 0x71000000
ORIGIN, VELOCITY, UNIT_AIM = (100.0, 200.0, 300.0), (0.2, 0.3, 0.4), (0.0, 1.0, 0.0)


def close(a, b):
    return len(a) == len(b) and all(abs(x-y) < 0.0001 for x,y in zip(a,b))


def run(image, legacy, direction, project_origin, use_unit_aim):
    uc = Uc(UC_ARCH_X86, UC_MODE_64)
    uc.mem_map(BASE, (len(image)+4095)&~4095)
    uc.mem_write(BASE, image)
    uc.mem_map(HEAP, 0x10000)
    uc.mem_map(STACK, 0x20000)
    uc.mem_map(STOP, 0x1000)
    def u64(at, value): uc.mem_write(at, struct.pack("<Q", value))
    def vec(at, value): uc.mem_write(at, struct.pack("<3f", *value))
    def read_vec(at): return struct.unpack("<3f", uc.mem_read(at, 12))
    def ret():
        sp = uc.reg_read(UC_X86_REG_RSP)
        uc.reg_write(UC_X86_REG_RIP, struct.unpack("<Q", uc.mem_read(sp, 8))[0])
        uc.reg_write(UC_X86_REG_RSP, sp+8)
    def service(machine, address, size, _):
        rva = address-BASE
        if address == STOP:
            machine.emu_stop()
        elif rva == 0xb04d00:
            vec(machine.reg_read(UC_X86_REG_RDX), ORIGIN); ret()
        elif rva == 0xb93c8c:
            machine.reg_write(UC_X86_REG_RAX, 0); ret()
        elif rva == 0xb38ba8:
            machine.reg_write(UC_X86_REG_RAX, 0x12340007); ret()
        elif rva == 0xb38398:
            vec(machine.reg_read(UC_X86_REG_RDX), VELOCITY); ret()
        elif rva == 0xa9bd08:
            ptr = machine.reg_read(UC_X86_REG_RCX)
            source = read_vec(ptr)
            length = math.sqrt(sum(x*x for x in source))
            if length: vec(ptr, [x/length for x in source])
            machine.reg_write(UC_X86_REG_XMM0, struct.unpack("<I", struct.pack("<f", length))[0]); ret()
        elif rva == 0x1391640:
            ret()
    uc.hook_add(UC_HOOK_CODE, service)
    # The native object header table uses index low16, 0xC stride and an
    # allocation-relative +0x34 payload, all witnessed in the CE functions.
    headers, objects = HEAP+0x2000, HEAP+0x4000
    u64(BASE+0x1c42248, headers); u64(BASE+0x2d9cdf8, objects)
    uc.mem_write(headers+0x34, struct.pack("<i", 0x100))
    uc.mem_write(headers+0x100+7*12+8, struct.pack("<i", 0x400))
    vec(objects+0x400+0x250, UNIT_AIM)
    position, aim, inherited = HEAP+0x100, HEAP+0x120, HEAP+0x140
    original_position = (102.0, 203.0, 304.0)
    vec(position, original_position); vec(aim, direction)
    sp = STACK+0x1ff08
    u64(sp, STOP)
    uc.reg_write(UC_X86_REG_RSP, sp)
    uc.reg_write(UC_X86_REG_RCX, 0x12340007)
    uc.reg_write(UC_X86_REG_RDX, position)
    uc.reg_write(UC_X86_REG_R8, aim)
    uc.reg_write(UC_X86_REG_R9, inherited)
    if legacy:
        u64(sp+0x28, int(project_origin)); u64(sp+0x30, int(use_unit_aim))
    else:
        u64(sp+0x28, 0); u64(sp+0x30, int(project_origin)); u64(sp+0x38, int(use_unit_aim))
    uc.emu_start(BASE+(0xb00740 if legacy else 0xb00880), STOP, count=20000)
    expected_aim = UNIT_AIM if use_unit_aim else direction
    distance = sum((p-o)*d for p,o,d in zip(original_position, ORIGIN, expected_aim))
    expected_position = tuple(o+d*distance for o,d in zip(ORIGIN,expected_aim)) if project_origin else original_position
    actual_aim, actual_position = read_vec(aim), read_vec(position)
    assert close(actual_aim, expected_aim), ("direction", legacy, use_unit_aim, actual_aim, expected_aim)
    assert close(actual_position, expected_position), ("position", actual_position, expected_position)
    if legacy:
        actual = struct.unpack("<f", uc.mem_read(inherited, 4))[0]
        expected = sum(v*d for v,d in zip(VELOCITY, expected_aim))
        assert abs(actual-expected) < 0.0001, ("inherited speed", actual, expected)
    else:
        assert close(read_vec(inherited), VELOCITY)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", type=Path, default=ROOT/"out/deps/re-tools/inputs/halo1.dll")
    args = parser.parse_args()
    raw = args.image.read_bytes()
    assert hashlib.sha256(raw).hexdigest().upper() == PINNED_SHA, "Pinned CE module SHA mismatch"
    image = pefile.PE(data=raw, fast_load=True).get_memory_mapped_image()
    count = 0
    for legacy in (False, True):
        for direction in ((1.0,0.0,0.0), (0.0,0.0,1.0), (0.6,0.0,0.8), (-0.6,0.8,0.0)):
            for project_origin in (False, True):
                for use_unit_aim in (False, True):
                    run(image, legacy, direction, project_origin, use_unit_aim)
                    count += 1
    print(f"PASS: {count} actual native CE modern/legacy shot cases; controller/native overwrite switch, origin projection, inherited velocity.")
    print("LIMIT: surrounding engine services are stubbed; live trigger/local ownership, collision and headset alignment remain untested.")


if __name__ == "__main__":
    main()
