"""Verify native CE-to-Saber camera output through production gameplay recovery.

Executes the pinned native producer in isolated memory, then the compiled C++
inverse. No game process, on-disk game write, GPU or headset is involved.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/"out/pydeps"))
from verify_ce_native_camera_math import NativeCamera,rotate


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image",type=Path,default=ROOT/"out/deps/re-tools/inputs/halo1.dll")
    parser.add_argument("--adapter-exe",type=Path,default=ROOT/"out/build/release/Release/halomccvr_ce_render_tests.exe")
    args=parser.parse_args()
    native=NativeCamera(args.image)
    records=[]
    expected=[]
    for position in ((0,0,0),(32.45,-99.76,59.27),(3300,-820,230)):
        for yaw,pitch in ((0,0),(.71,-.32),(-1.95,.63)):
            forward,up=(1,0,0),(0,0,1)
            for axis,angle in (((0,0,1),yaw),((0,1,0),pitch)):
                forward,up=rotate(forward,axis,angle),rotate(up,axis,angle)
            for offset,bias in (((0,0,0),0),((12,-3,9),.3),((-100,40,75),-.7)):
                camera=native.produce(position,up,forward,2912,1050,offset,bias)
                records.append(camera+struct.pack("<4f",*offset,bias))
                expected.append((*position,*forward,*up))
    with tempfile.TemporaryDirectory(prefix="ce-native-gameplay-bridge-",dir=ROOT/"out") as scratch:
        source,destination=Path(scratch)/"input.bin",Path(scratch)/"output.bin"
        source.write_bytes(struct.pack("<I",len(records))+b"".join(records))
        subprocess.run([str(args.adapter_exe.resolve()),"--native-gameplay-bridge-fixture",
            str(source),str(destination)],check=True)
        result=destination.read_bytes()
        assert len(result)==4+len(records)*0x54 and struct.unpack_from("<I",result)[0]==len(records)
        maximum_error=0
        for index,values in enumerate(expected):
            actual=struct.unpack_from("<9f",result,4+index*0x54)
            error=max(abs(a-b) for a,b in zip(actual,values))
            assert error<.002,(index,actual,values,error)
            maximum_error=max(maximum_error,error)
    print(json.dumps(dict(status="PASS_NATIVE_GAMEPLAY_CAMERA_RECOVERY",cases=len(records),
        native_instructions=native.instructions,maximum_error=maximum_error,
        image_sha256=hashlib.sha256(args.image.read_bytes()).hexdigest().upper(),
        adapter_sha256=hashlib.sha256(args.adapter_exe.read_bytes()).hexdigest().upper(),
        scope="Native producer plus compiled production inverse; synthetic camera/global data, no live process or headset."),indent=2))


if __name__=="__main__":
    main()
