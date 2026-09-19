"""Verify H4EK-matched firing/targeting witnesses in the pinned Halo 4 module."""
import hashlib,re,struct,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'out/pydeps'))
import pefile
raw=(root/'out/deps/re-tools/inputs/halo4.dll').read_bytes()
assert hashlib.sha256(raw).hexdigest()=='7c53e7d5bc9848545a1b70e2768242479336fba1b7630d7ab955f7fd0c34fa84'
pe=pefile.PE(data=raw);image=pe.get_memory_mapped_image()
source=(root/'src/dll/halo4_muzzle_lifecycle.inl').read_text()
bindings=re.findall(r'\{0x([0-9A-F]+),"([0-9A-F ]+)"\}',source)
assert len(bindings)==14
for address,pattern in bindings:
    assert [m.start() for m in re.finditer(re.escape(bytes.fromhex(pattern)),image)]==[int(address,16)],address
edges=re.findall(r'\{0x([0-9A-F]+),0x([0-9A-F]+)\}',source)
assert len(edges)==10
for address,target in edges:
    a=int(address,16)
    assert image[a]==0xe8 and a+5+struct.unpack_from('<i',image,a+1)[0]==int(target,16),address
for begin in (0x6176B8,0x1DB840,0x1DC9E4,0x5D5B74,0x5F3510):
    assert any(e.struct.BeginAddress==begin for e in pe.DIRECTORY_ENTRY_EXCEPTION),hex(begin)
print('PASS: pinned H4 identity, 14 unique witnesses, ten edges and five optional hook entries')
