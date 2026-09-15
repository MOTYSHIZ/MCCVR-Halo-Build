"""Execute pinned CE first-person GLT lens selection and the production adapter.

Synthetic material records in an isolated x64 emulator; no game process,
shader/GPU execution, native registration services or headset rendering.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "out/pydeps"))
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64, UC_HOOK_CODE, UC_PROT_READ, UC_PROT_EXEC
from unicorn.x86_const import (UC_X86_REG_RSP, UC_X86_REG_RIP, UC_X86_REG_RCX,
    UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_R9, UC_X86_REG_RSI)

SHA = "0A12DC561780F449D3F4D0DF10BB8D3BC7BE7840A5BEB2B236F672EB6CD42E6C"
BASE, HEAP, STACK, STOP = 0x180000000, 0x60000000, 0x70000000, 0x71000000


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", type=Path, default=ROOT/"out/deps/re-tools/inputs/halo1.dll")
    parser.add_argument("--adapter-exe", type=Path,
        default=ROOT/"out/build/release/Release/halomccvr_ce_first_person_tests.exe")
    args = parser.parse_args()
    raw = args.image.read_bytes()
    if hashlib.sha256(raw).hexdigest().upper() != SHA:
        raise SystemExit("Pinned CE image hash mismatch")
    mapped = pefile.PE(data=raw, fast_load=True).get_memory_mapped_image()
    uc = Uc(UC_ARCH_X86, UC_MODE_64)
    uc.mem_map(BASE, (len(mapped)+4095)&~4095)
    uc.mem_write(BASE, mapped)
    uc.mem_protect(BASE, (len(mapped)+4095)&~4095, UC_PROT_READ | UC_PROT_EXEC)
    uc.mem_map(HEAP, 0x10000)
    uc.mem_map(STACK, 0x20000)
    uc.mem_map(STOP, 0x1000)
    instructions = 0

    def instruction(machine, address, size, _):
        nonlocal instructions
        if address in (STOP, BASE+0x7AE1A):
            machine.emu_stop()
            return
        if not (BASE+0x264080 <= address < BASE+0x2646D5 or
                BASE+0x26FF00 <= address < BASE+0x26FF2C or
                BASE+0x26E960 <= address < BASE+0x26ECE2 or
                BASE+0x7ADF0 <= address < BASE+0x7AE1A):
            raise RuntimeError(f"Unexpected native instruction {address:#x}")
        instructions += 1
    uc.hook_add(UC_HOOK_CODE, instruction)

    def write(address, fmt, *values):
        uc.mem_write(address, struct.pack(fmt, *values))

    # Execute the real FP registration loop over three synthetic model slots.
    # Creation/update services outside this exact block are not simulated.
    write(HEAP+0x10, "<Q", HEAP+0x100)
    write(HEAP+0x100+0x190, "<I", 3)
    write(HEAP+0x100+0x198, "<Q", HEAP+0x300)
    models = [HEAP+0x1000+i*0x100 for i in range(3)]
    write(HEAP+0x300, "<3Q", *models)
    initial = [0, 1, 0x80000000]
    for model, flags in zip(models, initial):
        write(model+0x28, "<I", flags)
    uc.reg_write(UC_X86_REG_RSI, HEAP)
    uc.reg_write(UC_X86_REG_RDX, 0)
    uc.reg_write(UC_X86_REG_R8, 0)
    uc.emu_start(BASE+0x7ADF0, BASE+0x7AE1A, count=1000)
    assert uc.reg_read(UC_X86_REG_RIP) == BASE+0x7AE1A
    flags = [struct.unpack("<I", uc.mem_read(model+0x28, 4))[0] for model in models]
    assert flags == [value | 0x10000000 for value in initial]

    # The GLT writer's unused material variants stay zero, selecting no external
    # service calls. All eight ABI arguments and nested ambient/color records
    # are explicit. The complete pinned function must return normally.
    constants, model, instance, ambient = HEAP+0x2000, models[0], HEAP+0x3000, HEAP+0x3100
    variants, material, params = HEAP+0x4000, HEAP+0x5000, HEAP+0x6000
    write(model+0x10, "<Q", instance)
    write(instance+0x20, "<Q", ambient)
    write(instance+0x28, "<I", 0xFFFFFFFF)
    for offset in (0x9C, 0xDC, 0x11C):
        write(material+offset, "<b", -1)
    sp = STACK+0x1FE08
    results = []
    with tempfile.TemporaryDirectory(prefix="ce-native-fp-lens-", dir=ROOT/"out") as scratch:
        before, after = Path(scratch)/"input.bin", Path(scratch)/"output.bin"
        for writer,rva,selector_offset in [("GLT",0x264080,0x170),("ZFILL",0x26FF00,0x20),("SFX",0x26E960,0x70)]:
          for model_flags in [0, 1, 0x80000000, *flags]:
            write(model+0x28, "<I", model_flags)
            uc.mem_write(constants, b"\xcd"*384)
            write(sp, "<Q", STOP)
            write(sp+0x28, "<4Q", HEAP+0x7000, material, 0, params)
            for reg, value in [(UC_X86_REG_RSP, sp), (UC_X86_REG_RCX, 0),
                    (UC_X86_REG_RDX, constants), (UC_X86_REG_R8, model), (UC_X86_REG_R9, variants)]:
                uc.reg_write(reg, value)
            uc.emu_start(BASE+rva, STOP, count=10000)
            assert uc.reg_read(UC_X86_REG_RIP) == STOP
            native = bytes(uc.mem_read(constants, 384))
            first_person = bool(model_flags & 0x10000000)
            assert struct.unpack_from("<4f", native,selector_offset) == (float(first_person),)*4
            before.write_bytes(struct.pack("<2I", model_flags,selector_offset)+native)
            subprocess.run([str(args.adapter_exe.resolve()), "--saber-native-projection-layout-fixture",
                str(before), str(after)], check=True, capture_output=True)
            corrected = after.read_bytes()
            assert len(corrected) == 384 and corrected[:selector_offset] == native[:selector_offset]
            assert corrected[selector_offset+16:] == native[selector_offset+16:]
            assert struct.unpack_from("<4f", corrected,selector_offset) == (0.0,)*4
            if not first_person:
                assert corrected == native
            results.append(dict(writer=writer,model_flags=hex(model_flags), native_fixed_lens=first_person,
                adapter_world_lens=True, unrelated_constants_preserved=True))
    print(json.dumps(dict(status="PASS_NATIVE_FP_GLT_ZFILL_SFX_SELECTION_ONLY", image_sha256=SHA,
        adapter_sha256=hashlib.sha256(args.adapter_exe.read_bytes()).hexdigest().upper(),
        native_registration_models=3, native_material_calls=len(results), instructions=instructions,
        cases=results, limit="Synthetic native records and compiled helper; no GPU, game or headset."), indent=2))


if __name__ == "__main__":
    main()
