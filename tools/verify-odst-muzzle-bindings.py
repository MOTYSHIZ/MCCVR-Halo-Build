"""Verify ODSTEK-matched firing, targeting and FP ownership in pinned ODST."""
import hashlib,re,struct,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'out/pydeps'))
import pefile
raw=(root/'out/deps/re-tools/inputs/halo3odst.dll').read_bytes()
assert hashlib.sha256(raw).hexdigest()=='5bb20976efdfd9e1ce59c589339804725fec239021027c8d65b2733eab94829a'
pe=pefile.PE(data=raw);image=pe.get_memory_mapped_image()
source=(root/'src/dll/odst_muzzle_lifecycle.inl').read_text()
bindings=re.findall(r'\{0x([0-9A-F]+),"([0-9A-F ]+)"\}',source)
assert len(bindings)==16
for address,pattern in bindings:
 assert [m.start() for m in re.finditer(re.escape(bytes.fromhex(pattern)),image)]==[int(address,16)],address
edges=re.findall(r'\{0x([0-9A-F]+),0x([0-9A-F]+)\}',source)
assert len(edges)==8
for address,target in edges:
 a=int(address,16);assert image[a]==0xe8 and a+5+struct.unpack_from('<i',image,a+1)[0]==int(target,16),address
for begin in (0x3AF230,0x396B7C,0x1604E0,0x160FA0,0x242340,0x37F514):
 assert any(e.struct.BeginAddress==begin for e in pe.DIRECTORY_ENTRY_EXCEPTION),hex(begin)
for address,pattern in {
 0x3AF4AA:'41 B9 40 00 00 00',0x3AF4B0:'40 88 7C 24 28',
 0x3AF4B5:'4C 8D 85 70 04 00 00',0x3AF4BC:'40 88 7C 24 20',
}.items():
 expected=bytes.fromhex(pattern);assert image[address:address+len(expected)]==expected,hex(address)
print('PASS: pinned ODST identity, 16 unique bindings, eight native edges, six hook entries and own marker ABI')
