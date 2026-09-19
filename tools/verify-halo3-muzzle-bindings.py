"""Verify H3EK-matched muzzle and FP owner consumers in the pinned retail PE."""
import hashlib
from pathlib import Path
import re
import struct
import sys
root=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'out/pydeps'))
import pefile
raw=(root/'out/deps/re-tools/inputs/halo3.dll').read_bytes()
assert hashlib.sha256(raw).hexdigest().upper()=='B209D8454B12DC77E54CCD2C9924EC8D44B8619D21CF98E36FFAF601E67EFB63'
pe=pefile.PE(data=raw);image=pe.get_memory_mapped_image()
source=(root/'src/dll/halo3_muzzle_lifecycle.inl').read_text()
bindings=re.findall(r'\{0x([0-9A-F]+),"([0-9A-F ]+)"\}',source)
assert len(bindings)==3
for rva,pattern in bindings:
    assert [m.start() for m in re.finditer(re.escape(bytes.fromhex(pattern)),image)]==[int(rva,16)]
call=0x36864C
assert image[call]==0xE8 and call+5+struct.unpack_from('<i',image,call+1)[0]==0x343D74
assert [(e.struct.BeginAddress,e.struct.EndAddress) for e in pe.DIRECTORY_ENTRY_EXCEPTION
        if e.struct.BeginAddress==0x343D74]==[(0x343D74,0x343FF5)]
for address,pattern in {
    0x368631:'41 B9 40 00 00 00',
    0x368637:'40 88 7C 24 28',
    0x36863C:'4C 8D 85 50 05 00 00',
    0x368643:'40 88 7C 24 20',
    0x368898:'4C 6B C0 70',
}.items():
    expected=bytes.fromhex(pattern)
    assert image[address:address+len(expected)]==expected,hex(address)
print('PASS: pinned H3 muzzle identity, three unique bindings, six-argument caller, marker stride and resolver unwind')
