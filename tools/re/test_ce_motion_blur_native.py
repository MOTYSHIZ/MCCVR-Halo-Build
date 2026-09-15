"""Execute pinned CE's real motion-blur history updater in isolated memory.

No process attachment, game launch or file mutation. Camera/matrix instructions
execute natively in Unicorn; only the existing CRT math/cookie endpoints are
modeled. Demonstrates the two-eye history conflict, not the failed run's cause.
"""
import argparse
import json
import struct
from pathlib import Path
from verify_ce_native_camera_math import NativeCamera, BASE, CAMERA, RENDERER, SCRATCH, PINNED_SHA256


class NativeHistory(NativeCamera):
    # Matrix inverse spans chained unwind fragments; bound the full instruction
    # body through its disassembled return at FF467, not only fragment one.
    CODE_RANGES = NativeCamera.CODE_RANGES + ((0x446110, 0x44657F), (0xFED50, 0xFF468))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image',type=Path)
    parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    native=NativeHistory(args.image)
    effect=SCRATCH+0x50000
    native.machine.mem_write(BASE+0x1BEA9E0,struct.pack('<Q',RENDERER))
    native.machine.mem_write(RENDERER+0x9C,struct.pack('<I',1))
    records=[]
    for user in (0,1):
        native.machine.mem_write(effect,bytes(0x200))
        left,right=(17.0,4.0,-3.0),(17.07,4.0,-3.0)
        for position in (left,right,left):
            native.build(position,(1,0,0),(0,1,0),(0,0,1))
            native.machine.mem_write(CAMERA+0x220,struct.pack('<I',user))
            before=bytes(native.machine.mem_read(effect,0x200))
            native.call(0x446110,(effect,CAMERA))
            after=bytes(native.machine.mem_read(effect,0x200))
            history=struct.unpack_from('<3f',after,0x78+user*0x7C)
            if any(abs(a-b)>0.0001 for a,b in zip(history,position)):
                raise RuntimeError(f'native history did not store selected camera: {history} vs {position}')
            other=0x18+(1-user)*0x7C
            if before[other:other+0x7C]!=after[other:other+0x7C]:
                raise RuntimeError('native updater changed the other player history')
            records.append({'player':user,'current':position,'previous':struct.unpack_from('<3f',before,0x78+user*0x7C),
                            'stored':history})
    # In the actual one-player pair both cameras retain source-player zero:
    # eye1 inherits eye0, and next eye0 inherits eye1, at a fixed headset pose.
    if records[1]['previous']!=records[0]['stored'] or records[2]['previous']!=records[1]['stored']:
        raise RuntimeError('two-eye native history conflict was not reproduced')
    result={'status':'PASS','pinned_sha256':PINNED_SHA256,'native_calls':native.calls,
            'native_instructions':native.instructions,'history_updates':len(records),'records':records,
            'limit':'isolated native instructions, not live effect activation or headset rendering'}
    if args.output:args.output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(result))


if __name__=='__main__':main()
