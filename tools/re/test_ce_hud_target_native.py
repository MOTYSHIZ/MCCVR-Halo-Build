"""Execute pinned CE target binding/accessor/viewport instructions in Unicorn.

No process access or game writes. Only D3D setter calls and the security-cookie
service are stubbed. Native descriptor copying, view selection, target-cache
publication, dimensions and raster state construction execute from the PE.
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
    UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_R9, UC_X86_REG_RAX)

SHA = "0A12DC561780F449D3F4D0DF10BB8D3BC7BE7840A5BEB2B236F672EB6CD42E6C"
BASE, HEAP, STACK, STOP = 0x180000000, 0x60000000, 0x70000000, 0x71000000


def run(image, colors, depth, srgb=False, mip=0, face=0, layer=0, default=False, invalid=False, replay=False):
    uc = Uc(UC_ARCH_X86, UC_MODE_64)
    uc.mem_map(BASE, (len(image)+4095)&~4095)
    uc.mem_write(BASE, image)
    uc.mem_map(HEAP, 0x200000)
    uc.mem_map(STACK, 0x20000)
    uc.mem_map(STOP, 0x1000)
    backend, descriptor, context, vtable = HEAP+0x10000, HEAP+0x90000, HEAP+0xa0000, HEAP+0xa1000
    viewport_calls, scissor_calls, om_calls = [], [], []
    replay_calls=[]
    def write(at, fmt, *values): uc.mem_write(at, struct.pack(fmt, *values))
    def read(at, fmt): return struct.unpack(fmt, uc.mem_read(at, struct.calcsize(fmt)))
    def ret():
        sp = uc.reg_read(UC_X86_REG_RSP)
        uc.reg_write(UC_X86_REG_RIP, read(sp,"<Q")[0])
        uc.reg_write(UC_X86_REG_RSP, sp+8)
    def service(machine, address, size, _):
        if address == STOP: machine.emu_stop()
        elif address == BASE+0x1391640: ret()
        elif address == STOP+0x100:
            assert machine.reg_read(UC_X86_REG_RCX)==context
            count=machine.reg_read(UC_X86_REG_RDX)
            viewport_calls.append([read(machine.reg_read(UC_X86_REG_R8)+24*i,"<6f") for i in range(count)])
            ret()
        elif address == STOP+0x200:
            count=machine.reg_read(UC_X86_REG_RDX)
            scissor_calls.append([read(machine.reg_read(UC_X86_REG_R8)+16*i,"<4i") for i in range(count)])
            ret()
        elif address == STOP+0x300:
            count=machine.reg_read(UC_X86_REG_RDX)
            om_calls.append((count,read(machine.reg_read(UC_X86_REG_R8),"<"+"Q"*count),
                             machine.reg_read(UC_X86_REG_R9)))
            ret()
        elif address == STOP+0x400:
            # Fixture boundary only: this is not emulation of gameplay HUD.
            # The actual preamble has balanced its inner push/pop here.
            assert read(backend+0x10,"<i")[0]==2
            replay_calls.append(uc.mem_read(backend+0x18,0x48))
            # Simulate the callback selecting a different owned depth variant.
            write(backend+0x18+0x44,"<B",int(not srgb))
            ret()
    uc.hook_add(UC_HOOK_CODE,service)
    write(backend,"<Q",BASE+0x17f9d10)
    write(backend+0xce0,"<Q",context)
    write(context,"<Q",vtable)
    for slot,offset in [(44,0x100),(45,0x200),(33,0x300)]: write(vtable+8*slot,"<Q",STOP+offset)
    parent, constant, data = HEAP+0xa2000, HEAP+0xa3000, HEAP+0xa4000
    write(BASE+0x2e3bdd8,"<Q",parent);write(parent+0x114,"<I",2)
    write(backend+0xd8,"<Q",constant);write(constant+8,"<Q",data)
    expected_views=[]
    index=(6*layer+face)*2+mip
    def wrapper(slot,is_depth=False):
        value=HEAP+0xb0000+slot*0x300
        table=HEAP+0xc0000+slot*0x400
        write(value,"<Q",BASE+0x17fb608)
        write(value+0x10,"<hh",512,256);write(value+0x1a,"<b",2)
        flags=(1<<9) if is_depth else (1<<8)|(1<<12)
        write(value+0x88,"<I",0 if invalid and slot==0 else flags)
        write(value+0xe0,"<Q",HEAP+0x110000+slot*0x100)
        write(value+0xe8,"<Q",table);write(value+0xf0,"<I",24);write(value+0xf8,"<Q",table+0x100)
        for i in range(24):
            write(table+i*8,"<Q",0x72000000+slot*0x10000+i*0x10)
            write(table+0x100+i*8,"<Q",0x73000000+slot*0x10000+i*0x10)
        write(value+0x108,"<Q",0x74000000);write(value+0x110,"<Q",0x74000100)
        return value
    for slot in range(colors):
        value=wrapper(slot)
        if default:
            assert colors==1
            write(backend+0xcf0,"<Q",value)
        else: write(descriptor+0x10+slot*8,"<Q",value)
        write(descriptor+slot*4,"<I",(2 if default else 0)|(64 if srgb else 0))
        expected_views.append((0x73000000 if srgb else 0x72000000)+slot*0x10000+(0 if default else index)*0x10)
    if depth: write(descriptor+0x30,"<Q",wrapper(4,True))
    write(descriptor+0x40,"<hbbB",mip,face,layer,int(srgb))
    for attempt in range(2):
        sp=STACK+0x1ff08;write(sp,"<Q",STOP)
        uc.reg_write(UC_X86_REG_RSP,sp)
        uc.reg_write(UC_X86_REG_RCX,backend);uc.reg_write(UC_X86_REG_RDX,descriptor)
        uc.emu_start(BASE+0x205e40,STOP,count=100000)
        assert bool(uc.reg_read(UC_X86_REG_RAX)&0xff)==(not invalid)
    if invalid:
        assert not om_calls and not viewport_calls and not scissor_calls
    else:
        expected_depth=(0x74000100 if srgb else 0x74000000) if depth else 0
        assert len(om_calls)==2 and all(call==(colors,tuple(expected_views),expected_depth) for call in om_calls),om_calls
        width,height=((512>>mip,256>>mip) if colors and not default else (512,256))
        assert viewport_calls==[[(0.0,0.0,float(width),float(height),0.0,1.0)]]*2,viewport_calls
        assert scissor_calls==[[(0,0,width,height)]]*2,scissor_calls
        assert read(backend+0xcf8,"<I")[0]==colors
        assert read(backend+0xd20,"<Q")[0]==expected_depth
        assert uc.mem_read(backend+0x18,0x38)==uc.mem_read(descriptor,0x38)
        assert read(backend+0x50,"<II")== (width,height)
    if replay:
        assert not invalid
        original=bytes(uc.mem_read(backend+0x18,0x48))
        storage=HEAP+0x150000
        write(BASE+0x2e3bde0,"<Q",backend)
        write(BASE+0x1c33fe0,"<Q",STOP+0x400)
        write(backend+8,"<Qii",storage,1,4)
        uc.mem_write(storage,original)
        for eye in range(2):
            for target in [0x1dc1e0,0x4255a0,0x1dc2f0]:
                sp=STACK+0x1ff08;write(sp,"<Q",STOP)
                uc.reg_write(UC_X86_REG_RSP,sp)
                uc.emu_start(BASE+target,STOP,count=100000)
            assert uc.reg_read(UC_X86_REG_RAX)&0xff
            assert read(backend+0x10,"<i")[0]==1
            assert read(backend+8,"<Q")[0]==storage
            assert bytes(uc.mem_read(backend+0x18,0x48))==original
        assert len(replay_calls)==2 and all(bytes(value)==original for value in replay_calls)
        assert len(om_calls)==6,om_calls
    return dict(colors=colors,depth=depth,srgb=srgb,mip=mip,face=face,layer=layer,default=default,invalid=invalid,replay=replay,bind_calls=len(om_calls),replay_callbacks=len(replay_calls))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image",type=Path,default=ROOT/"out/deps/re-tools/inputs/halo1.dll")
    args=parser.parse_args()
    raw=args.image.read_bytes()
    if hashlib.sha256(raw).hexdigest().upper()!=SHA: raise SystemExit("Pinned CE image hash mismatch")
    image=pefile.PE(data=raw,fast_load=True).get_memory_mapped_image()
    cases=[run(image,1,False),run(image,1,True),run(image,4,True),
           run(image,1,True,srgb=True,mip=1,face=3,layer=1),
           run(image,1,True,default=True),run(image,0,True),run(image,1,False,invalid=True),
           run(image,1,True,replay=True)]
    print(json.dumps(dict(status="PASS_NATIVE_EMULATION_ONLY",cases=cases,
        limit="D3D setters/security-cookie service and replay gameplay callback are stubs; native push/pop/preamble/binder execute pinned code. No MCC or headset acceptance."),indent=2))

if __name__=="__main__": main()
