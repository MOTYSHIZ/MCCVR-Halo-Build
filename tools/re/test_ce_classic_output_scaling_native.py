"""Execute pinned CE final-quad construction with independent output sizes.

Runs the native final blit and kind-0 viewport setter in Unicorn. Native draw,
shader/driver calls and GetDesc are recorded fixtures, not actual GPU rendering.
No game process or game files are modified.
"""
import hashlib
import json
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "out/pydeps"))
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64, UC_HOOK_CODE
from unicorn.x86_const import *

SHA = "0A12DC561780F449D3F4D0DF10BB8D3BC7BE7840A5BEB2B236F672EB6CD42E6C"
BASE, HEAP, STACK, STOP = 0x180000000, 0x60000000, 0x70000000, 0x71000000


def run(image, raster, output):
    uc = Uc(UC_ARCH_X86, UC_MODE_64)
    uc.mem_map(BASE, (len(image)+4095)&~4095)
    uc.mem_write(BASE, image)
    for address, size in ((HEAP, 0x10000), (STACK, 0x10000), (STOP, 0x1000)):
        uc.mem_map(address, size)
    def write(at, fmt, *v): uc.mem_write(at, struct.pack("<"+fmt, *v))
    def read(at, fmt): return struct.unpack("<"+fmt, uc.mem_read(at, struct.calcsize("<"+fmt)))
    def ret():
        sp=uc.reg_read(UC_X86_REG_RSP)
        uc.reg_write(UC_X86_REG_RIP,read(sp,"Q")[0])
        uc.reg_write(UC_X86_REG_RSP,sp+8)
    constants, vertices, viewports = [], [], []
    stubs = {0xc4bc24,0xb519a0,0xbf6208,0xb3daa4,0xb3d8c4,0xb1580,0xbf6b70,
             0x1391640,0xaeb00,0xae1ab8}
    def hook(machine, address, size, _):
        rva=address-BASE
        if rva in stubs: ret()
        elif rva==0xbf632c:
            assert machine.reg_read(UC_X86_REG_RDX)==13
            assert machine.reg_read(UC_X86_REG_R9)==5
            constants.extend(read(machine.reg_read(UC_X86_REG_R8),"20f"));ret()
        elif rva==0xbf5f08:
            assert machine.reg_read(UC_X86_REG_RDX)==24
            vertices.extend(read(machine.reg_read(UC_X86_REG_RCX),"24f"));ret()
        elif address==STOP+0x100:
            write(machine.reg_read(UC_X86_REG_RDX),"Q",HEAP+0x4000);ret()
        elif address==STOP+0x200:
            write(machine.reg_read(UC_X86_REG_RDX),"11I",*output,1,1,28,1,0,0,0x20,0,0);ret()
        elif address==STOP+0x300: ret()
        elif address==STOP+0x400:
            assert machine.reg_read(UC_X86_REG_RDX)==1
            viewports.append(read(machine.reg_read(UC_X86_REG_R8),"6f"));ret()
    uc.hook_add(UC_HOOK_CODE,hook)
    # Only the two pointer dereferences in quad setup need native global data.
    write(BASE+0x1c55c78,"Q",HEAP+0x1000)
    write(BASE+0x29e1210,"Q",HEAP+0x1000)
    write(HEAP,"4h",0,0,raster[1],raster[0])
    def call(rva,rcx=0):
        sp=STACK+0xff08;write(sp,"Q",STOP)
        uc.reg_write(UC_X86_REG_RSP,sp)
        for register,value in ((UC_X86_REG_RCX,rcx),(UC_X86_REG_RDX,0),
                               (UC_X86_REG_R8,0),(UC_X86_REG_R9,0)):
            uc.reg_write(register,value)
        uc.emu_start(BASE+rva,STOP,count=10000)
        assert uc.reg_read(UC_X86_REG_RIP)==STOP
    call(0xb51a14,HEAP)
    # Float4x4 follows the four leading scale/padding values in the upload.
    assert abs(constants[0]-2/raster[0])<1e-6
    assert abs(constants[5]+2/raster[1])<1e-6
    assert {(round(vertices[i]),round(vertices[i+1])) for i in range(0,24,6)}=={
        (0,0),(raster[0],0),(0,raster[1]),raster}
    assert {(vertices[i+4],vertices[i+5]) for i in range(0,24,6)}=={(0.,0.),(1.,0.),(0.,1.),(1.,1.)}
    # Native kind-0 binder chooses viewport from the output texture descriptor,
    # independently of the quad's rectangle and internal raster dimensions.
    write(BASE+0x1b85e78,"Q",HEAP+0x2000)
    write(HEAP+0x2000,"Q",HEAP+0x3000)
    write(HEAP+0x3000+0x38,"Q",STOP+0x100)
    write(HEAP+0x4000,"Q",HEAP+0x5000)
    write(HEAP+0x5000+0x50,"Q",STOP+0x200)
    write(HEAP+0x5000+0x10,"Q",STOP+0x300)
    write(BASE+0x2ea2d30,"Q",HEAP+0x6000)
    write(HEAP+0x6000,"Q",HEAP+0x7000)
    write(HEAP+0x7000+0x160,"Q",STOP+0x400)
    call(0xb51670)
    assert viewports==[(0.,0.,float(output[0]),float(output[1]),0.,1.)],viewports
    return dict(raster=raster,output=output,full_uv=True,output_viewport=viewports[0])


if __name__=="__main__":
    raw=(ROOT/"out/deps/re-tools/inputs/halo1.dll").read_bytes()
    assert hashlib.sha256(raw).hexdigest().upper()==SHA
    image=pefile.PE(data=raw,fast_load=True).get_memory_mapped_image()
    cases=[run(image,raster,output) for raster,output in (
        ((3788,2732),(3786,2730)),((4368,3152),(4368,3150)),
        ((2912,2100),(2912,2100)),((1920,1080),(2560,1440)),
        ((3840,2160),(1920,1080)))]
    print(json.dumps(dict(status="PASS_NATIVE_EMULATION_ONLY",cases=cases,
        limit="Native quad geometry/UV/constants and output viewport execute; driver/draw calls are fixtures."),indent=2))
