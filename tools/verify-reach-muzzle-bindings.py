"""Verify HREK-matched firing, targeting and FP ownership in pinned Reach."""
import hashlib,re,struct,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'out/pydeps'))
import pefile
raw=(root/'out/deps/re-tools/inputs/haloreach.dll').read_bytes()
assert hashlib.sha256(raw).hexdigest()=='738dd2d24ea3aea12e1ee9aa4a61094bf116027d42004c35a19e5048608b0894'
pe=pefile.PE(data=raw);image=pe.get_memory_mapped_image()
source=(root/'src/dll/reach_muzzle_lifecycle.inl').read_text()
bindings=re.findall(r'\{0x([0-9A-F]+),"([0-9A-F ]+)"\}',source)
assert len(bindings)==14
for address,pattern in bindings:
 assert [m.start() for m in re.finditer(re.escape(bytes.fromhex(pattern)),image)]==[int(address,16)],address
edges=re.findall(r'\{0x([0-9A-F]+),0x([0-9A-F]+)\}',source)
assert len(edges)==8
for address,target in edges:
 a=int(address,16);assert image[a]==0xe8 and a+5+struct.unpack_from('<i',image,a+1)[0]==int(target,16),address
for begin in (0x4C2710,0x10E970,0x10FA74,0x47044C,0x484F24):
 assert any(e.struct.BeginAddress==begin for e in pe.DIRECTORY_ENTRY_EXCEPTION),hex(begin)
print('PASS: pinned Reach identity, 14 unique witnesses, eight edges, four optional hooks and shared core entry')
