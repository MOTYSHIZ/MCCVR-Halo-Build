"""Verify H2EK-matched independent firing against the pinned offline PE."""
import hashlib
from pathlib import Path
import re
import struct
import sys

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / 'out/pydeps'))
import pefile

raw = (root / 'out/deps/re-tools/inputs/halo2.dll').read_bytes()
assert hashlib.sha256(raw).hexdigest().upper() == (
    'DE65B4F4FDBF3F0A5EAB7431FE530DA17DD815599182DFD6AE9B7E21CF171946')
pe = pefile.PE(data=raw)
image = pe.get_memory_mapped_image()
source = (root / 'src/dll/halo2_dual_lifecycle.inl').read_text()
bindings = re.findall(r'\{0x([0-9A-F]+),"([0-9A-F ]+)"\}', source)
assert len(bindings) == 5
for rva, pattern in bindings:
    matches = [m.start() for m in re.finditer(re.escape(bytes.fromhex(pattern)), image)]
    assert matches == [int(rva, 16)], (rva, matches)
# H2 uses chained runtime functions; these are entry records, not the complete
# body extent. Do not interpret their first EndAddress as a function end.
for begin, end in [(0x8E4940,0x8E4979),(0x8F0F70,0x8F0F86),
                   (0x759260,0x7592B9),(0x7596A0,0x75977F),
                   (0x6D4730,0x6D47E9),(0x6F0E60,0x6F0E98),(0x6C0DF0,0x6C0E30)]:
    assert [(e.struct.BeginAddress,e.struct.EndAddress)
            for e in pe.DIRECTORY_ENTRY_EXCEPTION if e.struct.BeginAddress==begin] == [(begin,end)]
for call,target in [(0x8E4FC8,0x8F0F70),(0x8E527C,0x7596A0),
                    (0x759325,0x6F0E60),(0x75934C,0x6C0DF0),
                    (0x7597B8,0x6D4730),(0x6C2987,0x759260)]:
    assert image[call]==0xE8 and call+5+struct.unpack_from('<i',image,call+1)[0]==target
for address,pattern in {
    0x8E4ED0:'49 8D B7 D4 01 00 00',
    0x8E4EE1:'41 8B 87 60 02 00 00',
    0x8E4F3B:'8B 98 60 02 00 00',
    0x8E4F41:'48 8D B0 D4 01 00 00',
}.items():
    expected=bytes.fromhex(pattern)
    assert image[address:address+len(expected)]==expected,hex(address)
print('PASS: pinned H2 identity, five unique bindings, seven entry unwind records, six edges and native target/parent consumers')
