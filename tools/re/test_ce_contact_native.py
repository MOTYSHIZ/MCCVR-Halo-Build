"""Execute pinned CE world resolver and explicit-target player melee helper.

Only isolated Unicorn memory is used. Collision queries, tag/object services,
damage submission, marker lookup, effect emission and security-cookie checks
are modeled services. Native selection, event construction, response routing
and world-point resolution execute unchanged. No game process is accessed.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "out/pydeps"))
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64, UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_RAX, UC_X86_REG_RCX,
    UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_R9, UC_X86_REG_RSP,
    UC_X86_REG_RIP, UC_X86_REG_XMM0)

SHA = "0A12DC561780F449D3F4D0DF10BB8D3BC7BE7840A5BEB2B236F672EB6CD42E6C"
BASE, HEAP, STACK, STOP = 0x180000000, 0x60000000, 0x70000000, 0x71000000
OWNER, TARGET, WEAPON = 0x12340007, 0x34560009, 0x56780005


def require(condition, message):
    if not condition:
        raise ValueError(message)


def close(left, right):
    return all(abs(a-b) < 0.00001 for a, b in zip(left, right))


class NativeContact:
    def __init__(self, image, functions):
        self.uc = Uc(UC_ARCH_X86, UC_MODE_64)
        self.uc.mem_map(BASE, (len(image)+4095)&~4095)
        self.uc.mem_write(BASE, image)
        self.uc.mem_map(HEAP, 0x30000)
        self.uc.mem_map(STACK, 0x20000)
        self.uc.mem_map(STOP, 0x1000)
        self.instructions = 0
        self.functions = functions
        self.headers, self.objects = HEAP+0x1000, HEAP+0x3000
        self.unit_tag, self.weapon_tag = HEAP+0xA000, HEAP+0xB000
        self.game, self.tuning = HEAP+0xC000, HEAP+0xD000
        self.owner, self.target, self.weapon = self.objects+0x434, self.objects+0x1434, self.objects+0x2434
        self.write(BASE+0x1C42248, "<Q", self.headers)
        self.write(BASE+0x2D9CDF8, "<Q", self.objects)
        self.write(BASE+0x1C55C60, "<Q", self.game)
        self.write(BASE+0x2EA3410, "<Q", 0)
        self.write(BASE+0x2D9CE10, "<Q", self.tuning)
        self.write(self.headers+0x34, "<I", 0x100)
        for index, offset in ((7, 0x400), (9, 0x1400), (5, 0x2400)):
            self.write(self.headers+0x100+index*12+8, "<I", offset)
        self.write(self.game+0x174, "<I", 0x100)
        self.write(self.tuning+0x134, "<f", 1.0)
        self.write(self.owner, "<I", 10)
        self.write(self.weapon, "<I", 20)
        self.write(self.owner+0x70, "<HH", 0, 1)
        self.write(self.target+0x70, "<HH", 0, 2)
        self.write(self.owner+0x1F8, "<I", 0x11110003)
        self.write(self.owner+0x54, "<HH", 17, 18)
        self.write(self.owner+0x5C, "<3f", 10, 20, 30)
        self.write(self.owner+0x21C, "<3f", 1, 0, 0)
        self.uc.hook_add(UC_HOOK_CODE, self.instruction)

    def write(self, at, fmt, *values):
        self.uc.mem_write(at, struct.pack(fmt, *values))

    def read(self, at, fmt):
        return struct.unpack(fmt, self.uc.mem_read(at, struct.calcsize(fmt)))

    def ret(self, value=None):
        sp = self.uc.reg_read(UC_X86_REG_RSP)
        if value is not None:
            self.uc.reg_write(UC_X86_REG_RAX, value)
        self.uc.reg_write(UC_X86_REG_RIP, self.read(sp, "<Q")[0])
        self.uc.reg_write(UC_X86_REG_RSP, sp+8)

    def instruction(self, machine, address, size, _):
        self.instructions += 1
        if address == STOP:
            machine.emu_stop()
            return
        rva = address-BASE
        cx, dx, r8, r9, sp = [machine.reg_read(reg) for reg in
            (UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_R9, UC_X86_REG_RSP)]
        if rva == 0x1391640:
            self.ret()
        elif self.mode == "resolver" and rva == 0xB913FC:
            require(cx == 0x1000E9 and r9 == OWNER, "Resolver query lost native flags/owner")
            require(close(self.read(dx, "<3f"), (0, 0, 0)) and
                close(self.read(r8, "<3f"), (1, 0, 0)), "Resolver must query desired minus start")
            output = self.read(sp+0x28, "<Q")[0]
            self.write(output+0x10, "<h", -1 if self.collision == "unresolved" else 0)
            self.write(output+0x18, "<3f", .5, 0, 0)
            self.ret(int(self.collision != "clear"))
        elif self.mode == "resolver" and rva == 0xA9BD08:
            value = self.read(cx, "<3f")
            length = math.sqrt(sum(v*v for v in value))
            self.write(cx, "<3f", *(v/length for v in value))
            machine.reg_write(UC_X86_REG_XMM0, struct.unpack("<I", struct.pack("<f", length))[0])
            self.ret()
        elif self.mode == "melee" and rva == 0xA9B648:
            require(cx in (10, 20), f"Unexpected tag lookup {cx:#x}")
            self.ret(self.unit_tag if cx == 10 else self.weapon_tag)
        elif self.mode == "melee" and rva == 0xB389A4:
            require(cx == TARGET and dx == 0xFFFFFFFF, "Explicit-target validity ABI mismatch")
            self.ret(self.target)
        elif self.mode == "melee" and rva == 0xB05CDC:
            require(cx == OWNER, "Held weapon lookup lost owner")
            self.ret(WEAPON if self.armed else 0xFFFFFFFF)
        elif self.mode == "melee" and rva == 0xB9D854:
            self.uc.mem_write(cx, bytes(0x60))
            self.write(cx, "<I", dx)
            self.ret()
        elif self.mode == "melee" and rva == 0xB04C78:
            require(cx == OWNER, "Native marker origin lost owner")
            self.write(dx, "<3f", 11, 22, 33)
            self.ret()
        elif self.mode == "melee" and rva in (0xB220A8, 0xBAD264):
            self.ret(0)
        elif self.mode == "melee" and rva == 0xB0BC7C:
            self.responses.append((cx, dx, r8))
            self.ret()
        elif self.mode == "melee" and rva == 0xB9EA28:
            caller = self.read(sp, "<Q")[0]-BASE
            self.damage.append(dict(caller=hex(caller), target=dx, definition=self.read(cx, "<I")[0],
                attribution=self.read(cx+0x10, "<I")[0], flags=self.read(cx+8, "<I")[0],
                position=self.read(cx+0x20, "<3f"), direction=self.read(cx+0x38, "<3f"),
                material=self.read(cx+0x50, "<H")[0]))
            require((r8 & 0xFFFF) == 0xFFFF and (r9 & 0xFFFF) == 0xFFFF and
                self.read(sp+0x28, "<H")[0] == 0xFFFF and self.read(sp+0x30, "<Q")[0] == 0,
                "Native damage ABI argument slots changed")
            self.ret()
        else:
            # Use the pinned image's actual exception-directory extent. The
            # resolver epilogue continues through its RET at 0xB93DC6.
            begin, end = self.functions[self.mode]
            permitted = begin <= rva < end
            require(permitted, f"Unexpected native execution {rva:#x}")

    def call(self, rva, args):
        sp = STACK+0x1FF08
        self.write(sp, "<Q", STOP)
        self.uc.reg_write(UC_X86_REG_RSP, sp)
        for reg, value in zip((UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_R9), args):
            self.uc.reg_write(reg, value)
        self.uc.emu_start(BASE+rva, STOP, count=20000)
        require(self.uc.reg_read(UC_X86_REG_RIP) == STOP, "Native helper did not return")

    def resolver(self, collision):
        self.mode, self.collision = "resolver", collision
        self.write(HEAP+0x100, "<3f", 0, 0, 0)
        self.write(HEAP+0x120, "<3f", 1, 0, 0)
        self.write(HEAP+0x140, "<3f", 7, 8, 9)
        self.call(0xB93C8C, (HEAP+0x100, HEAP+0x120, HEAP+0x140, OWNER))
        output, valid = self.read(HEAP+0x140, "<3f"), self.uc.reg_read(UC_X86_REG_RAX)&0xFF
        expected = {"clear": ((1, 0, 0), 1), "hit": ((.49, 0, 0), 1), "unresolved": ((7, 8, 9), 0)}[collision]
        require(close(output, expected[0]) and valid == expected[1], f"Native resolver mismatch {collision,output,valid}")
        return dict(collision=collision, output=output, valid=valid)

    def melee(self, armed, fallback, clang, target_type=0, material=12):
        self.mode, self.armed, self.responses, self.damage = "melee", armed, [], []
        self.write(self.unit_tag+0x294, "<I", 100 if fallback else 0xFFFFFFFF)
        self.write(self.weapon_tag+0x3A0, "<I", 110 if armed else 0xFFFFFFFF)
        self.write(self.weapon_tag+0x3B0, "<I", 111 if clang else 0xFFFFFFFF)
        self.write(self.target+0x70, "<H", target_type)
        self.write(self.owner+0x269, "<B", 9)
        self.call(0xB0C388, (OWNER, TARGET, material))
        expected = 110 if armed else 100 if fallback else 0xFFFFFFFF
        primary = [d for d in self.damage if d["target"] == TARGET]
        require(len(primary) == int(expected != 0xFFFFFFFF and target_type == 0), "Native target-type/damage selection changed")
        if primary:
            require(primary[0]["caller"] == "0xb0c8a8" and primary[0]["definition"] == expected and
                primary[0]["attribution"] == OWNER and primary[0]["material"] == material and
                close(primary[0]["position"], (11, 22, 33)) and close(primary[0]["direction"], (1, 0, 0)),
                "Physical correction callsite/event fields do not match native constructed event")
        secondary = [d for d in self.damage if d["target"] == OWNER]
        require(len(secondary) == int(armed and clang and material != 0xFFFF), "Native weapon response branch changed")
        if secondary:
            require(secondary[0]["caller"] == "0xb0c955" and secondary[0]["definition"] == 111 and
                close(secondary[0]["direction"], (-1, 0, 0)), "Native owner response is distinct from target impact")
        require(self.read(self.owner+0x269, "<B")[0] == 0, "Native melee side-effect no longer clears +0x269")
        return dict(armed=armed, fallback=fallback, clang=clang, target_type=target_type,
            material=material, damage=self.damage, responses=self.responses)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", type=Path, default=ROOT/"out/deps/re-tools/inputs/halo1.dll")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    raw = args.image.read_bytes()
    require(hashlib.sha256(raw).hexdigest().upper() == SHA, "Pinned image mismatch")
    pe = pefile.PE(data=raw)
    functions = {}
    for name, begin in (("resolver", 0xB93C8C), ("melee", 0xB0C388)):
        entries = [entry.struct for entry in pe.DIRECTORY_ENTRY_EXCEPTION
            if entry.struct.BeginAddress == begin]
        require(len(entries) == 1, f"Native {name} function extent is not unique")
        functions[name] = (begin, entries[0].EndAddress)
    native = NativeContact(pe.get_memory_mapped_image(), functions)
    resolver = [native.resolver(kind) for kind in ("clear", "hit", "unresolved")]
    melee = [native.melee(armed, fallback, clang) for armed in (False, True)
        for fallback in (False, True) for clang in (False, True)]
    melee += [native.melee(True, True, True, target_type=2),
        native.melee(True, True, True, material=0xFFFF)]
    report = dict(status="PASS_NATIVE_CONTACT_CONTRACTS_ONLY", image_sha256=SHA,
        function_extents={name: [hex(value) for value in extent] for name, extent in functions.items()},
        resolver_cases=len(resolver), melee_cases=len(melee), instructions=native.instructions,
        resolver=resolver, melee=melee,
        limit="Collision world, native damage execution, network authority, actual mesh surfaces and headset behavior remain unexecuted.")
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2)+"\n", encoding="utf-8")
    print(json.dumps({k:v for k,v in report.items() if k not in ("resolver", "melee")}, indent=2))


if __name__ == "__main__":
    main()
