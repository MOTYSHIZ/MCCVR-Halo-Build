"""Run pinned CE native angle-update and salted-datum code in isolated memory.

No game process access or game-file writes. The on-foot seat-description call
is explicitly stubbed with no parent/seat/constraint; all native yaw wrapping,
pitch clamping, per-user addressing and datum lookup instructions execute.
This is ABI/state evidence, not a runtime or headset result.
"""
import argparse
import hashlib
import math
from pathlib import Path
import struct
import sys

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/"out/pydeps"))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_64,UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_RSP,UC_X86_REG_RIP,UC_X86_REG_RCX,
    UC_X86_REG_RDX,UC_X86_REG_RAX,UC_X86_REG_XMM1,UC_X86_REG_XMM2)

PINNED_SHA="0A12DC561780F449D3F4D0DF10BB8D3BC7BE7840A5BEB2B236F672EB6CD42E6C"
BASE,HEAP,STACK,STOP=0x180000000,0x60000000,0x70000000,0x71000000
LIMIT=1.4922565

def require(condition,message):
    if not condition: raise AssertionError(message)

def machine(image):
    uc=Uc(UC_ARCH_X86,UC_MODE_64)
    uc.mem_map(BASE,(len(image)+4095)&~4095);uc.mem_write(BASE,image)
    uc.mem_map(HEAP,0x20000);uc.mem_map(STACK,0x20000);uc.mem_map(STOP,0x1000)
    return uc

def u64(uc,address,value): uc.mem_write(address,struct.pack("<Q",value))
def f32(uc,address,value): uc.mem_write(address,struct.pack("<f",value))
def read_float(uc,address): return struct.unpack("<f",uc.mem_read(address,4))[0]
def xmm(uc,register,value): uc.reg_write(register,struct.unpack("<I",struct.pack("<f",value))[0])
def call(uc,rva):
    sp=STACK+0x1ff08;u64(uc,sp,STOP);uc.reg_write(UC_X86_REG_RSP,sp)
    uc.emu_start(BASE+rva,STOP,count=20000)
    require(uc.reg_read(UC_X86_REG_RIP)==STOP,"Native execution did not return")

def angle_case(image,user,yaw,delta,pitch,pitch_delta):
    uc=machine(image)
    control,clock,globals_tag=HEAP+0x1000,HEAP+0x2000,HEAP+0x3000
    u64(uc,BASE+0x2d8fe70,control);u64(uc,BASE+0x2e9fd68,clock)
    u64(uc,BASE+0x1c55c60,globals_tag);f32(uc,clock+0x28,1)
    for index in range(4):
        at=control+index*0x58
        uc.mem_write(at+0x10,struct.pack("<I",0x12340007+index))
        f32(uc,at+0x1c,yaw if index==user else .8)
        f32(uc,at+0x20,pitch if index==user else .3)
        f32(uc,at+0x54,-LIMIT);f32(uc,at+0x58,LIMIT)
    def service(machine,address,size,_):
        if address==BASE+0xb04ee4:
            # Native caller initializes this record with its full unit handle,
            # seat NONE and null seat definition. Model the on-foot result.
            ptr=machine.reg_read(UC_X86_REG_RCX)
            machine.mem_write(ptr+4,struct.pack("<h",-1));u64(machine,ptr+8,0)
            sp=machine.reg_read(UC_X86_REG_RSP)
            machine.reg_write(UC_X86_REG_RIP,struct.unpack("<Q",machine.mem_read(sp,8))[0])
            machine.reg_write(UC_X86_REG_RSP,sp+8)
    uc.hook_add(UC_HOOK_CODE,service)
    uc.reg_write(UC_X86_REG_RCX,user);xmm(uc,UC_X86_REG_XMM1,delta);xmm(uc,UC_X86_REG_XMM2,pitch_delta)
    call(uc,0xa99c1c)
    expected_yaw=(yaw+delta)%(2*math.pi)
    expected_pitch=max(-LIMIT,min(LIMIT,pitch+pitch_delta))
    at=control+user*0x58
    actual=(read_float(uc,at+0x1c),read_float(uc,at+0x20))
    require(abs(actual[0]-expected_yaw)<1e-5 and abs(actual[1]-expected_pitch)<1e-5,
        ("native delta",user,actual,(expected_yaw,expected_pitch)))
    for index in range(4):
        if index!=user:
            require(abs(read_float(uc,control+index*0x58+0x1c)-.8)<1e-5,"Other user's yaw changed")
            require(abs(read_float(uc,control+index*0x58+0x20)-.3)<1e-5,"Other user's pitch changed")

def datum_cases(image):
    uc=machine(image);table=HEAP+0x1000
    uc.mem_write(table+0x20,struct.pack("<HH",4,0xc20))
    uc.mem_write(table+0x34,struct.pack("<i",0x100))
    ptr=table+0x100+0xc20;uc.mem_write(ptr,struct.pack("<H",0x1234))
    cases=((0x12340001,ptr),(0x33330001,0),(0xffffffff,0),(0x1234ffff,0),
        (0x12340004,0),(0x00000001,ptr),(0x12340002,0))
    for datum,expected in cases:
        uc.reg_write(UC_X86_REG_RCX,table);uc.reg_write(UC_X86_REG_RDX,datum);call(uc,0xbbb8d0)
        require(uc.reg_read(UC_X86_REG_RAX)==expected,("native datum",hex(datum)))
    return len(cases)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image",type=Path,default=ROOT/"out/deps/re-tools/inputs/halo1.dll")
    args=parser.parse_args();raw=args.image.read_bytes()
    require(hashlib.sha256(raw).hexdigest().upper()==PINNED_SHA,"Pinned CE SHA mismatch")
    image=pefile.PE(data=raw,fast_load=True).get_memory_mapped_image();count=0
    for user in range(4):
        for yaw,delta in ((.1,-.8),(6.1,.8),(1,.02),(1,-.02)):
            for pitch,pitch_delta in ((.2,0),(1.4,.5)):
                angle_case(image,user,yaw,delta,pitch,pitch_delta);count+=1
    datums=datum_cases(image)
    print(f"PASS: {count} native CE angular-delta cases across 4 users; {datums} native salt/index cases.")
    print("LIMIT: on-foot seat resolution is stubbed; complete input-loop lifetime and headset feel remain untested.")

if __name__=="__main__": main()
