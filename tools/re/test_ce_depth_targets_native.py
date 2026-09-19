"""Execute pinned CE depth target selection, binding and clearing in Unicorn.

This is an offline native-instruction test, not a game or GPU render. It runs
the actual selector, wrapper accessors, target binder and clear wrapper. Only
D3D setter endpoints and the security-cookie return are modeled. Synthetic
resource identities allow a separate check of the all-depth-then-scene order:
different DSVs can still refer to the same texture, while recycling a final
color source after its capture has no such interpass dependency.
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
from unicorn import (Uc, UC_ARCH_X86, UC_MODE_64, UC_HOOK_CODE,
                     UC_PROT_READ, UC_PROT_EXEC)
from unicorn.x86_const import (UC_X86_REG_RSP, UC_X86_REG_RIP,
    UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_R9,
    UC_X86_REG_RAX, UC_X86_REG_XMM3)

SHA = "0A12DC561780F449D3F4D0DF10BB8D3BC7BE7840A5BEB2B236F672EB6CD42E6C"
BASE, HEAP, STACK, STOP = 0x180000000, 0x60000000, 0x70000000, 0x71000000
# Full pinned bodies, including split unwind fragments and leaf functions.
# Binding 205E40's first unwind fragment ends early at205F75; its real return
# is206151. It must execute the later cached-DSV/OMSetRenderTargets stores.
RANGES = ((0x205E40, 0x206160), (0x204E40, 0x204F40),
          (0xAD5F0, 0xAD648), (0x1DC120, 0x1DC1D4),
          (0x1F4B80, 0x1F4BB1), (0x1F4BC0, 0x1F4BF1),
          (0x22B830, 0x22B85A), (0x2059B0, 0x205BD1),
          (0x206160, 0x206233))


def run(image, *, config9=True, selection=True, children="independent", third_view=False):
    uc = Uc(UC_ARCH_X86, UC_MODE_64)
    image_size = (len(image)+4095)&~4095
    uc.mem_map(BASE, image_size)
    uc.mem_write(BASE, image)
    uc.mem_protect(BASE, image_size, UC_PROT_READ | UC_PROT_EXEC)
    uc.mem_map(HEAP, 0x200000)
    uc.mem_map(STACK, 0x20000)
    uc.mem_map(STOP, 0x1000)
    backend, descriptor = HEAP+0x10000, HEAP+0x90000
    context, vtable = HEAP+0xA0000, HEAP+0xA1000
    parent, config, selector = HEAP+0xA2000, HEAP+0xA3000, HEAP+0xA4000
    constant, data = HEAP+0xA5000, HEAP+0xA6000
    events, viewport_calls, scissor_calls = [], [], []
    dsv_resources, owners = {}, {}
    state = dict(eye=-1, stage="", instructions=0)

    def write(at, fmt, *values):
        uc.mem_write(at, struct.pack(fmt, *values))

    def read(at, fmt):
        return struct.unpack(fmt, uc.mem_read(at, struct.calcsize(fmt)))

    def ret():
        sp = uc.reg_read(UC_X86_REG_RSP)
        uc.reg_write(UC_X86_REG_RIP, read(sp, "<Q")[0])
        uc.reg_write(UC_X86_REG_RSP, sp+8)

    def service(machine, address, size, _):
        state["instructions"] += 1
        if address == STOP:
            machine.emu_stop()
            return
        if address == BASE+0x1391640:
            ret()
            return
        if STOP+0x100 <= address <= STOP+0x400:
            assert machine.reg_read(UC_X86_REG_RCX) == context
            if address == STOP+0x100:
                count = machine.reg_read(UC_X86_REG_RDX)
                viewport_calls.append([read(machine.reg_read(UC_X86_REG_R8)+24*i,
                                            "<6f") for i in range(count)])
            elif address == STOP+0x200:
                count = machine.reg_read(UC_X86_REG_RDX)
                scissor_calls.append([read(machine.reg_read(UC_X86_REG_R8)+16*i,
                                           "<4i") for i in range(count)])
            elif address == STOP+0x300:
                assert machine.reg_read(UC_X86_REG_RDX) == 0
                dsv = machine.reg_read(UC_X86_REG_R9)
                assert dsv in dsv_resources
                events.append(dict(stage=state["stage"], eye=state["eye"],
                    kind="bind", dsv=dsv, resource=dsv_resources[dsv]))
            elif address == STOP+0x400:
                dsv = machine.reg_read(UC_X86_REG_RDX)
                flags = machine.reg_read(UC_X86_REG_R8)
                depth = struct.unpack("<f", struct.pack("<I",
                    machine.reg_read(UC_X86_REG_XMM3)&0xFFFFFFFF))[0]
                stencil = read(machine.reg_read(UC_X86_REG_RSP)+0x28, "<B")[0]
                assert dsv in dsv_resources and flags == 3 and depth == 1 and stencil == 0
                assert read(backend+0xD20, "<Q")[0] == dsv
                owners[dsv_resources[dsv]] = None
                events.append(dict(stage=state["stage"], eye=state["eye"],
                    kind="clear", dsv=dsv, resource=dsv_resources[dsv], flags=flags))
            else:
                raise AssertionError(f"Unexpected D3D service {address:#x}")
            ret()
            return
        if not any(start <= address-BASE < end for start, end in RANGES):
            raise AssertionError(f"Unexpected native execution {address-BASE:#x}")

    uc.hook_add(UC_HOOK_CODE, service)
    write(backend, "<Q", BASE+0x17F9D10)
    write(backend+0xCE0, "<Q", context)
    write(context, "<Q", vtable)
    for slot, offset in [(44, 0x100), (45, 0x200), (33, 0x300), (53, 0x400)]:
        write(vtable+8*slot, "<Q", STOP+offset)
    write(BASE+0x2E3BDD8, "<Q", parent)
    write(BASE+0x1BEA6B8, "<Q", selector)
    write(parent+0x114, "<I", 2)
    write(parent+0x118, "<Q", config)
    write(config, "<I", (1 << 9) if config9 else 0)
    write(backend+0xD8, "<Q", constant)
    write(constant+8, "<Q", data)
    write(backend+0x84, "<fB", 1, 0)

    def wrapper(slot, height, *, resource=None, dsv=None):
        value = HEAP+0xB0000+slot*0x300
        resource = HEAP+0x110000+slot*0x100 if resource is None else resource
        dsv = 0x74000000+slot*0x100 if dsv is None else dsv
        write(value, "<Q", BASE+0x17FB608)
        write(value+0x10, "<hh", 2912, height)
        write(value+0x1A, "<b", 1)
        write(value+0x88, "<I", 1 << 9)
        write(value+0xE0, "<Q", resource)
        write(value+0x108, "<Q", dsv)
        write(value+0x110, "<Q", dsv)
        dsv_resources[dsv] = resource
        return value

    primary = wrapper(0, 2100)
    left = wrapper(1, 1050)
    if children == "aliased_wrapper":
        right = left
    elif children == "aliased_dsv":
        right = wrapper(2, 1050, resource=read(left+0xE0, "<Q")[0],
                        dsv=read(left+0x108, "<Q")[0])
    elif children == "aliased_resource":
        right = wrapper(2, 1050, resource=read(left+0xE0, "<Q")[0])
    else:
        assert children in ("independent", "missing")
        right = wrapper(2, 1050)
    if children != "missing":
        write(primary+0xA8, "<QQ", left, right)
    write(descriptor+0x30, "<Q", primary)

    def call(rva, args):
        sp = STACK+0x1FF08
        write(sp, "<Q", STOP)
        uc.reg_write(UC_X86_REG_RSP, sp)
        for register, arg in zip((UC_X86_REG_RCX, UC_X86_REG_RDX,
                                  UC_X86_REG_R8, UC_X86_REG_R9), args):
            uc.reg_write(register, arg)
        uc.emu_start(BASE+rva, STOP, count=100000)
        assert uc.reg_read(UC_X86_REG_RIP) == STOP, "Native instruction budget exhausted"
        return uc.reg_read(UC_X86_REG_RAX)

    selected, depth_receipts, scene_receipts = [], [], []
    # Native455A10 calls457510/clear for all depth views before its scene loop.
    # The native binder/clear below are real instructions. In place of omitted
    # scene geometry, owners records which eye would last have written a target.
    for stage in ("depth", "scene"):
        for eye in ((0, 1, 2) if third_view else (0, 1)):
            state.update(stage=stage, eye=eye)
            # Native455A10 compares viewIndex == 1; all other primary views
            # clear selector16. A third list entry is NOT a third target.
            write(selector+0x1D0, "<I", ((1 << 15) if selection else 0) | ((eye == 1) << 16))
            selected_wrapper = call(0xAD5F0, (primary,))
            expected = (right if eye == 1 else left) if (
                config9 and selection and children != "missing") else primary
            assert selected_wrapper == expected
            selected.append(selected_wrapper)
            assert call(0x205E40, (backend, descriptor))&0xFF == 1
            dsv = read(backend+0xD20, "<Q")[0]
            resource = read(selected_wrapper+0xE0, "<Q")[0]
            assert dsv == read(selected_wrapper+0x108, "<Q")[0]
            assert resource == dsv_resources[dsv]
            if stage == "depth":
                call(0x204E40, (backend, 6, 0))
                owners[resource] = eye  # modeled geometry write after native clear
                depth_receipts.append((selected_wrapper, resource, dsv))
            else:
                scene_receipts.append((selected_wrapper, resource, dsv, owners[resource]))
    resources_distinct = depth_receipts[0][1] != depth_receipts[1][1]
    correct_depth = all(receipt[3] == eye for eye, receipt in enumerate(scene_receipts))
    if third_view:
        assert resources_distinct and not correct_depth
        assert depth_receipts[2][1] == depth_receipts[0][1]
        assert scene_receipts[0][3] == 2, "third depth must overwrite left before scene"
    else:
        assert resources_distinct == correct_depth
        assert correct_depth == (config9 and selection and children == "independent")
    # Native dimensions route through SPLIT_1 whenever selector15 is set;
    # unlike AD5F0, width/height getters do not test config9 or selector16.
    height = 1050 if selection and children != "missing" else 2100
    count = 3 if third_view else 2
    assert viewport_calls == [[(0.0, 0.0, 2912.0, float(height), 0.0, 1.0)]]*(count*2)
    assert scissor_calls == [[(0, 0, 2912, height)]]*(count*2)
    assert [event["kind"] for event in events] == ["bind", "clear"]*count+["bind"]*count
    return dict(config9=config9, selection15=selection, children=children,
        selected_wrappers=selected, depth_receipts=depth_receipts,
        scene_receipts=scene_receipts, resources_distinct=resources_distinct,
        modeled_correct_depth=correct_depth, viewport_height=height,
        third_view=third_view, instructions=state["instructions"], native_events=events)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", type=Path, default=ROOT/"out/deps/re-tools/inputs/halo1.dll")
    args = parser.parse_args()
    raw = args.image.read_bytes()
    if hashlib.sha256(raw).hexdigest().upper() != SHA:
        raise SystemExit("Pinned CE image hash mismatch")
    image = pefile.PE(data=raw, fast_load=True).get_memory_mapped_image()
    cases = [run(image), run(image, config9=False), run(image, selection=False),
             run(image, children="missing"), run(image, children="aliased_wrapper"),
             run(image, children="aliased_dsv"), run(image, children="aliased_resource"),
             run(image, third_view=True)]
    print(json.dumps(dict(status="PASS_NATIVE_DEPTH_ROUTING_EMULATION_ONLY",
        image_sha256=SHA, cases=cases,
        limit="D3D endpoints and geometry ownership are modeled; no native scene, GPU, or headset result."), indent=2))


if __name__ == "__main__":
    main()
