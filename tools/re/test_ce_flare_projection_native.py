"""Execute CE's actual Anniversary lens-flare projection with synthetic lights.

The pinned image is read-only. Native camera rebuild and flare projection run
in Unicorn. Width/height queries and the final sprite draw are modeled leaves;
the latter records the actual projected coordinates rather than rendering.
No DLL is loaded into the host and no game process is used.
"""
import argparse
import json
import math
import struct
from pathlib import Path
from unicorn.x86_const import (
    UC_X86_REG_RAX, UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8,
    UC_X86_REG_RIP, UC_X86_REG_RSP,
)
from verify_ce_native_camera_math import NativeCamera, BASE, CAMERA, RENDERER, SCRATCH


class FlareProjection(NativeCamera):
    CODE_RANGES = NativeCamera.CODE_RANGES + ((0x446F70, 0x447332),)
    def on_instruction(self, machine, address, size, data):
        if address in (SCRATCH+0xF100, SCRATCH+0xF110, BASE+0x447EA0):
            self.instructions += 1
            if address == BASE+0x447EA0:
                pointer=machine.reg_read(UC_X86_REG_RDX)
                stack=machine.reg_read(UC_X86_REG_RSP)
                self.draws.append(dict(
                    point=struct.unpack('<2f',machine.mem_read(pointer,8)),
                    effect=machine.reg_read(UC_X86_REG_RCX),
                    record=machine.reg_read(UC_X86_REG_R8),
                    caller=struct.unpack('<Q',machine.mem_read(stack,8))[0]-BASE))
            else:
                machine.reg_write(UC_X86_REG_RAX, self.width if address==SCRATCH+0xF100 else self.height)
            stack=machine.reg_read(UC_X86_REG_RSP)
            machine.reg_write(UC_X86_REG_RIP,struct.unpack('<Q',machine.mem_read(stack,8))[0])
            machine.reg_write(UC_X86_REG_RSP,stack+8)
            return
        super().on_instruction(machine,address,size,data)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image',type=Path)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    native=FlareProjection(args.image)
    config,surface,vtable,effect=(SCRATCH+x for x in (0x48000,0x49000,0x4A000,0x4B000))
    native.machine.mem_write(BASE+0x1BEA9E0,struct.pack('<Q',RENDERER))
    native.machine.mem_write(BASE+0x2E3BDD8,struct.pack('<Q',config))
    native.machine.mem_write(config+0x318,struct.pack('<Q',surface))
    native.machine.mem_write(surface,struct.pack('<Q',vtable))
    native.machine.mem_write(vtable+0x18,struct.pack('<QQ',SCRATCH+0xF100,SCRATCH+0xF110))
    records=[]
    for width,height in ((2912,2100),(3786,2730),(3788,2732)):
        native.width,native.height=width,height
        for position in ((0.,0.,0.),(5.,7.,9.)):
            for right,forward in (((1,0,0),(0,0,1)),((0,0,-1),(1,0,0))):
                native.build(position,right,(0,1,0),forward,width,height)
                for depth in (10.,1.,.026,.025,.01,0.,-.01,-1.):
                    for enabled in range(4):
                        native.machine.mem_write(effect,bytes(0x110))
                        expected=[]
                        for light in range(2):
                            sign=1 if light==0 else -1
                            point=tuple(position[i]+right[i]*.1*sign+
                                (0,.2*sign,0)[i]+forward[i]*depth for i in range(3))
                            offset=0x70+light*0x28
                            native.machine.mem_write(effect+offset,struct.pack('<3f',*point))
                            native.machine.mem_write(effect+offset+0x1C,
                                struct.pack('<f',1. if enabled&(1<<light) else 0.))
                            if enabled&(1<<light):
                                expected.append((offset,0x44718A if light==0 else 0x4472EB))
                        native.draws=[]
                        native.call(0x446F70,(effect,0x1122334455667788,CAMERA))
                        assert len(native.draws)==len(expected), 'Native intensity dispatch changed'
                        projected=[]
                        for draw,(offset,caller) in zip(native.draws,expected):
                            assert draw['effect']==effect and draw['record']==effect+offset, draw
                            assert draw['caller']==caller, draw
                            screen=draw['point']
                            if depth==0:
                                assert any(not math.isfinite(value) for value in screen), screen
                            if depth in (.01,-.01):
                                assert abs(screen[1]-height*.5)>height, screen
                            projected.append(dict(record_offset=hex(offset),caller=hex(caller),
                                screen=[value if math.isfinite(value) else str(value) for value in screen]))
                        records.append(dict(raster=[width,height],camera=position,forward=forward,depth=depth,
                            enabled=enabled,draws=projected,inside_near_plane=depth<.025))
    result=dict(status='PASS_NATIVE_FLARE_PROJECTION_DOMAIN',cases=len(records),
        native_calls=native.calls,instructions=native.instructions,records=records,
        limits='Synthetic light input and modeled final sprite draw; no live symptom attribution or headset acceptance')
    args.output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:v for k,v in result.items() if k!='records'}))


if __name__=='__main__':
    main()
