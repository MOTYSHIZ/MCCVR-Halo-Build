"""Execute CE's RGB-only HUD blend selection in the hash-pinned retail image.

No game process, game writes or runtime shader replacement. The bitmap entry,
native render-state table/byte writer, blend flush and descriptor creation all
execute native instructions. D3D endpoints, blend-cache storage and the compiler
cookie/memset services are fixture boundaries. The emitted D3D11_BLEND_DESC can
then drive the production shader WARP fixture.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'out/pydeps'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_64,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_RSP,UC_X86_REG_RIP,UC_X86_REG_RCX,UC_X86_REG_RDX,UC_X86_REG_R8,UC_X86_REG_R9,UC_X86_REG_RAX

SHA='0A12DC561780F449D3F4D0DF10BB8D3BC7BE7840A5BEB2B236F672EB6CD42E6C'
BASE,HEAP,STACK,STOP=0x180000000,0x60000000,0x70000000,0x71000000

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--image',type=Path,default=ROOT/'out/deps/re-tools/inputs/halo1.dll')
    parser.add_argument('--descriptor',type=Path,default=ROOT/'out/ce-reticle-native-blend-20260915.bin')
    parser.add_argument('--shader-exe',type=Path)
    args=parser.parse_args()
    raw=args.image.read_bytes()
    assert hashlib.sha256(raw).hexdigest().upper()==SHA,'CE pinned image mismatch'
    pe=pefile.PE(data=raw,fast_load=True)
    mapped=pe.get_memory_mapped_image()
    uc=Uc(UC_ARCH_X86,UC_MODE_64)
    uc.mem_map(BASE,(len(mapped)+4095)&~4095);uc.mem_write(BASE,mapped)
    uc.mem_map(HEAP,0x20000);uc.mem_map(STACK,0x20000);uc.mem_map(STOP,0x1000)
    def write(at,fmt,*values):uc.mem_write(at,struct.pack(fmt,*values))
    def read(at,fmt):return struct.unpack(fmt,uc.mem_read(at,struct.calcsize(fmt)))
    def ret():
        sp=uc.reg_read(UC_X86_REG_RSP)
        uc.reg_write(UC_X86_REG_RIP,read(sp,'<Q')[0]);uc.reg_write(UC_X86_REG_RSP,sp+8)
    instructions=0;phase='bitmap';created=[];bound=[]
    device,context,devicevt,contextvt=HEAP+0x1000,HEAP+0x2000,HEAP+0x3000,HEAP+0x4000
    def service(machine,address,size,_):
        nonlocal instructions
        instructions+=1
        if address==STOP:machine.emu_stop();return
        if phase=='bitmap' and address==BASE+0xb0ecf4:
            # Immediately after native A8=7 setter. No bitmap data is invented.
            machine.emu_stop();return
        if address==BASE+0x1391640:ret();return
        if address==BASE+0x139362f:
            target=machine.reg_read(UC_X86_REG_RCX);value=machine.reg_read(UC_X86_REG_RDX)&255
            count=machine.reg_read(UC_X86_REG_R8)
            assert count<=0x1000
            machine.mem_write(target,bytes([value])*count);machine.reg_write(UC_X86_REG_RAX,target);ret();return
        if address==BASE+0x5b2c0:
            # Blend cache lookup: native descriptor construction is untouched.
            write(machine.reg_read(UC_X86_REG_RDX),'<Q',0);ret();return
        if address==BASE+0x5c280:ret();return
        if address==STOP+0x100:
            assert machine.reg_read(UC_X86_REG_RCX)==device
            created.append(bytes(machine.mem_read(machine.reg_read(UC_X86_REG_RDX),264)))
            write(machine.reg_read(UC_X86_REG_R8),'<Q',HEAP+0x5000)
            machine.reg_write(UC_X86_REG_RAX,0);ret();return
        if address==STOP+0x200:
            assert machine.reg_read(UC_X86_REG_RCX)==context
            bound.append(machine.reg_read(UC_X86_REG_RDX));ret();return
    uc.hook_add(UC_HOOK_CODE,service)
    write(BASE+0x1b7d472,'<B',1);write(BASE+0x29e0580,'<I',1)
    # Exercise native bitmap setup even if the prior unrelated pass wrote RGBA.
    write(BASE+0x1c53780,'<I',15)
    write(BASE+0x29e05b4,'<4h',0,0,1080,1920)
    def call(entry):
        sp=STACK+0x1f008;write(sp,'<Q',STOP);uc.reg_write(UC_X86_REG_RSP,sp)
        uc.emu_start(BASE+entry,STOP,count=100000)
    call(0xb0ec20)
    assert read(BASE+0x1c532ac,'<B')[0]==7,'Bitmap native RGB mask missing'
    assert read(BASE+0x1c53280,'<B')[0]==1,'Blend descriptor was not dirtied'
    table=read(BASE+0x1b85b78,'<I4xQQQQ')
    assert table==(0xa8,BASE+0x1c532ac,BASE+0xb3e224,BASE+0xb3e228,BASE+0x1c53280)
    phase='flush'
    write(BASE+0x2ea2d20,'<Q',device);write(BASE+0x2ea2d30,'<Q',context)
    write(device,'<Q',devicevt);write(context,'<Q',contextvt)
    write(devicevt+0xa0,'<Q',STOP+0x100);write(contextvt+0x118,'<Q',STOP+0x200)
    write(BASE+0x2b04ee4,'<I',1);write(BASE+0x2b04ee8,'<Q',HEAP+0x6000)
    # Initialize only valid baseline blend factors; the native mask is retained.
    write(BASE+0x1c53294,'<II',2,1);write(BASE+0x1c5329c,'<I',1)
    write(BASE+0x1c532a0,'<III',2,1,1)
    write(BASE+0x1c53281,'<BBB',0,0,0);write(BASE+0x1c53284,'<I',0)
    call(0xb3db28)
    assert len(created)==1 and created[0][36]==7
    assert bound==[HEAP+0x5000] and read(BASE+0x1c53280,'<B')[0]==0
    args.descriptor.parent.mkdir(parents=True,exist_ok=True)
    args.descriptor.write_bytes(created[0])
    result={'result':'PASS_NATIVE_BLEND_ONLY','image_sha256':SHA,'instructions':instructions,
        'bitmap_entry':'0xB0EC20','render_state':168,'render_target_write_mask':created[0][36],
        'descriptor':str(args.descriptor),'native_bind_count':len(bound),
        'limits':'Bitmap art, weapon tag selection and native gameplay are not executed.'}
    if args.shader_exe:
        run=subprocess.run([str(args.shader_exe),str(args.descriptor)],check=True,capture_output=True,text=True)
        result['warp']=run.stdout.strip()
    print(json.dumps(result,indent=2))

if __name__=='__main__':main()
