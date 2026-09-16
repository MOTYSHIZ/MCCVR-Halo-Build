"""Execute pinned CE sprite-envelope and alpha instructions, offline only.

Synthetic radius/record inputs; no scene rendering, process access or game writes.
The complete native image is hash checked and mapped read/execute only.
"""
import argparse
import json
import struct
from pathlib import Path
from unicorn.x86_const import (UC_X86_REG_XMM0, UC_X86_REG_XMM10,
    UC_X86_REG_XMM13, UC_X86_REG_XMM14, UC_X86_REG_XMM15,
    UC_X86_REG_RBP, UC_X86_REG_RSI, UC_X86_REG_RBX, UC_X86_REG_R13,
    UC_X86_REG_R14, UC_X86_REG_RAX, UC_X86_REG_RIP)
from verify_ce_native_camera_math import NativeCamera, BASE, SCRATCH


class Envelope(NativeCamera):
    CODE_RANGES = ((0x448144, 0x4481E0), (0x44851C, 0x448573))

    def execute(self, begin, end):
        self.machine.emu_start(BASE+begin, BASE+end, count=200)
        assert self.machine.reg_read(UC_X86_REG_RIP) == BASE+end

    def put_float(self, register, value):
        self.machine.reg_write(register, struct.unpack('<I', struct.pack('<f', value))[0])

    def get_float(self, register):
        return struct.unpack('<f', struct.pack('<I', self.machine.reg_read(register)&0xffffffff))[0]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args=parser.parse_args()
    native=Envelope(args.image)
    machine=native.machine
    sprite, light=SCRATCH+0x2000, SCRATCH+0x3000
    machine.reg_write(UC_X86_REG_RBP,SCRATCH+0x1000)
    machine.reg_write(UC_X86_REG_RSI,sprite)
    machine.reg_write(UC_X86_REG_RBX,0)
    machine.reg_write(UC_X86_REG_R13,light)
    machine.reg_write(UC_X86_REG_R14,0)
    machine.mem_write(sprite+0x24,struct.pack('<f',1))
    machine.mem_write(light+0x1c,struct.pack('<f',.5))
    cases=[]
    for radius in (0,.1,.16,.5,.79,.8,.8001,1,2,1000):
        native.put_float(UC_X86_REG_XMM0,radius)
        native.put_float(UC_X86_REG_XMM14,0)
        native.put_float(UC_X86_REG_XMM15,0)
        native.execute(0x448144,0x4481e0)
        growth=native.get_float(UC_X86_REG_XMM10)
        envelope=native.get_float(UC_X86_REG_XMM13)
        for centered in (False,True):
            machine.mem_write(sprite,struct.pack('<f',0 if centered else .5))
            native.execute(0x44851c,0x448573)
            alpha=machine.reg_read(UC_X86_REG_RAX)
            if radius>.8:
                assert envelope==0 and growth>1
                assert alpha==(127 if centered else 0)
            cases.append(dict(radius=radius,source_centered=centered,
                native_size_multiplier=growth,native_envelope=envelope,native_alpha=alpha))
    assert cases[-1]['native_size_multiplier']>2000
    result=dict(status='PASS_NATIVE_RESIDUAL_FLARE_ENVELOPE',cases=cases,
        instructions=native.instructions,
        limits='Native arithmetic with synthetic inputs; not attribution of a specific live beam or headset acceptance')
    args.output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(dict(status=result['status'],cases=len(cases),instructions=native.instructions)))


if __name__=='__main__':
    main()
