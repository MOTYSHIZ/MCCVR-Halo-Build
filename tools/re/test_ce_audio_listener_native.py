"""Execute pinned CE sound-manager listener packing and backend orientation offline.

No game process access. Native matrix conversion and the backend's handoff to
the audio interface execute; clock/player/liquid/interface services are fixtures.
"""
import argparse
import hashlib
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


class Machine:
    def __init__(self, image):
        self.uc = Uc(UC_ARCH_X86, UC_MODE_64)
        self.uc.mem_map(BASE, (len(image) + 4095) & ~4095)
        self.uc.mem_write(BASE, image)
        for address, length in ((HEAP, 0x10000), (STACK, 0x10000), (STOP, 0x1000)):
            self.uc.mem_map(address, length)
        self.instructions = 0

    def write(self, address, fmt, *values):
        self.uc.mem_write(address, struct.pack(fmt, *values))

    def read(self, address, fmt):
        return struct.unpack(fmt, self.uc.mem_read(address, struct.calcsize(fmt)))

    def ret(self):
        sp = self.uc.reg_read(UC_X86_REG_RSP)
        self.uc.reg_write(UC_X86_REG_RIP, self.read(sp, "<Q")[0])
        self.uc.reg_write(UC_X86_REG_RSP, sp + 8)

    def run(self, rva, hook):
        def step(uc, address, size, _):
            self.instructions += 1
            hook(address - BASE)
        self.uc.hook_add(UC_HOOK_CODE, step)
        sp = STACK + 0xff08
        self.write(sp, "<Q", STOP)
        self.uc.reg_write(UC_X86_REG_RSP, sp)
        self.uc.emu_start(BASE + rva, STOP, count=50000)
        require(self.uc.reg_read(UC_X86_REG_RIP) == STOP, "Native routine returned")


def native_packet(image, forward, up):
    m = Machine(image)
    observer = BASE + 0x2d9bdd4
    players, scenario, globals_ = HEAP + 0x1000, HEAP + 0x2000, HEAP + 0x3000
    m.write(BASE + 0x2ea2d90, "<Q", players)
    m.write(players + 0xb8, "<4i", 0x10000, -1, -1, -1)
    m.write(BASE + 0x1c544a8, "<Q", globals_)
    m.write(BASE + 0x1c55c60, "<Q", scenario)
    m.write(BASE + 0x1c54318, "<Q", BASE + 0x1b7b880)
    m.write(BASE + 0x1c5432d, "<B", 0)
    m.write(observer, "<3f", 3, -2, 1.5)
    m.write(observer + 0x10, "<h", 4)
    m.write(observer + 0x14, "<3f", .1, -.2, .3)
    m.write(observer + 0x20, "<3f", *forward)
    m.write(observer + 0x2c, "<3f", *up)
    result = []

    def step(rva):
        if rva in (0xaafb4c, 0xb14ea4, 0xb71a8c, 0xabcbd4):
            m.uc.reg_write(UC_X86_REG_RAX, 1 if rva in (0xaafb4c, 0xabcbd4) else 0)
            m.ret()
        elif rva == 0xabcc54:
            require(m.uc.reg_read(UC_X86_REG_RCX) == 0, "Listener zero owns first native packet")
            require(m.read(m.uc.reg_read(UC_X86_REG_RSP), "<Q")[0] == BASE + 0xb4cefe,
                    "Exact native callback caller")
            result.append(bytes(m.uc.mem_read(m.uc.reg_read(UC_X86_REG_RDX), 0x80)))
            m.ret()
        elif rva == 0x1391640:
            m.ret()
        elif not any(lo <= rva < hi for lo, hi in (
                (0xb4cb80, 0xb4cf3d), (0xba24ec, 0xba258e), (0xba31b0, 0xba3290))):
            raise AssertionError(f"Unexpected producer call {rva:x}")

    m.run(0xb4cb80, step)
    require(len(result) == 1, "Exactly one active native listener")
    p = result[0]
    require(struct.unpack_from("<3f", p, 0xc) == forward, "Native packet forward offset")
    require(struct.unpack_from("<3f", p, 0x18) == up, "Native packet up offset")
    require(p[0xc:0x18] == p[0x4c:0x58] and p[0x18:0x24] == p[0x64:0x70],
            "Native flat and matrix orientations agree")
    require(p[0:12] == p[0x70:0x7c], "Native packet and matrix positions agree")
    return p, m.instructions


def native_backend(image, packet):
    m = Machine(image)
    backend, interface, vtable, data = [HEAP + n * 0x1000 for n in (1, 2, 3, 4)]
    m.write(BASE + 0x2e9fdf0, "<Q", backend)
    m.write(BASE + 0x1bea898, "<Q", interface)
    m.write(interface, "<Q", vtable)
    m.uc.mem_write(data, packet)
    result = []

    def vec(address):
        return m.read(address, "<3f")

    def step(rva):
        if rva == 0xabce1b:
            sp = m.uc.reg_read(UC_X86_REG_RSP)
            result.append((vec(m.uc.reg_read(UC_X86_REG_R8)),
                           vec(m.uc.reg_read(UC_X86_REG_R9)),
                           vec(m.read(sp + 0x20, "<Q")[0]),
                           vec(m.read(sp + 0x28, "<Q")[0])))
            # Environment policy after this handoff is outside the pose fixture.
            m.uc.reg_write(UC_X86_REG_RIP, BASE + 0xabd072)
        elif rva == 0x1391640:
            m.ret()
        elif not any(lo <= rva < hi for lo, hi in (
                (0xabcc54, 0xabd0a5), (0x77e70, 0x77fae), (0xf99c0, 0xf9a60),
                (0x70db0, 0x70e4b), (0xfed50, 0xff468))):
            raise AssertionError(f"Unexpected backend call {rva:x}")

    m.uc.reg_write(UC_X86_REG_RCX, 0)
    m.uc.reg_write(UC_X86_REG_RDX, data)
    m.run(0xabcc54, step)
    require(len(result) == 1, "Native backend submits one listener pose")
    return result[0], m.instructions


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", type=Path, default=ROOT / "out/deps/re-tools/inputs/halo1.dll")
    parser.add_argument("--packet-fixture", type=Path,
                        help="Production C++ fixture: original 0x80 bytes then adapted 0x80 bytes")
    args = parser.parse_args()
    raw = args.image.read_bytes()
    require(hashlib.sha256(raw).hexdigest().upper() == PINNED, "Pinned CE image")
    image = pefile.PE(data=raw, fast_load=True).get_memory_mapped_image()
    instructions = 0
    for forward, up in (((1., 0., 0.), (0., 0., 1.)), ((0., 1., 0.), (0., 0., 1.)),
                        ((0., 0., 1.), (-1., 0., 0.)), ((1., 0., 0.), (0., 1., 0.))):
        packet, n = native_packet(image, forward, up)
        pose, count = native_backend(image, packet)
        instructions += n + count
        expected_forward = (forward[0], forward[2], -forward[1])
        expected_up = (-up[0], -up[2], up[1])
        require(all(abs(a-b) < 1e-5 for a, b in zip(pose[2], expected_forward)),
                "Native audio interface forward uses the verified CE-to-backend conversion")
        require(all(abs(a-b) < 1e-5 for a, b in zip(pose[3], expected_up)),
                "Native audio interface up uses its original sign convention")
    if args.packet_fixture:
        data = args.packet_fixture.read_bytes()
        require(len(data) == 0x100, "Production fixture size")
        before, n = native_backend(image, data[:0x80])
        after, count = native_backend(image, data[0x80:])
        instructions += n + count
        require(before[0] == after[0], "Head rotation preserves native listener origin")
        require(all(abs(a-b) < 1e-5 for a, b in zip(before[1], after[1])),
                "Production HMD packet preserves native world velocity/Doppler")
        require(before[2:] != after[2:], "Production HMD packet changes native audio orientation")
    print(f"PASS: native CE listener producer/backend, {instructions} instructions")
    print("LIMIT: interface, player and liquid services are fixtures; no live audio/headset result.")


if __name__ == "__main__":
    main()
