"""Execute pinned Anniversary authored-visibility producer/application code.

Only isolated Unicorn memory is used. The native named-mask producer is entered
after its string-map lookup (0x53C49B) and stopped before unrelated name/resource
work (0x53C4DF). The complete 0x2E2160 name lookup and visibility application
executes, including modeled CRT memcmp for equal-content/different-name objects.
No game process, graphics device, DLL entry point or installation is touched.
This identifies native player-mask semantics; it is not a headset result.
"""
import argparse
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
    UC_X86_REG_R8, UC_X86_REG_R9, UC_X86_REG_R11, UC_X86_REG_R12,
    UC_X86_REG_R15, UC_X86_REG_RBP, UC_X86_REG_RSP, UC_X86_REG_RIP)

SHA = "0A12DC561780F449D3F4D0DF10BB8D3BC7BE7840A5BEB2B236F672EB6CD42E6C"
BASE, HEAP, STACK, STOP = 0x180000000, 0x60000000, 0x70000000, 0x71000000


def require(condition, message):
    if not condition:
        raise ValueError(message)


class NativeVisibility:
    def __init__(self, image):
        self.uc = Uc(UC_ARCH_X86, UC_MODE_64)
        self.size = (len(image) + 4095) & ~4095
        self.uc.mem_map(BASE, self.size)
        self.uc.mem_write(BASE, image)
        self.uc.mem_map(HEAP, 0x20000)
        self.uc.mem_map(STACK, 0x20000)
        self.uc.mem_map(STOP, 0x1000)
        self.renderer, self.owner, self.pending = HEAP+0x1000, HEAP+0x2000, HEAP+0x3000
        self.domains, self.pointers = HEAP+0x4000, HEAP+0x5000
        self.domain, self.name, self.name_ref = HEAP+0x6000, HEAP+0x7000, HEAP+0x8000
        self.write(BASE+0x1BEA9E0, "<Q", self.renderer)
        self.write(BASE+0x1BEA9D0, "<Q", self.domains)
        self.uc.mem_protect(BASE, self.size, UC_PROT_READ | UC_PROT_EXEC)
        self.write(self.owner+0x238, "<Q", self.pending)
        self.write(self.domains+0x18, "<Q", self.pointers)
        self.write(self.domains+0x20, "<I", 1)
        self.write(self.pointers, "<Q", self.domain)
        self.write(self.domain+8, "<Q", self.domain+0x100)
        self.write(self.domain+0x140, "<Q", self.name)
        self.write(self.name_ref, "<Q", self.name)
        self.uc.mem_write(self.name, struct.pack("<III", 10, 9, 9)+b"world|bsp\0")
        self.uc.hook_add(UC_HOOK_CODE, self.instruction)
        self.mode = ""
        self.instructions = 0
        self.memcmp_calls = 0

    def write(self, address, fmt, *values):
        self.uc.mem_write(address, struct.pack(fmt, *values))

    def read(self, address, fmt):
        return struct.unpack(fmt, self.uc.mem_read(address, struct.calcsize(fmt)))

    def instruction(self, machine, address, size, _):
        self.instructions += 1
        if address == STOP or (self.mode == "producer" and address == BASE+0x53C4DF):
            machine.emu_stop()
            return
        if address == BASE+0x1606FBD:
            a, b, count = [machine.reg_read(r) for r in (UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8)]
            if count > 128:
                raise AssertionError("Unbounded CRT comparison")
            left, right = bytes(machine.mem_read(a, count)), bytes(machine.mem_read(b, count))
            machine.reg_write(UC_X86_REG_RAX, int(left > right)-int(left < right) & 0xFFFFFFFF)
            sp = machine.reg_read(UC_X86_REG_RSP)
            machine.reg_write(UC_X86_REG_RIP, self.read(sp, "<Q")[0])
            machine.reg_write(UC_X86_REG_RSP, sp+8)
            self.memcmp_calls += 1
            return
        rva = address-BASE
        permitted = 0x53C49B <= rva < 0x53C4DF if self.mode == "producer" else 0x2E2160 <= rva < 0x2E224F
        if not permitted:
            raise AssertionError(f"Unexpected native execution {rva:#x}")

    def produce(self, split, requested, visible, touched=0, values=0):
        self.mode = "producer"
        self.write(self.renderer+0xB0, "<I", int(split))
        self.write(self.pending+8, "<II", touched, values)
        for reg, value in ((UC_X86_REG_RBP, self.owner), (UC_X86_REG_R11, 0),
                (UC_X86_REG_R12, int(visible)), (UC_X86_REG_R15, requested)):
            self.uc.reg_write(reg, value)
        self.uc.emu_start(BASE+0x53C49B, STOP, count=100)
        require(self.uc.reg_read(UC_X86_REG_RIP) == BASE+0x53C4DF,
            "Native producer did not reach its bounded endpoint")
        return self.read(self.pending+8, "<II")

    def apply(self, mask, visible, previous, name_case="identity"):
        self.mode = "apply"
        self.write(self.domain+4, "<I", previous)
        self.write(self.name_ref, "<Q", self.name if name_case == "identity" else self.name+0x100)
        self.uc.mem_write(self.name+0x100, bytes(self.uc.mem_read(self.name, 32)))
        if name_case == "missing":
            self.uc.mem_write(self.name+0x10C, b"other|bsp")
        sp = STACK+0x1FF08
        self.write(sp, "<Q", STOP)
        for reg, value in ((UC_X86_REG_RSP, sp), (UC_X86_REG_RCX, 0xDEADBEEF),
                (UC_X86_REG_RDX, self.name_ref), (UC_X86_REG_R8, int(visible)),
                (UC_X86_REG_R9, mask)):
            self.uc.reg_write(reg, value)
        self.uc.emu_start(BASE+0x2E2160, STOP, count=1000)
        require(self.uc.reg_read(UC_X86_REG_RIP) == STOP,
            "Native visibility application did not return")
        return self.read(self.domain+4, "<I")[0], self.uc.reg_read(UC_X86_REG_RAX) & 0xFF


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", type=Path, default=ROOT/"out/deps/re-tools/inputs/halo1.dll")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    raw = args.image.read_bytes()
    require(hashlib.sha256(raw).hexdigest().upper() == SHA, "Pinned image mismatch")
    native = NativeVisibility(pefile.PE(data=raw, fast_load=True).get_memory_mapped_image())
    producers, applications = [], []
    for split in (False, True):
        for requested in (1, 2, 3):
            for visible in (False, True):
                for touched, values in ((0, 0), (0, 3), (1, 1), (2, 2)):
                    actual = native.produce(split, requested, visible, touched, values)
                    updated = touched | (requested if split else 3)
                    expected = (updated, values | updated if visible else values & ~updated)
                    require(actual == expected,
                        f"Producer mismatch: {split, requested, visible, actual, expected}")
                    producers.append(dict(split=split, requested=requested, visible=visible,
                        previous_touched=touched, previous_values=values, result=actual))
    for mask in (1, 2, 3):
        for visible in (False, True):
            for previous in (0, 1, 2, 3, 0x100, 0xA503):
                for name_case in ("identity", "equal", "missing"):
                    actual, found = native.apply(mask, visible, previous, name_case)
                    expected = previous | mask if visible else previous & ~(mask | 0x100)
                    if name_case == "missing": expected = previous
                    require(actual == expected and found == (name_case != "missing"),
                        f"Application mismatch: {mask, visible, previous, name_case, actual}")
                    applications.append(dict(mask=mask, visible=visible, previous=previous,
                        name_case=name_case, result=actual, found=found))
    mono = native.produce(False, 1, True)
    synthetic_split = native.produce(True, 1, True)
    explicit_both = native.produce(True, 3, True)
    require(mono == explicit_both == (3, 3) and synthetic_split == (1, 1),
        "Named visibility source-player semantics differed")
    report = dict(status="PASS_NATIVE_VISIBILITY_SEMANTICS_ONLY", image_sha256=SHA,
        producer_cases=len(producers), application_cases=len(applications),
        instructions=native.instructions, modeled_memcmp_calls=native.memcmp_calls,
        source_player_zero_visible=dict(mono=mono, synthetic_split=synthetic_split,
            explicit_both_views=explicit_both), producers=producers, applications=applications,
        limit="Native named visibility semantics; independent geometric culling, live producer chronology and headset output are not executed.")
    output = json.dumps(report, indent=2)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(output+"\n", encoding="utf-8")
    print(json.dumps({k:v for k,v in report.items() if k not in ("producers", "applications")}, indent=2))


if __name__ == "__main__":
    main()
