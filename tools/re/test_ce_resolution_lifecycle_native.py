"""Reproduce CE native full-renderer release losing classic HUD effects.

Read-only PE emulation, without process access or game writes. Executes the
pinned release entry, backend vtable dispatch, 138-slot effect disposal and
backend reinitializer's actual readiness gate. Driver/allocator services are
stubs; this is not a D3D or headset acceptance test.
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
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64, UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_RSP, UC_X86_REG_RIP,
    UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_R9,
    UC_X86_REG_RAX)

SHA = "0A12DC561780F449D3F4D0DF10BB8D3BC7BE7840A5BEB2B236F672EB6CD42E6C"
BASE, HEAP, STACK, STOP = 0x180000000, 0x60000000, 0x70000000, 0x71000000


def run(image, imports, release, restore_intent=False, initially_ready=True,
        reload_fail=False):
    uc = Uc(UC_ARCH_X86, UC_MODE_64)
    uc.mem_map(BASE, (len(image) + 4095) & ~4095)
    uc.mem_write(BASE, image)
    uc.mem_map(HEAP, 0x200000)
    uc.mem_map(STACK, 0x20000)
    uc.mem_map(STOP, 0x1000)
    backend, owner = HEAP + 0x1000, HEAP + 0x2000
    effects = [HEAP + 0x10000 + i * 0x100 for i in range(138)]
    entries, freed = [], []
    refresh_entered = False
    instructions = 0
    allocated = HEAP + 0x40000
    shader_creates = 0
    loader_result = None
    # One technique with one four-byte shader per effect, followed by the
    # three shader blobs the native container loader consumes after its table.
    # Native parsing, record creation and publication remain unmodified.
    effect_blob = struct.pack("<HHII4s", 1, 0, 1, 4, b"TEST")
    shader_file = b"SH02" + (struct.pack("<I", len(effect_blob)) + effect_blob) * 138
    shader_file += (struct.pack("<I", 4) + b"TEST") * 3

    def write(at, fmt, *values):
        uc.mem_write(at, struct.pack(fmt, *values))

    def read(at, fmt):
        return struct.unpack(fmt, uc.mem_read(at, struct.calcsize(fmt)))

    def ret(value=None):
        if value is not None:
            uc.reg_write(UC_X86_REG_RAX, value)
        sp = uc.reg_read(UC_X86_REG_RSP)
        uc.reg_write(UC_X86_REG_RIP, read(sp, "<Q")[0])
        uc.reg_write(UC_X86_REG_RSP, sp + 8)

    # Boundaries not involved in the readiness gate or effect-table lifetime.
    services = {0x4d0780, 0x1fd330, 0xb515ec, 0xc4b228,
                0x1f0250, 0x2f5840, 0x20a090, 0x1391640, 0x1606e60}

    def instruction(machine, address, size, _):
        nonlocal refresh_entered, instructions, allocated, shader_creates
        instructions += 1
        rva = address - BASE
        if address == STOP:
            machine.emu_stop()
        elif rva in (0x4f1ad0, 0x82170, 0x80dd0):
            entries.append(hex(rva))
        elif rva in services:
            ret()
        elif rva == 0xd94a0:
            freed.append(machine.reg_read(UC_X86_REG_RCX))
            ret()
        elif rva == 0xd9500:
            size = machine.reg_read(UC_X86_REG_RCX)
            value = allocated
            allocated += (size + 15) & ~15
            assert allocated < HEAP + 0x180000
            ret(value)
        elif rva == 0x48980:
            machine.mem_write(machine.reg_read(UC_X86_REG_RCX), b"fixture\0")
            ret(7)
        elif rva == 0x139362f:
            target = machine.reg_read(UC_X86_REG_RCX)
            value = machine.reg_read(UC_X86_REG_RDX) & 255
            count = machine.reg_read(UC_X86_REG_R8)
            machine.mem_write(target, bytes([value]) * count)
            ret(target)
        elif rva == 0x1ef660:
            # Backend driver initialization succeeds. It does not create CE
            # classic shader objects, which the enclosing native gate owns.
            ret(1)
        elif rva == 0x80e49:
            refresh_entered = True
            machine.emu_stop()
        elif address == STOP + 0x100:  # CreateFileA
            ret(0x1234)
        elif address == STOP + 0x110:  # GetFileSizeEx
            write(machine.reg_read(UC_X86_REG_RDX), "<Q", len(shader_file))
            ret(1)
        elif address == STOP + 0x120:  # ReadFile
            assert machine.reg_read(UC_X86_REG_R8) == len(shader_file)
            machine.mem_write(machine.reg_read(UC_X86_REG_RDX), shader_file)
            write(machine.reg_read(UC_X86_REG_R9), "<I", len(shader_file))
            ret(1)
        elif address == STOP + 0x130:  # CloseHandle
            ret(1)
        elif address == STOP + 0x200:  # ID3D11Device::CreatePixelShader
            shader_creates += 1
            if reload_fail and shader_creates == 3:
                ret(0x80004005)
            else:
                out = read(machine.reg_read(UC_X86_REG_RSP) + 0x28, "<Q")[0]
                write(out, "<Q", HEAP + 0x180000 + shader_creates * 16)
                write(HEAP + 0x180000 + shader_creates * 16, "<Q", HEAP + 0x3000)
                ret(0)
        elif address == STOP + 0x210:  # IUnknown::Release
            ret(0)

    uc.hook_add(UC_HOOK_CODE, instruction)
    write(BASE + 0x2e3bdd8, "<Q", backend)
    write(BASE + 0x2e3c090, "<Q", backend)
    write(backend, "<Q", BASE + 0x17f0c98)
    assert read(BASE + 0x17f0c98 + 0x60, "<Q")[0] == BASE + 0x80dd0
    assert read(BASE + 0x17f0c98 + 0x70, "<Q")[0] == BASE + 0x82170
    write(BASE + 0x1c33e30, "<Q", owner)
    for rva in (0x2e3cd78, 0x1bea6b8, 0x1bea8a0, 0x1bea9e0,
                0x2d62880, 0x2e3b960, 0x2e2c4f8, 0x2a7a568,
                0x2ea3238):
        write(BASE + rva, "<Q", 0)
    write(BASE + 0x2d698bc, "<I", 0)
    write(BASE + 0x2ea2d5c, "<I", int(initially_ready))
    write(BASE + 0x2ea3230, "<I", 0)
    for rva, length in ((0x2de1190, 0x120), (0x1b87a90, 0x410),
                        (0x1b8e550, 0x80), (0x2de19a0, 0x2000)):
        uc.mem_write(BASE + rva, bytes(length))
    for i, effect in enumerate(effects):
        write(BASE + 0x1b7c290 + i * 0x20, "<Q", effect if initially_ready else 0)
    for name, offset in (("CreateFileA", 0x100), ("GetFileSizeEx", 0x110),
                         ("ReadFile", 0x120), ("CloseHandle", 0x130)):
        write(imports[name], "<Q", STOP + offset)
    write(BASE + 0x2ea2d20, "<Q", HEAP + 0x4000)
    write(HEAP + 0x4000, "<Q", HEAP + 0x3000)
    write(HEAP + 0x3000 + 0x78, "<Q", STOP + 0x200)
    write(HEAP + 0x3000 + 0x10, "<Q", STOP + 0x210)

    def invoke(rva):
        sp = STACK + 0x1ff08
        write(sp, "<Q", STOP)
        uc.reg_write(UC_X86_REG_RSP, sp)
        uc.reg_write(UC_X86_REG_RCX, backend)
        try:
            uc.emu_start(BASE + rva, STOP, count=100000)
        except Exception as error:
            raise AssertionError(f"Native fixture stopped at {uc.reg_read(UC_X86_REG_RIP):#x}; entries={entries}; freed={len(freed)}") from error

    if release:
        invoke(0x4f1ad0)
        assert uc.reg_read(UC_X86_REG_RIP) == STOP
        assert entries == ["0x4f1ad0", "0x82170"], entries
        assert freed == (effects if initially_ready else []), freed
        assert read(BASE + 0x2ea2d5c, "<I")[0] == 0
        assert all(read(BASE + 0x1b7c290 + i * 0x20, "<Q")[0] == 0
                   for i in range(138))
    freed_before_reload = len(freed)
    if restore_intent and initially_ready:
        write(BASE + 0x2ea2d5c, "<I", 1)
    # Keep the unrelated helper object valid so the initializer's skip path
    # returns naturally without introducing an allocation into this fixture.
    write(BASE + 0x2e3b960, "<Q", HEAP + 0x5000)
    invoke(0x80dd0)
    if (release and not restore_intent) or not initially_ready:
        assert not refresh_entered
        assert uc.reg_read(UC_X86_REG_RIP) == STOP
        assert uc.reg_read(UC_X86_REG_RAX) == 1
        assert read(BASE + 0x2ea2d5c, "<I")[0] == 0
    else:
        assert refresh_entered
        if release:
            # The positive backend gate is proven above. Exercise its actual
            # shader loader independently: the intervening terrain/texture
            # initialization is outside this narrowly scoped fixture.
            invoke(0xb11af8)
            assert uc.reg_read(UC_X86_REG_RIP) == STOP
            loader_result = bool(uc.reg_read(UC_X86_REG_RAX) & 255)
            assert loader_result == (not reload_fail)
            pointers = [read(BASE + 0x1b7c290 + i * 0x20, "<Q")[0]
                        for i in range(138)]
            assert all(bool(value) == (not reload_fail) for value in pointers)
            if not reload_fail:
                for pointer in pointers:
                    assert read(pointer + 8, "<I")[0] == 1
                    technique = read(pointer, "<Q")[0]
                    assert read(technique + 0x48, "<I")[0] == 1
                    assert read(technique + 0x10, "<Q")[0]
                assert shader_creates == 141
        else:
            assert not freed
            assert all(read(BASE + 0x1b7c290 + i * 0x20, "<Q")[0] == value
                       for i, value in enumerate(effects))
    return dict(full_release=release, native_entries=entries,
                restore_previous_intent=restore_intent,
                initially_ready=initially_ready,
                native_effects_freed=freed_before_reload,
                classic_shader_refresh_entered=refresh_entered,
                backend_init_reports_success=(True if not refresh_entered else None),
                separate_native_loader_result=loader_result,
                shader_creation_services=shader_creates,
                injected_shader_creation_failure=reload_fail,
                instructions=instructions)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", type=Path,
                        default=ROOT / "out/deps/re-tools/inputs/halo1.dll")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    raw = args.image.read_bytes()
    if hashlib.sha256(raw).hexdigest().upper() != SHA:
        raise SystemExit("Pinned CE image hash mismatch")
    pe = pefile.PE(data=raw)
    image = pe.get_memory_mapped_image()
    imports = {entry.name.decode(): entry.address
               for dll in pe.DIRECTORY_ENTRY_IMPORT for entry in dll.imports
               if entry.name}
    report = dict(status="PASS_NATIVE_EMULATION_ONLY", image_sha256=SHA,
                  cases=[run(image, imports, False), run(image, imports, True),
                         run(image, imports, True, restore_intent=True),
                         run(image, imports, True, restore_intent=True, initially_ready=False),
                         run(image, imports, True, restore_intent=True, reload_fail=True)],
                  limit="Native release, vtable dispatch, effects disposal and initializer gate execute pinned code. Positive gate stops at entry; B11AF8/B10484/B10330 shader loading then executes separately against synthetic SH02 data. Driver, allocator, file and unrelated helper services are stubs. No runtime or headset acceptance.")
    text = json.dumps(report, indent=2) + "\n"
    if args.output:
        args.output.write_text(text, encoding="utf-8")
    print(text, end="")


if __name__ == "__main__":
    main()
