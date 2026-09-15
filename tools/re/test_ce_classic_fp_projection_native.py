"""Execute CE Classic's pinned weapon/effect lens setter and production selector.

No process access or game-file writes. Native projection, save/restore, depth
adjustment and constant composition execute in Unicorn. The CRT tan service,
GPU constant upload and security-cookie service are explicit isolated stubs.
Synthetic frusta establish projection arithmetic, not live weapon rendering.
"""
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
from unicorn.x86_const import *

SHA = "0A12DC561780F449D3F4D0DF10BB8D3BC7BE7840A5BEB2B236F672EB6CD42E6C"
BASE, STACK, STOP = 0x180000000, 0x70000000, 0x71000000
CAMERA, FRUSTUM = BASE+0x29E0588, BASE+0x29E05DC
CALLERS = [0xC1769F, 0xC159B1, 0xBF4440, 0xC12354, 0xC12B52]


def main():
    raw = (ROOT / "out/deps/re-tools/inputs/halo1.dll").read_bytes()
    assert hashlib.sha256(raw).hexdigest().upper() == SHA
    mapped = pefile.PE(data=raw, fast_load=True).get_memory_mapped_image()
    uc = Uc(UC_ARCH_X86, UC_MODE_64)
    size = (len(mapped)+4095)&~4095
    uc.mem_map(BASE, size)
    uc.mem_write(BASE, mapped)
    uc.mem_protect(BASE, size, UC_PROT_READ | UC_PROT_EXEC)
    # Only the native raster camera/frustum and native save slots are writable.
    uc.mem_protect(BASE+0x29E0000, 0x1000, 7)
    uc.mem_protect(BASE+0x3BBC000, 0x1000, 7)
    uc.mem_map(STACK, 0x20000)
    uc.mem_map(STOP, 0x1000)
    instructions, uploads = 0, []

    def f32(value): return struct.unpack("<I", struct.pack("<f", value))[0]
    def ret():
        sp = uc.reg_read(UC_X86_REG_RSP)
        uc.reg_write(UC_X86_REG_RIP, struct.unpack("<Q", uc.mem_read(sp, 8))[0])
        uc.reg_write(UC_X86_REG_RSP, sp+8)

    def instruction(machine, address, length, _):
        nonlocal instructions
        if address == STOP:
            machine.emu_stop(); return
        if address == BASE+0x1607047:
            value = struct.unpack("<f", struct.pack("<I", uc.reg_read(UC_X86_REG_XMM0)&0xffffffff))[0]
            uc.reg_write(UC_X86_REG_XMM0, f32(math.tan(value))); ret(); return
        if address == BASE+0x1391640:
            ret(); return
        if address == BASE+0xBF632C:
            count = uc.reg_read(UC_X86_REG_R9)
            assert count in (2, 6)
            uploads.append((uc.reg_read(UC_X86_REG_RDX), bytes(uc.mem_read(uc.reg_read(UC_X86_REG_R8), count*16))))
            ret(); return
        assert (BASE+0xAE18CC <= address < BASE+0xAE1B5A or
                BASE+0xB8EE3C <= address < BASE+0xB8F144), hex(address)
        instructions += 1
    uc.hook_add(UC_HOOK_CODE, instruction)

    def invoke(rva, first, second):
        sp = STACK+0x1FE08
        uc.mem_write(sp, struct.pack("<Q", STOP))
        uc.reg_write(UC_X86_REG_RSP, sp)
        uc.reg_write(UC_X86_REG_XMM0, f32(first))
        uc.reg_write(UC_X86_REG_XMM1, f32(float(second)))
        uc.reg_write(UC_X86_REG_RDX, int(second))
        uc.emu_start(BASE+rva, STOP, count=10000)
        assert uc.reg_read(UC_X86_REG_RIP) == STOP

    def lens(value, rebuild=False): invoke(0xAE1AF0, value, rebuild)
    def depth(near, far): invoke(0xAE1AB8, near, far)
    def frustum(): return bytes(uc.mem_read(FRUSTUM, 0x18C))
    def floats(data, offsets): return [struct.unpack_from("<f", data, at)[0] for at in offsets]

    fixed = struct.unpack("<f", uc.mem_read(BASE+0x194F4AC, 4))[0]
    assert abs(fixed-.9671381116) < 1e-8
    cases = []
    adapter = ROOT / "out/build/release/Release/halomccvr_ce_first_person_tests.exe"
    with tempfile.TemporaryDirectory(prefix="ce-classic-fp-native-", dir=ROOT/"out") as scratch:
        before, after = Path(scratch)/"input.bin", Path(scratch)/"output.bin"
        for width, height, fov in [(1920,1080,1.4), (1280,1280,1.9), (1600,900,2.2)]:
          for eye in (-1, 1):
            source = bytearray(0x18C)
            struct.pack_into("<4f", source, 0, -1,1,-1,1)
            fx, fy = 1/(math.tan(fov/2)*width/height), 1/math.tan(fov/2)
            for offset, value in [(0x144,fx),(0x158,fy),(0x164,eye*.025),
                    (0x178,.0125),(0x184,fx*width/2),(0x188,fy*height/2),
                    (0x14C,.001),(0x15C,.002),(0x16C,.003),(0x17C,.004)]:
                struct.pack_into("<f", source, offset, value)
            uc.mem_write(CAMERA, bytes(0x240))
            uc.mem_write(CAMERA+0x2C, struct.pack("<4h", 0,0,height,width))
            uc.mem_write(FRUSTUM, bytes(source))
            lens(-1)  # Native world projection save.
            depth(-1,-1)  # Native world depth save.
            lens(fixed)
            stock = frustum()
            assert abs(floats(stock,[0x144])[0]-fx) > .05
            assert abs(floats(stock,[0x158])[0]-fy) > .05
            lens(0)
            # Native restore saves +168, whereas the FOV rewrite touches +178.
            # Reset the complete baseline to test the tracked no-write path.
            for caller in CALLERS:
                uc.mem_write(FRUSTUM, bytes(source))
                before.write_bytes(struct.pack("<Qf", caller, fixed))
                subprocess.run([str(adapter), "--classic-native-projection-fixture", str(before), str(after)], check=True)
                selected = struct.unpack("<f", after.read_bytes())[0]
                assert selected == -2
                lens(selected, caller==0xC12B52)
                assert frustum() == bytes(source), hex(caller)
                # Real native depth-range adjustment still affects only Z.
                depth(.01,50)
                tracked_depth = frustum()
                assert floats(tracked_depth,[0x144,0x158,0x164,0x178]) == floats(source,[0x144,0x158,0x164,0x178])
                assert floats(tracked_depth,[0x14C,0x15C,0x16C,0x17C]) != floats(source,[0x14C,0x15C,0x16C,0x17C])
                lens(0);depth(0,0)
                assert frustum() == bytes(source)
                cases.append({"return_rva":hex(caller),"raster":[width,height],"eye":eye,"world_fov":fov})
        # A foreign positive request and the save/restore/skip sentinels cannot
        # be transformed merely because the native lens helper was called.
        for caller, value in [(0xC196B6,fixed), (CALLERS[0],-1), (CALLERS[0],0), (CALLERS[0],-2)]:
            before.write_bytes(struct.pack("<Qf",caller,value))
            subprocess.run([str(adapter),"--classic-native-projection-fixture",str(before),str(after)],check=True)
            assert after.read_bytes()==struct.pack("<f",value)
    print(json.dumps({"result":"PASS_OFFLINE_ONLY","sha256":SHA,"cases":len(cases),
        "adapter_sha256":hashlib.sha256(adapter.read_bytes()).hexdigest().upper(),
        "native_instructions":instructions,"native_constant_uploads":len(uploads),
        "fixed_weapon_fov_degrees":math.degrees(fixed),"details":cases},indent=2))


if __name__ == "__main__": main()
