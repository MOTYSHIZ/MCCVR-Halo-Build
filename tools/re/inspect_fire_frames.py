"""Rank already-identified kit barrel consumers by nearby large stack allocation."""
import pathlib, re, struct, sys
root=pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0,str(root/'out/pydeps'))
import pefile
groups={}
name=None
raw=(root/'out/dual-fire-kit-refs.txt').read_bytes()
for line in raw.decode('utf-16' if raw.startswith(b'\xff\xfe') else 'utf-8-sig').splitlines():
    if '.exe strings ' in line:
        name=line.split()[0]; groups[name]=set()
    m=re.search(r'function (0x[0-9a-f]+)',line)
    if m and name: groups[name].add(int(m[1],16))
for name,entries in groups.items():
    pe=pefile.PE(str(root/'out/deps/re-tools/inputs'/name))
    image=pe.get_memory_mapped_image()
    print(name)
    for entry in sorted(entries):
        start=max(0,entry-256)
        data=image[start:entry+128]
        for m in re.finditer(rb'\xb8(.{4})\xe8',data,re.DOTALL):
            size=struct.unpack('<I',m.group(1))[0]
            if 0x1000<=size<=0x100000:
                print('consumer',hex(entry),'alloc',hex(size),'instruction',hex(start+m.start()))
