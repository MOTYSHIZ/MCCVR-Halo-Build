"""Check the pinned Anniversary bone converter through the production scale adapter.

No game process or file writes. Native x64 instructions run against synthetic
bones in Unicorn; only the compiled helper subprocess runs on Windows. This
proves matrix conversion/scale behavior, not native mesh or headset rendering.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "out/pydeps"))
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64, UC_HOOK_CODE, UC_PROT_READ, UC_PROT_EXEC
from unicorn.x86_const import UC_X86_REG_RSP, UC_X86_REG_RIP, UC_X86_REG_RCX, UC_X86_REG_RDX

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
        if address == STOP:
            machine.emu_stop()
            return
        if not BASE+0x77E70 <= address < BASE+0x77FAE:
            raise RuntimeError(f"Unexpected native instruction {address:#x}")
        instructions += 1
    uc.hook_add(UC_HOOK_CODE, instruction)

    def convert(scale, basis, position):
        source = struct.pack("<13f", scale, *basis, *position)
        uc.mem_write(HEAP, source)
        uc.mem_write(HEAP+0x100, b"\xcd"*64)
        sp = STACK+0x1FF08
        uc.mem_write(sp, struct.pack("<Q", STOP))
        uc.reg_write(UC_X86_REG_RSP, sp)
        uc.reg_write(UC_X86_REG_RCX, HEAP)
        uc.reg_write(UC_X86_REG_RDX, HEAP+0x100)
        uc.emu_start(BASE+0x77E70, STOP, count=1000)
        assert uc.reg_read(UC_X86_REG_RIP) == STOP
        assert uc.mem_read(HEAP, len(source)) == source
        return bytes(uc.mem_read(HEAP+0x100, 64))

    cases = []
    scratch_root = ROOT/"out"
    scratch_root.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="ce-native-skin-", dir=scratch_root) as scratch:
        before, after = Path(scratch)/"input.bin", Path(scratch)/"output.bin"
        for yaw in (0.0, 0.7, -2.1):
            c, s = math.cos(yaw), math.sin(yaw)
            basis = (c, s, 0, -s, c, 0, 0, 0, 1)
            for position in ((0, 0, 0), (98.925, 180.641, 304.072)):
                unit_bytes = convert(1, basis, position)
                unit = struct.unpack("<16f", unit_bytes)
                for scale in (0.00001, 0.3, 1.0, 3.0, 12.0):
                    native = convert(scale, basis, position)
                    # The actual converter ignores the native separate scale.
                    assert native == unit_bytes
                    before.write_bytes(struct.pack("<f", scale)+native)
                    subprocess.run([str(args.adapter_exe.resolve()), "--saber-native-scale-fixture",
                                    str(before), str(after)], check=True, capture_output=True)
                    corrected = struct.unpack("<16f", after.read_bytes())
                    for i, value in enumerate(corrected):
                        expected = unit[i]*scale if i//4 < 3 and i%4 < 3 else unit[i]
                        assert abs(value-expected) <= max(1e-6, abs(expected)*2e-7), (i, value, expected)
                    # Homogeneous column and world origin must remain byte exact.
                    result = after.read_bytes()
                    for offset in (12, 28, 44, 48, 52, 56, 60):
                        assert result[offset:offset+4] == native[offset:offset+4]
                    cases.append(dict(yaw=yaw, position=position, scale=scale))
    print(json.dumps(dict(status="PASS_NATIVE_SKIN_ADAPTER_ONLY", image_sha256=SHA,
        adapter_sha256=hashlib.sha256(args.adapter_exe.read_bytes()).hexdigest().upper(),
        cases=cases, native_calls=36, instructions=instructions,
        limit="Native converter and production scale helper only; no mesh draw, game or headset."), indent=2))


if __name__ == "__main__":
    main()
