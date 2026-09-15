"""Execute pinned CE pool/entry/root/split allocation instructions in Unicorn.

Runs complete native pool initialization, entry registration, root initialization,
child construction and split rebuild functions. Texture allocation, native string
storage, driver creation and auxiliary lookup assets are modeled. The simulated
detour changes are reported separately from executed native instructions. No game
process, executable entry point, installation or graphics device is used.
"""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "out/pydeps"))
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64, UC_HOOK_CODE, UC_PROT_READ, UC_PROT_EXEC
from unicorn.x86_const import (UC_X86_REG_RAX, UC_X86_REG_RCX, UC_X86_REG_RDX,
    UC_X86_REG_R8, UC_X86_REG_R9, UC_X86_REG_RSP, UC_X86_REG_RIP)

SHA = "0A12DC561780F449D3F4D0DF10BB8D3BC7BE7840A5BEB2B236F672EB6CD42E6C"
BASE, HEAP, STACK, STOP = 0x180000000, 0x60000000, 0x70000000, 0x71000000
CREATE, COMPARE, BACKING, WIDTH, HEIGHT, SIZE = [STOP + x * 0x100 for x in range(1, 7)]
FUNCTIONS = dict(pool=0x2D44B0, entry=0x20A9B0, root=0x1F3E90,
                 child=0x1F9D20, rebuild=0x1F9BD0)


def require(value, message):
    if not value:
        raise RuntimeError(message)


class NativeAllocation:
    def __init__(self, pe, width, height, full, pending=True, backbuffer=True):
        self.uc = Uc(UC_ARCH_X86, UC_MODE_64)
        image = pe.get_memory_mapped_image()
        image_size = (len(image) + 4095) & ~4095
        self.uc.mem_map(BASE, image_size)
        self.uc.mem_write(BASE, image)
        self.uc.mem_map(HEAP, 0x100000)
        self.uc.mem_map(STACK, 0x20000)
        self.uc.mem_map(STOP, 0x1000)
        self.pool, self.backend, self.config = HEAP, HEAP + 0x4000, HEAP + 0x5000
        self.vtable, self.string = HEAP + 0x6000, HEAP + 0x7000
        self.width, self.height, self.full, self.pending = width, height, full, pending
        self.calls, self.instructions, self.allocations = Counter(), 0, []
        self.entries, self.children, self.creations = [], [], []
        self.ranges = {name: (start, next(e.struct.EndAddress for e in pe.DIRECTORY_ENTRY_EXCEPTION
            if e.struct.BeginAddress == start)) for name, start in FUNCTIONS.items()}
        # Pool initialization has three contiguous chained unwind entries.
        self.ranges["pool"] = (FUNCTIONS["pool"], 0x2D4E44)
        self.write(BASE + 0x2E3BDD8, "Q", self.backend)
        self.write(self.backend + 0x118, "Q", self.config)
        self.write(self.backend + 0x226, "B", int(backbuffer))
        self.write(self.config, "I", 0x200)
        self.write(self.config + 0x10, "2I", width, height)
        self.write(self.config + 0x20, "4I", width, height, width, height)
        self.write(self.string, "2I", 100000, 1)
        self.uc.mem_write(self.string + 0xc, b"fixture\0")
        for offset, address in ((0x18, WIDTH), (0x20, HEIGHT), (0x90, SIZE),
                                (0x98, COMPARE), (0xa0, BACKING),
                                (0xa8, BASE + FUNCTIONS["root"]), (0xc0, CREATE)):
            self.write(self.vtable + offset, "Q", address)
        self.uc.mem_protect(BASE, image_size, UC_PROT_READ | UC_PROT_EXEC)
        self.uc.hook_add(UC_HOOK_CODE, self.instruction)

    def write(self, address, fmt, *values):
        self.uc.mem_write(address, struct.pack("<" + fmt, *values))

    def read(self, address, fmt):
        return struct.unpack("<" + fmt, self.uc.mem_read(address, struct.calcsize("<" + fmt)))

    def ret(self, value=0):
        sp = self.uc.reg_read(UC_X86_REG_RSP)
        self.uc.reg_write(UC_X86_REG_RAX, value)
        self.uc.reg_write(UC_X86_REG_RIP, self.read(sp, "Q")[0])
        self.uc.reg_write(UC_X86_REG_RSP, sp + 8)

    def instruction(self, machine, address, size, _):
        self.instructions += 1
        if address == STOP:
            machine.emu_stop()
            return
        rcx, rdx, r8, r9, sp = [machine.reg_read(register) for register in
            (UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_R9, UC_X86_REG_RSP)]
        if address in (CREATE, COMPARE, BACKING, WIDTH, HEIGHT, SIZE):
            if address == CREATE:
                self.creations.append((rcx, *self.read(rcx + 0x10, "2h")))
                self.ret(1)
            elif address in (WIDTH, HEIGHT):
                self.ret(self.read(rcx + (0x10 if address == WIDTH else 0x12), "h")[0])
            else:
                self.ret(0)
            return
        rva = address - BASE
        if rva == 0x492C0:
            self.write(rcx, "Q", self.string)
            self.ret(rcx)
            return
        if rva == 0x4FFA0:
            self.ret(rcx)
            return
        if rva == 0x1F68C0:
            texture = HEAP + 0x10000 + len(self.allocations) * 0x200
            self.allocations.append(texture)
            self.write(texture, "2Q", self.vtable, self.string)
            self.write(texture + 0x88, "I", r8 & 0xffffffff)
            self.ret(texture)
            return
        if rva == 0x3009B0:
            _, width, height = self.read(rcx, "3I")
            self.ret(width * height * 4)
            return
        if rva == 0x2D4F50:
            self.ret(1)  # Auxiliary lookup assets are outside this allocation contract.
            return
        if rva == FUNCTIONS["entry"]:
            index = self.read(rcx + 0xc, "I")[0]
            height, fmt, usage = [self.read(sp + offset, "I")[0] for offset in (0x28, 0x30, 0x38)]
            name = bytes(self.uc.mem_read(rdx, 80)).split(b"\0")[0].decode("ascii")
            entry = dict(index=index, name=name, usage=usage, flags=r8, format=fmt,
                         requested_width=r9, requested_height=height)
            self.entries.append(entry)
            if self.full and not r9 and not height and usage in (0x04000001, 0x24000001):
                machine.reg_write(UC_X86_REG_R9, self.width)
                self.write(sp + 0x28, "I", 2 * self.height)
                self.calls["modeled_packed_argument_change"] += 1
        if rva == FUNCTIONS["child"]:
            count = self.read(self.pool + 0xc, "I")[0]
            current = self.read(self.pool + 0x10 + count * 0x38, "Q")[0]
            known = next((entry for entry in self.entries
                if self.read(self.pool + 0x10 + entry["index"] * 0x38, "Q")[0] == rcx), None)
            require(known is not None, "Child escaped the native pool")
            parent_width, parent_height = self.read(rcx + 0x10, "2h")
            published = known["index"] < count
            pending = known["index"] == count and current == rcx
            require(published or pending, "Native child root is not current or published")
            require(r8 == parent_width and r9 == parent_height // 2,
                    "Native split arguments are not parent width / half parent height")
            if pending:
                require(self.read(self.pool + 0x18 + count * 0x38, "I")[0] == 0,
                        "Native usage unexpectedly published before children")
                self.calls["child_before_usage_and_count"] += 1
            if self.full and (published or self.pending) and known["usage"] & 7 and not known["usage"] & 0x4000000:
                machine.reg_write(UC_X86_REG_R9, parent_height)
                self.calls["modeled_child_argument_change"] += 1
            self.children.append(dict(root=rcx, entry=known["index"], original_height=r9,
                actual_height=machine.reg_read(UC_X86_REG_R9), published=published))
        for name, (start, end) in self.ranges.items():
            if start <= rva < end:
                if start == rva:
                    self.calls[name] += 1
                return
        raise RuntimeError(f"Unexpected native instruction {rva:#x}")

    def call(self, rva, *arguments):
        sp = STACK + 0x1FF08
        self.write(sp, "Q", STOP)
        self.uc.reg_write(UC_X86_REG_RSP, sp)
        for register, value in zip((UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_R9), arguments):
            self.uc.reg_write(register, value)
        self.uc.emu_start(BASE + rva, STOP, count=400000)
        require(self.uc.reg_read(UC_X86_REG_RIP) == STOP, "Native function exceeded instruction budget")
        return self.uc.reg_read(UC_X86_REG_RAX)

    def verify(self):
        require(self.call(FUNCTIONS["pool"], self.pool) == 1, "Native pool did not initialize")
        require(self.read(self.pool + 0xc, "I")[0] == len(self.entries), "Native entry count mismatch")
        require(self.calls["child_before_usage_and_count"] > 30, "Native allocation order was not exercised")
        for entry in self.entries:
            root = self.read(self.pool + 0x10 + entry["index"] * 0x38, "Q")[0]
            width, height = self.read(root + 0x10, "2h")
            usage = entry["usage"]
            require(self.read(self.pool + 0x18 + entry["index"] * 0x38, "I")[0] == usage,
                    "Allocation changed native pool lookup role")
            if usage in (0x04000001, 0x24000001):
                require((width, height) == (self.width, self.height * (2 if self.full else 1)),
                        "Packed output is not exact eye-pair size")
                require(self.read(root + 0xa8, "2Q") == (0, 0), "Packed output unexpectedly split")
            if entry["flags"] & 0x10:
                left, right = self.read(root + 0xa8, "2Q")
                require(left and right and left != right, "Native eye allocations alias or are missing")
                corrected = self.full and self.pending and usage & 7 and not usage & 0x4000000
                expected = (width, height if corrected else height // 2)
                require(self.read(left + 0x10, "2h") == expected and self.read(right + 0x10, "2h") == expected,
                        "Native child dimensions disagree with allocation policy")
            entry["root_size"] = [width, height]
        before = len(self.children)
        first = self.read(self.pool + 0x10, "Q")[0]
        self.call(FUNCTIONS["rebuild"], first)
        require(len(self.children) == before + 2 and all(c["published"] for c in self.children[-2:]),
                "Native split recreation did not use published pool membership")
        return dict(width=self.width, height=self.height, full=self.full, pending_receipt=self.pending,
                    instructions=self.instructions, calls=dict(self.calls), entries=self.entries)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", type=Path, default=ROOT / "out/deps/re-tools/inputs/halo1.dll")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    raw = args.image.read_bytes()
    require(hashlib.sha256(raw).hexdigest().upper() == SHA, "Pinned image SHA-256 mismatch")
    pe = pefile.PE(data=raw)
    cases = [NativeAllocation(pe, width, height, full, pending, backbuffer).verify()
        for width, height in ((32, 17), (1920, 1080), (2640, 2368))
        for full, pending in ((False, True), (True, False), (True, True))
        for backbuffer in (False, True)]
    report = dict(status="PASS_NATIVE_RESOLUTION_ALLOCATION", image_sha256=SHA, case_count=len(cases),
                  instructions=sum(case["instructions"] for case in cases), cases=cases,
                  modeled_dependencies=["texture wrapper allocation", "string references", "driver creation",
                      "auxiliary lookup assets", "compatibility/backing queries", "detour argument adjustments"],
                  limit="Executes native allocation order and dimensions. Driver resources/GPU pixels and production detours are covered separately by C++ WARP tests; headset behavior remains unaccepted.")
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({key: value for key, value in report.items() if key != "cases"}, indent=2))


if __name__ == "__main__":
    main()
