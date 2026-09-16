"""Execute the pinned CE observer-to-camera effect composition offline.

No game process access. The native camera builder and matrix routines execute;
the effect generator, perspective and FOV queries are explicit fixture services.
This verifies where the camera consumes recoil/shake, and the identity matrix
needed to suppress only that returned transform while keeping native updates.
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
from unicorn.x86_const import *

PINNED = "0A12DC561780F449D3F4D0DF10BB8D3BC7BE7840A5BEB2B236F672EB6CD42E6C"
BASE, HEAP, STACK, STOP = 0x180000000, 0x60000000, 0x70000000, 0x71000000


def require(value, message):
    if not value:
        raise AssertionError(message)


def run(image, yaw, displacement, suppressed):
    uc = Uc(UC_ARCH_X86, UC_MODE_64)
    uc.mem_map(BASE, (len(image) + 4095) & ~4095)
    uc.mem_write(BASE, image)
    uc.mem_map(HEAP, 0x10000)
    uc.mem_map(STACK, 0x10000)
    uc.mem_map(STOP, 0x1000)
    window, observer, clock, cinematic = [HEAP + n * 0x1000 for n in range(1, 5)]
    uc.mem_write(BASE + 0x2e9fd68, struct.pack("<Q", clock))
    uc.mem_write(BASE + 0x2ea0208, struct.pack("<Q", cinematic))
    uc.mem_write(BASE + 0x2dcfc00, b"\0")
    uc.mem_write(BASE + 0x1c34ed5, b"\0")
    uc.mem_write(window, struct.pack("<h", 0))
    uc.mem_write(window + 0x84, struct.pack("<hhhh", 0, 0, 720, 1280))
    source = (3.0, -2.0, 1.5)
    uc.mem_write(observer, struct.pack("<3f", *source))
    uc.mem_write(observer + 0x20, struct.pack("<3f", 1, 0, 0))
    uc.mem_write(observer + 0x2c, struct.pack("<3f", 0, 0, 1))
    uc.mem_write(observer + 0x38, struct.pack("<f", 1.2))
    effects = 0
    instructions = 0

    def ret():
        sp = uc.reg_read(UC_X86_REG_RSP)
        uc.reg_write(UC_X86_REG_RIP, struct.unpack("<Q", uc.mem_read(sp, 8))[0])
        uc.reg_write(UC_X86_REG_RSP, sp + 8)

    def step(machine, address, size, _):
        nonlocal effects, instructions
        instructions += 1
        rva = address - BASE
        if rva == 0xb14ea4:
            uc.reg_write(UC_X86_REG_RAX, 0)
            ret()
        elif rva == 0xac4494:
            # Native FOV policy is outside this pose-only experiment.
            ret()
        elif rva == 0xbac0cc:
            require(uc.reg_read(UC_X86_REG_RCX) == 0, "Camera effect user ABI")
            require(struct.unpack("<Q", uc.mem_read(uc.reg_read(UC_X86_REG_RSP), 8))[0]
                    == BASE + 0xac461d, "Exact camera effect caller")
            effects += 1
            a = 0 if suppressed else yaw
            d = (0, 0, 0) if suppressed else displacement
            matrix = (1, math.cos(a), math.sin(a), 0,
                      -math.sin(a), math.cos(a), 0, 0, 0, 1, *d)
            uc.mem_write(uc.reg_read(UC_X86_REG_RDX), struct.pack("<13f", *matrix))
            ret()
        elif not any(start <= rva < end for start, end in (
                (0xac450c, 0xac4771), (0xba24ec, 0xba25c0), (0xba3360, 0xba3600))):
            raise AssertionError(f"Unexpected native call/instruction {rva:x}")

    uc.hook_add(UC_HOOK_CODE, step)
    sp = STACK + 0xff08
    uc.mem_write(sp, struct.pack("<Q", STOP))
    uc.reg_write(UC_X86_REG_RSP, sp)
    uc.reg_write(UC_X86_REG_RCX, window)
    uc.reg_write(UC_X86_REG_RDX, observer)
    uc.emu_start(BASE + 0xac450c, STOP, count=20000)
    require(uc.reg_read(UC_X86_REG_RIP) == STOP and effects == 1,
            "Native builder returned after one effect query")
    render = bytes(uc.mem_read(window + 4, 0x24))
    raster = bytes(uc.mem_read(window + 0x58, 0x24))
    require(render == raster, "Render and raster both consume the same effect")
    result = struct.unpack("<9f", render)
    if suppressed:
        expected = (*source, 1, 0, 0, 0, 0, 1)
        require(all(abs(a - b) < 1e-5 for a, b in zip(result, expected)),
                ("Identity effect keeps native camera unchanged", result))
    else:
        require(any(abs(a - b) > 1e-4 for a, b in zip(result,
                (*source, 1, 0, 0, 0, 0, 1))), "Fixture effect changes native camera")
    return instructions


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", type=Path,
                        default=ROOT / "out/deps/re-tools/inputs/halo1.dll")
    args = parser.parse_args()
    raw = args.image.read_bytes()
    require(hashlib.sha256(raw).hexdigest().upper() == PINNED, "Pinned CE image")
    image = pefile.PE(data=raw, fast_load=True).get_memory_mapped_image()
    count = instructions = 0
    for yaw, displacement in ((.13, (0, 0, 0)), (-.2, (.02, -.03, .015)),
                              (0, (0, 0, .025))):
        for suppressed in (False, True):
            instructions += run(image, yaw, displacement, suppressed)
            count += 1
    print(f"PASS: {count} native CE camera effect cases, {instructions} instructions.")
    print("LIMIT: effect generator is a service fixture; no live renderer or headset result.")


if __name__ == "__main__":
    main()
