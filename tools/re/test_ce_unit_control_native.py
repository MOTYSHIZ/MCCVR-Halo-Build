"""Execute pinned CE unit control and camera readers in isolated Unicorn memory.

The native packet writer runs in full. The native output-user angle reader,
angle conversion and on-foot seat transform run in full (only CRT sin/cos are
modeled). Native current-aim, grenade-release argument and motion-formation
slices prove their field handoff; this does not emulate complete animation,
collision/physics or a headset.
No game process is accessed and no game file is written.
"""
import argparse
import hashlib
import math
from pathlib import Path
import struct
import sys

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'out/pydeps'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_64,UC_HOOK_CODE
from unicorn.x86_const import *

SHA='0A12DC561780F449D3F4D0DF10BB8D3BC7BE7840A5BEB2B236F672EB6CD42E6C'
BASE,HEAP,STACK,STOP=0x180000000,0x60000000,0x70000000,0x71000000
OWNER=0x12340007

def require(value,message):
    if not value: raise AssertionError(message)

class Machine:
    def __init__(self,image):
        self.uc=Uc(UC_ARCH_X86,UC_MODE_64)
        self.uc.mem_map(BASE,(len(image)+4095)&~4095);self.uc.mem_write(BASE,image)
        self.uc.mem_map(HEAP,0x30000);self.uc.mem_map(STACK,0x20000);self.uc.mem_map(STOP,0x1000)
        self.headers,self.objects,self.players,self.control,self.mapping=tuple(HEAP+i*0x4000 for i in range(1,6))
        self.unit=self.objects+0x434;self.packet=HEAP+0x18000;self.result=HEAP+0x19000
        self.write(BASE+0x1c42248,'Q',self.headers);self.write(self.headers+0x34,'i',0x100)
        self.write(self.headers+0x100+(OWNER&0xffff)*12+8,'i',0x400)
        self.write(BASE+0x2d9cdf8,'Q',self.objects)
        self.write(self.unit+0x70,'H',0) # biped, native update flags service modeled
        self.write(self.unit+0x1f8,'I',0x45670000) # owned local player
        self.write(self.unit+0x2d0,'h',-1) # on foot: no seat transform
        self.write(BASE+0x1c40480,'Q',self.players);self.write(self.players+0x34,'i',0x100)
        self.write(self.players+0x100+0x64,'I',OWNER)
        self.write(BASE+0x2d8fe70,'Q',self.control)
        self.write(BASE+0x2ea2d90,'Q',self.mapping)
        self.native_flags=[]
        self.uc.hook_add(UC_HOOK_CODE,self.service)
    def write(self,address,fmt,*values): self.uc.mem_write(address,struct.pack('<'+fmt,*values))
    def read(self,address,fmt): return struct.unpack('<'+fmt,self.uc.mem_read(address,struct.calcsize('<'+fmt)))
    def ret(self):
        sp=self.uc.reg_read(UC_X86_REG_RSP)
        self.uc.reg_write(UC_X86_REG_RIP,self.read(sp,'Q')[0]);self.uc.reg_write(UC_X86_REG_RSP,sp+8)
    def service(self,uc,address,size,_):
        if address in (BASE+0x1607023,BASE+0x1607029):
            number=struct.unpack('<f',struct.pack('<I',uc.reg_read(UC_X86_REG_XMM0)&0xffffffff))[0]
            value=(math.cos if address==BASE+0x1607023 else math.sin)(number)
            uc.reg_write(UC_X86_REG_XMM0,struct.unpack('<I',struct.pack('<f',value))[0]);self.ret()
        elif address==BASE+0xc09fac:
            self.native_flags.append((uc.reg_read(UC_X86_REG_RCX),uc.reg_read(UC_X86_REG_RDX)))
            self.ret()
    def call(self,rva,**registers):
        sp=STACK+0x1ff08;self.write(sp,'Q',STOP);self.uc.reg_write(UC_X86_REG_RSP,sp)
        for register,value in registers.items(): self.uc.reg_write(globals()['UC_X86_REG_'+register],value)
        self.uc.emu_start(BASE+rva,STOP,count=10000)
        require(self.uc.reg_read(UC_X86_REG_RIP)==STOP,('native call did not return',hex(rva)))

def packet_case(image,network,client,yaw,pitch):
    machine=Machine(image);u=machine.unit;p=machine.packet
    raw=bytearray((i*3+7)&255 for i in range(0x50))
    struct.pack_into('<BBHBBhh',raw,0,3,1,0x2000,0,0xff,2,-1)
    facing=(math.cos(yaw),math.sin(yaw),0)
    aiming=(math.cos(pitch),0,math.sin(pitch))
    looking=(math.cos(yaw)*math.cos(pitch),math.sin(yaw)*math.cos(pitch),math.sin(pitch))
    for offset,vector in ((0xc,(.8,-.6,.2)),(0x1c,facing),(0x28,aiming),(0x34,looking)):
        struct.pack_into('<3f',raw,offset,*vector)
    machine.uc.mem_write(p,bytes(raw));machine.write(BASE+0x1b7b630,'B',network)
    machine.write(BASE+0x2b236e0,'H',2 if network else 0)
    machine.call(0xafe098,RCX=OWNER,RDX=p,R8=client&0xffffffff)
    for source,target,length in ((0xc,0x258,16),(0x1c,0x204,12),(0x28,0x210,12),
                                 (0x34,0x234,12),(0x40,0x1e0,16)):
        require(machine.uc.mem_read(u+target,length)==raw[source:source+length],('native packet field',hex(source)))
    require(machine.read(u+0x1d8,'I')[0]==0x2000,'grenade action flags changed')
    require(machine.read(u+0x2d7,'B')[0]==2,'native desired grenade selection')
    require(machine.read(u+0x301,'B')[0]==255,'native zoom retained')
    require(machine.uc.mem_read(p,len(raw))==raw,'native input packet unexpectedly mutated')
    if network: require(machine.uc.mem_read(u+0x478,0x50)==raw,'native network packet history')
    require(bool(machine.read(u+0x1d4,'I')[0]&0x10000000)==(client!=-1),'client update flag ABI')
    if client!=-1: require(machine.read(u+0x4c8,'I')[0]==client,'client update id ABI')
    require(machine.native_flags==[(OWNER,0x18000)],'native biped update notification')
    # Run the native zero-rate aiming branch. Its source is desired aim+0x210,
    # destination current aim+0x21c; other rate branches retain native interpolation.
    machine.uc.reg_write(UC_X86_REG_R12,machine.objects)
    machine.uc.reg_write(UC_X86_REG_R15,0x400);machine.uc.reg_write(UC_X86_REG_R14,0x400)
    machine.uc.reg_write(UC_X86_REG_RDI,0xffffffff);machine.uc.reg_write(UC_X86_REG_RBX,u+0x21c)
    machine.uc.emu_start(BASE+0xafce44,BASE+0xafce6c,count=50)
    require(machine.uc.mem_read(u+0x21c,12)==raw[0x28:0x34],'desired/current aim handoff')
    # Execute the actual release callsite up to its native random-cone service.
    sp=STACK+0x10000;machine.uc.reg_write(UC_X86_REG_RSP,sp)
    machine.uc.reg_write(UC_X86_REG_RBP,sp+0x200);machine.uc.reg_write(UC_X86_REG_RDI,u)
    machine.uc.emu_start(BASE+0xb0b36f,BASE+0xb89b4c,count=50)
    release_sp=machine.uc.reg_read(UC_X86_REG_RSP)
    aim_pointer=machine.read(release_sp+0x28,'Q')[0]
    require(aim_pointer==u+0x21c and machine.uc.mem_read(aim_pointer,12)==raw[0x28:0x34],
            'native grenade release must consume unit current controller aim')
    # The same altered unit vectors must not rotate the native base camera.
    # Native output-user reader obtains yaw/pitch from its separate 0x38 state.
    for user in range(4):
        camera_yaw,camera_pitch=.3+user*.2,-.1
        machine.write(machine.mapping+0xb8+user*4,'I',0x45670000)
        machine.write(machine.control+0x17c+user*0x38,'2f',camera_yaw,camera_pitch)
        machine.call(0xa999e8,RCX=user,RDX=machine.result)
        expected=(math.cos(camera_yaw)*math.cos(camera_pitch),math.sin(camera_yaw)*math.cos(camera_pitch),math.sin(camera_pitch))
        require(all(abs(a-b)<1e-6 for a,b in zip(machine.read(machine.result,'3f'),expected)),
                'native camera must remain independent of unit head/controller vectors')
    return 1

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--image',type=Path,default=ROOT/'out/deps/re-tools/inputs/halo1.dll')
    args=parser.parse_args();raw=args.image.read_bytes()
    require(hashlib.sha256(raw).hexdigest().upper()==SHA,'pinned CE image mismatch')
    image=pefile.PE(data=raw,fast_load=True).get_memory_mapped_image();count=0
    for network in (0,1):
        for client in (-1,42):
            for yaw,pitch in ((0,0),(math.pi/2,.4),(-math.pi/2,-.4),(.78,.9)):
                count+=packet_case(image,network,client,yaw,pitch)
    movement_cases=0
    for yaw in (-math.pi/2,-math.pi/4,0,math.pi/4,math.pi/2):
        for throttle in ((1,0,.2),(0,1,.2),(.6,-.8,.2),(1,1,.2)):
            machine=Machine(image);motion=HEAP+0x20000
            machine.write(motion,'I',OWNER);machine.write(motion+4,'H',0x20)
            basis=(math.cos(yaw),math.sin(yaw),0)
            machine.write(motion+0x14,'3f',*basis)
            machine.write(motion+0x20,'3f',0,-1,0) # deliberately different current aim
            machine.write(motion+0x3c,'3f',*throttle)
            before=machine.uc.mem_read(motion,0xb8)
            machine.uc.reg_write(UC_X86_REG_RSP,STACK+0x1ff08)
            machine.uc.reg_write(UC_X86_REG_RCX,motion)
            machine.uc.emu_start(BASE+0xbb5d88,BASE+0xbb674e,count=500)
            expected=(basis[0]*throttle[0]-basis[1]*throttle[1],
                      basis[1]*throttle[0]+basis[0]*throttle[1],throttle[2])
            require(all(abs(a-b)<1e-6 for a,b in zip(machine.read(motion+0xb8,'3f'),expected)),
                    'native movement forward/LEFT basis and diagonal magnitude')
            require(machine.uc.mem_read(motion+0x14,24)==before[0x14:0x2c],
                    'native movement basis fields are inputs, not outputs')
            movement_cases+=1
    print(f'PASS: {count} native CE packet writes, current-aim/grenade callsite handoffs and {count*4} independent camera headings.')
    print(f'PASS: {movement_cases} native motion-consumer forward/left/diagonal cases; input basis preserved.')
    print('LIMIT: CRT trig and biped update notification are modeled; movement stops before collision. Aim-rate animation, complete grenade flight and headset behavior remain untested.')

if __name__=='__main__': main()
