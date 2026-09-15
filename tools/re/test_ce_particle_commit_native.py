"""Execute pinned CE particle-buffer commit instructions in isolated memory.

The real 0x202B10 commit runs against complete original and private CPU buffers.
D3D upload endpoints, the capability query and compiler cookie service are
explicit stubs. Descriptor cloning and selector correction are verifier setup;
production adapter admission/restoration and shader output are tested elsewhere.
No game process, native hook installation or game-file modification is used.
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
from unicorn.x86_const import (UC_X86_REG_RSP, UC_X86_REG_RIP, UC_X86_REG_RCX,
    UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_R9, UC_X86_REG_RAX)

IMAGE_SHA = "0A12DC561780F449D3F4D0DF10BB8D3BC7BE7840A5BEB2B236F672EB6CD42E6C"
BASE, HEAP, STACK, STOP = 0x180000000, 0x60000000, 0x70000000, 0x71000000
COMMIT, ORIGINAL, CONTROL, PRIVATE = BASE+0x202B10, HEAP+0x1000, HEAP+0x2000, HEAP+0x3000
SOURCE, COPIED, GPU = HEAP+0x20000, HEAP+0x40000, HEAP+0x8000
BACKEND, CONTEXT, CONTEXT1, CAPABILITY = HEAP+0x10000, HEAP+0x12000, HEAP+0x13000, HEAP+0x14000
PARTICLE_VECTORS, MAX_VECTORS = 201, 4096


def require(condition, message):
    if not condition:
        raise AssertionError(message)


class NativeCommit:
    def __init__(self, mapped):
        self.uc = Uc(UC_ARCH_X86, UC_MODE_64)
        image_size = (len(mapped)+4095)&~4095
        self.uc.mem_map(BASE, image_size)
        self.uc.mem_write(BASE, mapped)
        self.uc.mem_protect(BASE, image_size, UC_PROT_READ | UC_PROT_EXEC)
        self.uc.mem_map(HEAP, 0x60000)
        self.uc.mem_map(STACK, 0x20000)
        self.uc.mem_map(STOP, 0x1000)
        # Native code may only read the actual shared descriptor and backing.
        # Fixture setup uses Unicorn's host writes, outside emulated execution.
        self.uc.mem_protect(ORIGINAL, 0x1000, UC_PROT_READ)
        self.uc.mem_protect(SOURCE, MAX_VECTORS*16, UC_PROT_READ)
        self.instructions = 0
        self.uploads = []
        self.capability_queries = 0
        self.gpu = bytearray()
        self.has_context1 = False
        self.supports_ranges = False
        self.capacity = 0
        self.expected_backing = 0
        self.write(CONTEXT, "<Q", CONTEXT+0x800)
        self.write(CONTEXT+0x800+0x180, "<Q", STOP+0x100)
        self.write(CONTEXT1, "<Q", CONTEXT1+0x800)
        self.write(CONTEXT1+0x800+0x3A0, "<Q", STOP+0x200)
        self.write(CAPABILITY, "<Q", CAPABILITY+0x800)
        self.write(CAPABILITY+0x800+0x18, "<Q", STOP+0x300)
        self.write(BASE+0x2E3C090, "<Q", CAPABILITY)
        self.write(BACKEND+0xCE0, "<Q", CONTEXT)
        self.uc.hook_add(UC_HOOK_CODE, self.instruction)

    def write(self, at, fmt, *values):
        self.uc.mem_write(at, struct.pack(fmt, *values))

    def read(self, at, fmt):
        return struct.unpack(fmt, self.uc.mem_read(at, struct.calcsize(fmt)))

    def ret(self):
        sp = self.uc.reg_read(UC_X86_REG_RSP)
        self.uc.reg_write(UC_X86_REG_RIP, self.read(sp, "<Q")[0])
        self.uc.reg_write(UC_X86_REG_RSP, sp+8)

    def instruction(self, machine, address, size, _):
        if address == STOP:
            machine.emu_stop()
            return
        if address == BASE+0x1391640:
            self.ret()  # Compiler security-cookie service, not render logic.
            return
        if address == STOP+0x300:
            require(machine.reg_read(UC_X86_REG_RCX) == CAPABILITY, "foreign capability query")
            self.capability_queries += 1
            machine.reg_write(UC_X86_REG_RAX, int(self.supports_ranges))
            self.ret()
            return
        if address in (STOP+0x100, STOP+0x200):
            ranged = address == STOP+0x200
            require(machine.reg_read(UC_X86_REG_RCX) == (CONTEXT1 if ranged else CONTEXT),
                "wrong D3D context")
            require(machine.reg_read(UC_X86_REG_RDX) == GPU, "GPU identity was changed")
            require(machine.reg_read(UC_X86_REG_R8) == 0, "nonzero destination subresource")
            sp = machine.reg_read(UC_X86_REG_RSP)
            source = self.read(sp+0x28, "<Q")[0]
            require(self.read(sp+0x30, "<I")[0] == 0 and self.read(sp+0x38, "<I")[0] == 0,
                "unexpected buffer upload pitches")
            box_pointer = machine.reg_read(UC_X86_REG_R9)
            if ranged:
                require(self.has_context1 and self.supports_ranges, "unavailable ranged path executed")
                box = self.read(box_pointer, "<6I")
                left, top, front, right, bottom, back = box
                require(top == front == 0 and bottom == back == 1 and
                    0 <= left < right <= self.capacity*16, "invalid D3D byte box")
                require(self.read(sp+0x40, "<I")[0] == 1, "native ranged copy flag differs")
                require(source == self.expected_backing+left, "ranged source offset differs")
            else:
                require(box_pointer == 0, "full upload unexpectedly supplies a box")
                left, right = 0, self.capacity*16
                require(source == self.expected_backing, "full upload did not use complete backing")
            payload = bytes(machine.mem_read(source, right-left))
            self.gpu[left:right] = payload
            self.uploads.append(dict(path="UpdateSubresource1" if ranged else "UpdateSubresource",
                first_byte=left, end_byte=right, bytes=len(payload), source=source))
            self.ret()
            return
        require(BASE+0x202B10 <= address < BASE+0x202C25,
            f"unexpected native instruction {address:#x}")
        self.instructions += 1

    def commit(self, descriptor, backing, initial_gpu, flag):
        self.gpu = bytearray(initial_gpu)
        self.uploads = []
        self.capability_queries = 0
        self.expected_backing = backing
        self.write(BACKEND+0xCE8, "<Q", CONTEXT1 if self.has_context1 else 0)
        sp = STACK+0x1FE08  # Windows x64 entry alignment plus caller shadow space.
        self.uc.mem_write(sp, bytes(128))
        self.write(sp, "<Q", STOP)
        for register, value in ((UC_X86_REG_RCX, descriptor), (UC_X86_REG_RDX, BACKEND),
                (UC_X86_REG_R8, int(flag)), (UC_X86_REG_R9, 0)):
            self.uc.reg_write(register, value)
        self.uc.reg_write(UC_X86_REG_RSP, sp)
        self.uc.emu_start(COMMIT, STOP, count=1000)
        require(self.uc.reg_read(UC_X86_REG_RIP) == STOP, "native commit did not return")
        return bytes(self.gpu), list(self.uploads), self.capability_queries

    def case(self, name, capacity, start, path, selectors, clean=False, flag=False):
        require(0 <= start <= capacity <= MAX_VECTORS, "invalid fixture capacity")
        end = 0 if clean else start+PARTICLE_VECTORS
        require(clean or end <= capacity, "fixture dirty allocation exceeds capacity")
        self.capacity = capacity
        self.has_context1 = path != "no-context1"
        self.supports_ranges = path == "ranged"
        values = [((index % 97)-48)*0.125 for index in range(capacity*4)]
        source = bytearray(struct.pack("<"+"f"*len(values), *values))
        changed = []
        if not clean:
            for emitter, value in enumerate(selectors):
                at = start*16 + (21+emitter*20+10)*16
                struct.pack_into("<f", source, at, value)
                if value:
                    changed.append(at)
        source = bytes(source)
        copied = bytearray(source)
        for at in changed:
            struct.pack_into("<f", copied, at, 0)
        # A complete 0x30 descriptor clone; unconsumed fields are distinctive
        # canaries so native dirty-field stores cannot silently clobber them.
        original = bytearray((index*17+3) % 256 for index in range(0x30))
        struct.pack_into("<Qiii", original, 0, BASE+0x17F8C78, start, end, capacity)
        struct.pack_into("<Q", original, 0x18, SOURCE)
        struct.pack_into("<Q", original, 0x28, GPU)
        original = bytes(original)
        private = bytearray(original)
        struct.pack_into("<Q", private, 0x18, COPIED)
        self.uc.mem_write(ORIGINAL, original)
        self.uc.mem_write(CONTROL, original)
        self.uc.mem_write(PRIVATE, bytes(private))
        self.uc.mem_write(SOURCE, source)
        self.uc.mem_write(COPIED, bytes(copied))
        initial_gpu = bytes((index*29+11) % 256 for index in range(capacity*16))
        before_instructions = self.instructions
        stock_gpu, stock_calls, stock_queries = self.commit(CONTROL, SOURCE, initial_gpu, flag)
        shadow_gpu, shadow_calls, shadow_queries = self.commit(PRIVATE, COPIED, initial_gpu, flag)
        require(bytes(self.uc.mem_read(ORIGINAL, 0x30)) == original, f"{name}: shared descriptor changed")
        require(bytes(self.uc.mem_read(SOURCE, len(source))) == source, f"{name}: shared backing changed")
        require(bytes(self.uc.mem_read(COPIED, len(copied))) == bytes(copied), f"{name}: private backing changed")
        expected_private, expected_control = bytearray(private), bytearray(original)
        if not clean:
            for descriptor in (expected_private, expected_control):
                struct.pack_into("<ii", descriptor, 8, capacity, 0)
        require(bytes(self.uc.mem_read(PRIVATE, 0x30)) == bytes(expected_private),
            f"{name}: private descriptor changed beyond native dirty fields")
        require(bytes(self.uc.mem_read(CONTROL, 0x30)) == bytes(expected_control),
            f"{name}: control descriptor dirty transition differs")
        expected_gpu = bytearray(stock_gpu)
        for at in changed:
            struct.pack_into("<f", expected_gpu, at, 0)
        require(shadow_gpu == bytes(expected_gpu), f"{name}: GPU bytes differ outside intended selector lanes")
        require(len(stock_calls) == len(shadow_calls) == (0 if clean else 1), f"{name}: unexpected upload count")
        require(stock_queries == shadow_queries == (0 if clean or not self.has_context1 else 1),
            f"{name}: unexpected native feature query count")
        expected_stock = bytearray(initial_gpu)
        if not clean:
            left, right = (start*16, end*16) if path == "ranged" else (0, capacity*16)
            expected_stock[left:right] = source[left:right]
            require(shadow_calls[0]["first_byte"] == left and shadow_calls[0]["end_byte"] == right,
                f"{name}: native upload range differs")
        require(stock_gpu == bytes(expected_stock), f"{name}: native control upload differs")
        return dict(name=name, capacity_vectors=capacity, dirty_start=start, dirty_end=end,
            context_path=path, immediate_argument=flag, corrected_selector_lanes=len(changed),
            upload_count=len(shadow_calls), uploaded_bytes=sum(call["bytes"] for call in shadow_calls),
            native_instructions=self.instructions-before_instructions,
            original_descriptor_and_backing_unchanged=True, private_dirty_state=[capacity, 0] if not clean else [start, end])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", type=Path, default=ROOT/"out/deps/re-tools/inputs/halo1.dll")
    args = parser.parse_args()
    raw = args.image.read_bytes()
    require(hashlib.sha256(raw).hexdigest().upper() == IMAGE_SHA, "Pinned CE image hash mismatch")
    native = NativeCommit(pefile.PE(data=raw, fast_load=True).get_memory_mapped_image())
    cases = []
    for path in ("no-context1", "range-feature-refused", "ranged"):
        for label, capacity, start, selectors in (
                ("minimum-all-fp", 201, 0, [1]*9),
                ("offset-mixed", 512, 13, [1,0,1,1,0,1,0,1,1]),
                ("maximum-end", 4096, 3895, [1]*9),
                ("maximum-stock", 4096, 1, [0]*9)):
            cases.append(native.case(f"{path}-{label}", capacity, start, path, selectors,
                flag=bool(start % 2)))
        cases.append(native.case(f"{path}-already-clean", 4096, 4096, path, [], clean=True))
    print(json.dumps(dict(status="PASS_PINNED_NATIVE_PARTICLE_COMMIT_ONLY", image_sha256=IMAGE_SHA,
        native_entry_rva="0x202B10", cases=cases, case_count=len(cases),
        native_instructions=native.instructions,
        limits="Descriptor clone and nine selector edits are fixture setup. D3D endpoints copy exact native arguments into a synthetic GPU byte array; capability query and compiler cookie are stubbed. Native commit branches, byte offsets and dirty-field transitions execute pinned instructions. Production admission, real D3D driver behavior, particle shader output and headset rendering are not established by this verifier."), indent=2))


if __name__ == "__main__":
    main()
