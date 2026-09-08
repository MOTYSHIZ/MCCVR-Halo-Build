import hashlib, json, pathlib, re, struct
import pefile

root = pathlib.Path(__file__).resolve().parent.parent
path = root / 'out/deps/re-tools/inputs/haloreach.dll'
raw = path.read_bytes()
expected = '738DD2D24EA3AEA12E1EE9AA4A61094BF116027D42004C35A19E5048608B0894'
assert hashlib.sha256(raw).hexdigest().upper() == expected, 'Pinned Reach image changed'
pe = pefile.PE(data=raw)
image = pe.get_memory_mapped_image()
source = (root / 'src/dll/reach_contact_melee_runtime.inl').read_text()
bindings = re.findall(r'\{0x([0-9A-F]+),"([0-9A-F? ]+)"\}', source)
assert len(bindings) == 5
report = {'sha256': expected, 'bindings': [], 'calls': []}
for rva, pattern in bindings:
    expression = b''.join(b'.' if b == '??' else re.escape(bytes([int(b,16)])) for b in pattern.split())
    matches = [m.start() for m in re.finditer(b'(?='+expression+b')', image, re.DOTALL)]
    assert matches == [int(rva,16)], (rva,matches)
    report['bindings'].append({'rva':rva,'matches':len(matches)})
calls = re.findall(r'ReachVerifyRel32Call\(base,0x([0-9A-F]+),0x([0-9A-F]+)\)',source)
assert len(calls) == 7
for caller, target in calls:
    address = int(caller,16)
    assert image[address] == 0xE8
    actual = address+5+struct.unpack_from('<i', image, address+1)[0]
    assert actual == int(target,16), (caller,target,hex(actual))
    report['calls'].append({'caller':caller,'target':target})
destination = root / 'out/contact-reach-native-bindings.json'
destination.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))

# Each title has its own pinned image and patterns; shared semantics never
# stand in for title-specific machine-code verification.
path = root / 'out/deps/re-tools/inputs/halo3.dll'
raw = path.read_bytes()
expected = 'B209D8454B12DC77E54CCD2C9924EC8D44B8619D21CF98E36FFAF601E67EFB63'
assert hashlib.sha256(raw).hexdigest().upper() == expected, 'Pinned Halo 3 image changed'
image = pefile.PE(data=raw).get_memory_mapped_image()
source = (root / 'src/dll/halo3_contact_melee_runtime.inl').read_text()
bindings = re.findall(r'\{0x([0-9A-F]+),"([0-9A-F? ]+)"\}', source)
assert len(bindings) == 5
report = {'sha256': expected, 'bindings': [], 'calls': []}
for rva, pattern in bindings:
    expression = b''.join(b'.' if b == '??' else re.escape(bytes([int(b,16)])) for b in pattern.split())
    matches = [m.start() for m in re.finditer(b'(?='+expression+b')', image, re.DOTALL)]
    assert matches == [int(rva,16)], (rva,matches)
    report['bindings'].append({'rva':rva,'matches':len(matches)})
calls = re.findall(r'Halo3ContactVerifyCall\(base,0x([0-9A-F]+),0x([0-9A-F]+)\)',source)
assert len(calls) == 4
for caller, target in calls:
    address = int(caller,16)
    assert image[address] == 0xE8
    actual = address+5+struct.unpack_from('<i', image, address+1)[0]
    assert actual == int(target,16), (caller,target,hex(actual))
    report['calls'].append({'caller':caller,'target':target})
assert 0x35B26C + struct.unpack_from('<i',image,0x35B268)[0] == 0x2127108
(root / 'out/contact-h3-native-bindings.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))

path = root / 'out/deps/re-tools/inputs/halo2.dll'
raw = path.read_bytes()
expected = 'DE65B4F4FDBF3F0A5EAB7431FE530DA17DD815599182DFD6AE9B7E21CF171946'
assert hashlib.sha256(raw).hexdigest().upper() == expected, 'Pinned Halo 2 image changed'
image = pefile.PE(data=raw).get_memory_mapped_image()
source = (root / 'src/dll/halo2_contact_melee_runtime.inl').read_text()
bindings = re.findall(r'\{0x([0-9A-F]+),"([0-9A-F? ]+)"\}', source)
assert len(bindings) == 5
report = {'sha256': expected, 'bindings': [], 'calls': []}
for rva, pattern in bindings:
    expression = b''.join(b'.' if b == '??' else re.escape(bytes([int(b,16)])) for b in pattern.split())
    matches = [m.start() for m in re.finditer(b'(?='+expression+b')', image, re.DOTALL)]
    assert matches == [int(rva,16)], (rva,matches)
    report['bindings'].append({'rva':rva,'matches':len(matches)})
calls = re.findall(r'Halo2ContactVerifyCall\(base,0x([0-9A-F]+),0x([0-9A-F]+)\)',source)
assert len(calls) == 7
for caller, target in calls:
    address = int(caller,16)
    assert image[address] == 0xE8
    actual = address+5+struct.unpack_from('<i', image, address+1)[0]
    assert actual == int(target,16), (caller,target,hex(actual))
    report['calls'].append({'caller':caller,'target':target})
assert 0x8DD523 + struct.unpack_from('<i',image,0x8DD51F)[0] == 0x18B7398
(root / 'out/contact-h2-native-bindings.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))

path = root / 'out/deps/re-tools/inputs/halo3odst.dll'
raw = path.read_bytes()
expected = '5BB20976EFDFD9E1CE59C589339804725FEC239021027C8D65B2733EAB94829A'
assert hashlib.sha256(raw).hexdigest().upper() == expected, 'Pinned ODST image changed'
image = pefile.PE(data=raw).get_memory_mapped_image()
source = (root / 'src/dll/odst_contact_melee_runtime.inl').read_text()
bindings = re.findall(r'\{0x([0-9A-F]+),"([0-9A-F? ]+)"\}', source)
assert len(bindings) == 5
report = {'sha256': expected, 'bindings': [], 'calls': []}
for rva, pattern in bindings:
    expression = b''.join(b'.' if b == '??' else re.escape(bytes([int(b,16)])) for b in pattern.split())
    matches = [m.start() for m in re.finditer(b'(?='+expression+b')', image, re.DOTALL)]
    assert matches == [int(rva,16)], (rva,matches)
    report['bindings'].append({'rva':rva,'matches':len(matches)})
calls = re.findall(r'OdstContactVerifyCall\(base,0x([0-9A-F]+),0x([0-9A-F]+)\)',source)
assert len(calls) == 5
for caller, target in calls:
    address = int(caller,16)
    assert image[address] == 0xE8
    actual = address+5+struct.unpack_from('<i', image, address+1)[0]
    assert actual == int(target,16), (caller,target,hex(actual))
    report['calls'].append({'caller':caller,'target':target})
assert 0x39FE92 + struct.unpack_from('<i',image,0x39FE8E)[0] == 0x217E3C8
(root / 'out/contact-odst-native-bindings.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))

assert struct.unpack_from('<Q',image,0x8F6A30)[0] == 0x1803DEB98

path = root / 'out/deps/re-tools/inputs/halo4.dll'
raw = path.read_bytes()
expected = '7C53E7D5BC9848545A1B70E2768242479336FBA1B7630D7AB955F7FD0C34FA84'
assert hashlib.sha256(raw).hexdigest().upper() == expected, 'Pinned Halo 4 image changed'
image = pefile.PE(data=raw).get_memory_mapped_image()
source = (root / 'src/dll/halo4_contact_melee_runtime.inl').read_text()
bindings = re.findall(r'\{0x([0-9A-F]+),"([0-9A-F? ]+)"\}', source)
assert len(bindings) == 9
report = {'sha256': expected, 'bindings': [], 'calls': []}
for rva, pattern in bindings:
    expression = b''.join(b'.' if b == '??' else re.escape(bytes([int(b,16)])) for b in pattern.split())
    matches = [m.start() for m in re.finditer(b'(?='+expression+b')', image, re.DOTALL)]
    assert matches == [int(rva,16)], (rva,matches)
    report['bindings'].append({'rva':rva,'matches':len(matches)})
calls = re.findall(r'Halo4ContactVerifyCall\(base,0x([0-9A-F]+),0x([0-9A-F]+)\)',source)
assert len(calls) == 13
for caller, target in calls:
    address = int(caller,16)
    assert image[address] == 0xE8
    actual = address+5+struct.unpack_from('<i', image, address+1)[0]
    assert actual == int(target,16), (caller,target,hex(actual))
    report['calls'].append({'caller':caller,'target':target})
assert 0x601EEB + struct.unpack_from('<i',image,0x601EE7)[0] == 0x2FFB098
(root / 'out/contact-h4-native-bindings.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
