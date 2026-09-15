"""Execute pinned CE bone conversion/packing/upload and the actual skinned VS.

Native CPU instructions run in isolated synthetic memory. The real locally
extracted Anniversary GLT vertex shader runs on D3D11 WARP with stream output.
No game process is started or modified. This verifies consumers and transforms,
not live material selection, animations or headset rendering.
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
from unicorn.x86_const import (UC_X86_REG_RSP, UC_X86_REG_RIP, UC_X86_REG_RCX,
    UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_R9, UC_X86_REG_RAX)

IMAGE_SHA = "0A12DC561780F449D3F4D0DF10BB8D3BC7BE7840A5BEB2B236F672EB6CD42E6C"
SHADER_SHA = "B8F433CF95831AD2F19A4B993EB407E870B9DE7598F99E0E3486B7435BAFFD86"
BASE, HEAP, STACK, STOP = 0x180000000, 0x60000000, 0x70000000, 0x71000000


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", type=Path, default=ROOT/"out/deps/re-tools/inputs/halo1.dll")
    parser.add_argument("--shader", type=Path, default=ROOT/"out/ce-anniversary-shaders/glt/0013-001.dxbc")
    parser.add_argument("--zfill-shader", type=Path, default=ROOT/"out/ce-anniversary-shaders/zfill/0000-001.dxbc")
    parser.add_argument("--sfx-shader", type=Path, default=ROOT/"out/ce-anniversary-shaders/sfx/0002-003.dxbc")
    parser.add_argument("--adapter-exe", type=Path,
        default=ROOT/"out/build/release/Release/halomccvr_ce_first_person_tests.exe")
    parser.add_argument("--shader-exe", type=Path,
        default=ROOT/"out/build/release/Release/halomccvr_ce_first_person_shader_tests.exe")
    args = parser.parse_args()
    raw = args.image.read_bytes()
    assert hashlib.sha256(raw).hexdigest().upper() == IMAGE_SHA, "Pinned CE image hash mismatch"
    assert hashlib.sha256(args.shader.read_bytes()).hexdigest().upper() == SHADER_SHA, "Pinned shader hash mismatch"
    assert hashlib.sha256(args.zfill_shader.read_bytes()).hexdigest().upper() == "9913BF9C2CD321399F1AC39C4FD5E9F6B85A566F9A1E91149847493A7AF35489", "Pinned ZFILL shader hash mismatch"
    assert hashlib.sha256(args.sfx_shader.read_bytes()).hexdigest().upper() == "3D3EDD95C2712BE65D7D45CCC76020A7E8E9548F0D9191F0B49D4BF1DADF3CFF", "Pinned SFX shader hash mismatch"
    mapped = pefile.PE(data=raw, fast_load=True).get_memory_mapped_image()
    uc = Uc(UC_ARCH_X86, UC_MODE_64)
    image_size = (len(mapped)+4095)&~4095
    uc.mem_map(BASE, image_size)
    uc.mem_write(BASE, mapped)
    uc.mem_protect(BASE, image_size, UC_PROT_READ | UC_PROT_EXEC)
    uc.mem_map(HEAP, 0x20000)
    uc.mem_map(STACK, 0x20000)
    uc.mem_map(STOP, 0x1000)
    instructions, copies = 0, 0

    def write(address, fmt, *values):
        uc.mem_write(address, struct.pack(fmt, *values))

    def native_return():
        sp = uc.reg_read(UC_X86_REG_RSP)
        target = struct.unpack("<Q", uc.mem_read(sp, 8))[0]
        uc.reg_write(UC_X86_REG_RSP, sp+8)
        uc.reg_write(UC_X86_REG_RIP, target)

    def instruction(machine, address, size, _):
        nonlocal instructions, copies
        if address == STOP:
            machine.emu_stop()
            return
        if address == BASE+0x1391640:  # compiler cookie service, no render math
            native_return()
            return
        if address == BASE+0x1606FB1:  # native uploader's exact memcpy call
            target, source, count = (uc.reg_read(r) for r in (UC_X86_REG_RCX,UC_X86_REG_RDX,UC_X86_REG_R8))
            assert HEAP <= source < HEAP+0x20000 and HEAP <= target < HEAP+0x20000 and 0 < count <= 3200
            uc.mem_write(target, bytes(uc.mem_read(source,count)))
            uc.reg_write(UC_X86_REG_RAX,target)
            copies += 1
            native_return()
            return
        if not any(BASE+start <= address < BASE+end for start,end in
            ((0x77E70,0x77FAE),(0x2CF700,0x2CFC32),(0x30E290,0x30E4CF),
             (0xF9B80,0xF9CCB))):
            raise RuntimeError(f"Unexpected native instruction {address:#x}")
        instructions += 1
    uc.hook_add(UC_HOOK_CODE,instruction)

    def run(rva, args):
        sp = STACK+0x1FE08
        uc.mem_write(sp,b"\0"*128)
        write(sp,"<Q",STOP)
        for reg,value in zip((UC_X86_REG_RCX,UC_X86_REG_RDX,UC_X86_REG_R8,UC_X86_REG_R9),args):
            uc.reg_write(reg,value)
        for index,value in enumerate(args[4:]):
            write(sp+0x28+index*8,"<Q",value)
        uc.reg_write(UC_X86_REG_RSP,sp)
        uc.emu_start(BASE+rva,STOP,count=100000)
        assert uc.reg_read(UC_X86_REG_RIP)==STOP, f"Native function {rva:x} did not return"

    cases = []
    with tempfile.TemporaryDirectory(prefix="ce-native-fp-shader-",dir=ROOT/"out") as scratch:
        before, after = Path(scratch)/"scale-input.bin",Path(scratch)/"scale-output.bin"
        # Distinct independently moving carriers plus full-size, resized and
        # floating-arm scales. The synthetic inputs use CE's proven Z-up axes.
        for index,(scale,yaw,position) in enumerate(((1,.2,(1,-2,3)),(.3,-.7,(-3,1,2)),
                (3,1.1,(4,3,1)),(.00001,-1.5,(3,2,4)),(1,2.1,(98.925,180.641,304.072)))):
            c,s = math.cos(yaw),math.sin(yaw)
            write(HEAP,"<13f",scale,c,s,0,-s,c,0,0,0,1,*position)
            run(0x77E70,[HEAP,HEAP+0x100])
            native = bytes(uc.mem_read(HEAP+0x100,64))
            before.write_bytes(struct.pack("<f",scale)+native)
            subprocess.run([str(args.adapter_exe.resolve()),"--saber-native-scale-fixture",str(before),str(after)],
                check=True,capture_output=True,text=True)
            converted = after.read_bytes()
            assert len(converted)==64
            cases.append(dict(scale=scale,yaw=yaw,position=position,converted=converted))
            bone,matrix = HEAP+0x1000+index*0x100,HEAP+0x3000+index*0x40
            write(bone+0x70,"<Q",matrix)
            uc.mem_write(matrix,converted)
            write(HEAP+0x5000+index*8,"<Q",bone)

        count = len(cases)
        # Execute native range selection and constant-buffer staging, using
        # private record/pool identities. mem_write initializes a synthetic PE
        # data global; native code pages remain read/execute and unchanged.
        write(BASE+0x2B0A920,"<Q",HEAP+0x7000)
        wrapper,record,renderer,common,skin = (HEAP+x for x in (0x8000,0x8100,0x9000,0xA000,0xB000))
        write(wrapper,"<Q",renderer);write(wrapper+0x10,"<Q",record)
        write(record+0xD2,"<H",0xFFFF)
        write(record+0xF0,"<B",count)
        write(renderer+0xD8,"<Q",common);write(renderer+0xF8,"<Q",skin)
        write(common+8,"<Q",HEAP+0xC000);write(skin+8,"<Q",HEAP+0xD000)
        fixture = Path(scratch)/"native-palette.bin"
        summary = []
        for apply_bind in (False,True):
            # Native flags bit zero bypasses the optional bind array. Also
            # execute the real +0x60 array path with distinct rotations and
            # translations, including the native 0xF9B80 matrix multiply.
            write(HEAP+0x6000,"<I",0 if apply_bind else 1)
            write(HEAP+0x6060,"<Q",HEAP+0x11000)
            expected_bones=[]
            for index,case in enumerate(cases):
                matrix=struct.unpack("<16f",case["converted"])
                angle=.15*(index+1)
                c,s=math.cos(angle),math.sin(angle)
                bind=struct.unpack("<16f",struct.pack("<16f",c,s,0,0,-s,c,0,0,
                    0,0,1,0,2+index,-3+index,.5*index,1))
                write(HEAP+0x11000+index*64,"<16f",*bind)
                expected=matrix if not apply_bind else tuple(sum(
                    bind[row*4+k]*matrix[k*4+column] for k in range(4))
                    for row in range(4) for column in range(4))
                expected_bones.append(struct.pack("<16f",*expected))
            run(0x2CF700,[HEAP+0x6000,HEAP+0x7000,0,HEAP+0x5000,0,count,0,0,0,0,0])
            packed=bytes(uc.mem_read(HEAP+0x7000,count*48))
            for index,expected in enumerate(expected_bones):
                matrix=struct.unpack("<16f",expected)
                expected_values=[matrix[column*4+row] for row in range(3) for column in range(4)]
                actual=struct.unpack_from("<12f",packed,index*48)
                assert all(abs(a-b)<.002+abs(b)*.000002 for a,b in zip(actual,expected_values)), (apply_bind,index,actual,expected_values)
            uc.mem_write(HEAP+0xD000,b"\xcd"*(count*48))
            run(0x30E290,[wrapper])
            uploaded=bytes(uc.mem_read(HEAP+0xD000,count*48))
            assert uploaded==packed and copies==(2 if apply_bind else 1)
            assert uc.mem_read(skin+0x30,1)==b"\x01"
            assert uc.mem_read(renderer+0x2C8,1)==b"\x01"
            fixture.write_bytes(struct.pack("<I",count)+b"".join(expected_bones)+uploaded)
            for shader,kind in [(args.shader,None),(args.zfill_shader,"zfill"),(args.sfx_shader,"sfx")]:
                command=[str(args.shader_exe.resolve()),str(shader.resolve()),str(fixture)]
                if kind: command.append(kind)
                result=subprocess.run(command,capture_output=True,text=True)
                if result.returncode:
                    raise RuntimeError(result.stdout+result.stderr)
                summary.append(dict(bind_array=apply_bind,shader=str(shader),
                    sha256=hashlib.sha256(shader.read_bytes()).hexdigest().upper(),result=result.stdout.strip()))
    print(json.dumps(dict(status="PASS_NATIVE_SKIN_PACK_UPLOAD_AND_GLT_ZFILL_SFX_SHADERS",image_sha256=IMAGE_SHA,
        shader_sha256=SHADER_SHA,shader_fixture=str(args.shader),
        adapter_sha256=hashlib.sha256(args.adapter_exe.read_bytes()).hexdigest().upper(),
        shader_exe_sha256=hashlib.sha256(args.shader_exe.read_bytes()).hexdigest().upper(),
        native_instructions=instructions,native_memcpy_calls=copies,
        cases=[{k:v for k,v in case.items() if k!="converted"} for case in cases],gpu_result=summary,
        limits="Synthetic bones/bind arrays/ranges; compiler cookie and memcpy services stubbed. Native optional bind multiply and actual GLT/ZFILL/SFX shaders executed by WARP; live draw selection and headset untested."),indent=2))


if __name__=="__main__":
    main()
