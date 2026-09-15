"""Execute the pinned CE kind-0 output initializer and texture selector.

No process access or game writes. COM reference counting and GetDesc are
fixtures; native owner selection, variant selection, RTV lookup, publication
and dimension writes execute the original PE instructions.
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
from unicorn.x86_const import UC_X86_REG_RSP, UC_X86_REG_RIP, UC_X86_REG_RCX, UC_X86_REG_RDX

SHA = "0A12DC561780F449D3F4D0DF10BB8D3BC7BE7840A5BEB2B236F672EB6CD42E6C"
BASE, HEAP, STACK, STOP = 0x180000000, 0x60000000, 0x70000000, 0x71000000


def run(image, variant, array_decoy, previous_view):
    uc = Uc(UC_ARCH_X86, UC_MODE_64)
    uc.mem_map(BASE, (len(image)+4095)&~4095)
    uc.mem_write(BASE, image)
    uc.mem_map(HEAP, 0x20000)
    uc.mem_map(STACK, 0x20000)
    uc.mem_map(STOP, 0x1000)
    owner, root = HEAP+0x1000, HEAP+0x2000
    options, config, flags = HEAP+0xa000, HEAP+0xb000, HEAP+0xc000
    com_vtable = HEAP+0xd000
    calls = []

    def write(at, fmt, *values): uc.mem_write(at, struct.pack(fmt, *values))
    def read(at, fmt): return struct.unpack(fmt, uc.mem_read(at, struct.calcsize(fmt)))
    def ret():
        sp = uc.reg_read(UC_X86_REG_RSP)
        uc.reg_write(UC_X86_REG_RIP, read(sp, "<Q")[0])
        uc.reg_write(UC_X86_REG_RSP, sp+8)

    def service(machine, address, size, _):
        if address == BASE+0xae510:
            # Stop before allocation of kinds 1..8: the complete kind-0
            # owner/cache/descriptor path has executed without any stubs.
            machine.emu_stop()
        elif address in (STOP+0x100, STOP+0x200):
            calls.append(("addref" if address == STOP+0x100 else "release",
                          machine.reg_read(UC_X86_REG_RCX)))
            ret()
        elif address == STOP+0x300:
            resource = machine.reg_read(UC_X86_REG_RCX)
            assert resource == selected_resource
            destination = machine.reg_read(UC_X86_REG_RDX)
            write(destination, "<11I", 1556, 1078, 1, 1, 87, 1, 0, 0, 0x20, 0, 0)
            calls.append(("getdesc", resource))
            ret()

    uc.hook_add(UC_HOOK_CODE, service)
    write(BASE+0x2e3c090, "<Q", owner)
    write(owner+0x500, "<Q", root)
    write(BASE+0x1bea6b8, "<Q", options)
    write(BASE+0x2e3bdd8, "<Q", config)
    write(config+0x118, "<Q", flags)
    write(flags, "<I", 1 << 9)
    write(options+0x1d0, "<I", 0 if variant < 0 else (1 << 15) | (variant << 16))
    views, resources = [], []
    for index in range(3):
        wrapper = root+index*0x400
        table, view, resource = HEAP+0x4000+index*0x100, HEAP+0x5000+index*0x100, HEAP+0x6000+index*0x100
        write(wrapper, "<Q", BASE+0x17fb608)
        write(wrapper+0x1a, "<b", 1)
        write(wrapper+0xe0, "<Q", resource)
        write(wrapper+0xe8, "<Q", table)
        write(table, "<Q", view)
        write(view, "<Q", com_vtable)
        write(resource, "<Q", com_vtable)
        views.append(view)
        resources.append(resource)
    write(root+0xa8, "<QQ", root+0x400, root+0x800)
    write(com_vtable+8, "<QQ", STOP+0x100, STOP+0x200)
    write(com_vtable+0x50, "<Q", STOP+0x300)
    selected = 0 if variant < 0 else variant+1
    selected_resource = resources[selected]
    old_view = HEAP+0x7000 if previous_view else 0
    if old_view: write(old_view, "<Q", com_vtable)
    write(BASE+0x1b85e78, "<Q", old_view)
    decoy = root+0x400 if array_decoy else 0
    write(BASE+0x2e3b910, "<Q", decoy)
    sp = STACK+0x1ff08
    write(sp, "<Q", STOP)
    uc.reg_write(UC_X86_REG_RSP, sp)
    uc.emu_start(BASE+0xae410, STOP, count=1000)
    assert uc.reg_read(UC_X86_REG_RIP) == BASE+0xae510
    assert read(BASE+0x1b85e78, "<Q")[0] == views[selected]
    assert read(BASE+0x2e3b910, "<Q")[0] == decoy
    assert read(BASE+0x1b85e68, "<III") == (1556, 1078, 87)
    expected = [("addref", views[selected])]
    if old_view: expected.append(("release", old_view))
    expected.append(("getdesc", selected_resource))
    assert calls == expected, calls
    return dict(variant=variant, array_decoy=array_decoy, previous_view=previous_view,
                source="native output owner + 0x500", array_element_ignored=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", type=Path, default=ROOT/"out/deps/re-tools/inputs/halo1.dll")
    args = parser.parse_args()
    raw = args.image.read_bytes()
    if hashlib.sha256(raw).hexdigest().upper() != SHA:
        raise SystemExit("Pinned CE image hash mismatch")
    image = pefile.PE(data=raw, fast_load=True).get_memory_mapped_image()
    cases = [run(image, variant, decoy, old) for variant in (-1, 0, 1)
             for decoy in (False, True) for old in (False, True)]
    print(json.dumps(dict(status="PASS_NATIVE_EMULATION_ONLY", cases=cases,
        limit="COM services are fixtures. Native kind-0 owner/variant/RTV selection and publication execute pinned instructions. No MCC or headset acceptance."), indent=2))


if __name__ == "__main__": main()
