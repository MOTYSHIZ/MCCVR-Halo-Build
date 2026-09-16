"""Offline HREK-first Reach HUD height proof. Never attaches to a game.

Pins the official HREK basis/assert/enum and native call graph, then matches the
retail homolog/signatures/ABI. Executes the real HREK and retail screen/projected
anchor instructions in Unicorn at both official fullscreen virtual canvas sizes.
Only HREK's tag-block/user lookup and stack-cookie helper are mocked; none alters
the native anchor math. Inputs stay under ignored out/; no game file is changed.
"""
from pathlib import Path
import hashlib
import json
import re
import struct
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'out/pydeps'))
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64, UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_RSP, UC_X86_REG_RIP, UC_X86_REG_RAX,
    UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_R9,
    UC_X86_REG_GS_BASE)

INPUTS = ROOT / 'out/deps/re-tools/inputs'
SOURCE = (ROOT / 'src/dll/reach_hud_height.inl').read_text()
REPORT = {'status': 'PASS', 'headset_accepted': False, 'images': {}}
SPECS = {
    'reach_tag_test.exe': ('CBDD8448A87A433B0DFFC0DE47D06DB7A18B4BF868B96B057135DAA86790ABA8',
        0x91C490, 0x91D270, '36C30C21050AEEF3BB442C2624DAD1607A4F86AD5A45697EAF04DEF85391427B'),
    'haloreach.dll': ('738DD2D24EA3AEA12E1EE9AA4A61094BF116027D42004C35A19E5048608B0894',
        0x2DB54C, 0x2DC1BE, 'BFA80813CCBDDC580FC2444CE782AFDE54633B8C6AB08BDD3CFF8E024CB38889'),
}

def call(image, rva):
    assert image[rva] == 0xE8
    return rva + 5 + struct.unpack_from('<i', image, rva+1)[0]

def rip_data(image, rva, length):
    return rva + length + struct.unpack_from('<i', image, rva+length-4)[0]

def string(image, rva):
    return image[rva:image.index(b'\0', rva)].decode()

def pointer_string(pe, image, rva):
    return string(image, struct.unpack_from('<Q',image,rva)[0]-pe.OPTIONAL_HEADER.ImageBase)

def pattern(symbol):
    body = re.search(r'char\s+'+symbol+r'\[\]\s*=([^;]+);',SOURCE).group(1)
    tokens = ' '.join(re.findall(r'"([^"]*)"',body)).split()
    return b''.join(b'.' if t == '??' else re.escape(bytes.fromhex(t)) for t in tokens)

for name, (sha, begin, end, bodysha) in SPECS.items():
    data = (INPUTS/name).read_bytes()
    assert hashlib.sha256(data).hexdigest().upper() == sha
    pe = pefile.PE(data=data, fast_load=True)
    pe.parse_data_directories(directories=[3])
    im = pe.get_memory_mapped_image()
    assert any(e.struct.BeginAddress == begin and e.struct.EndAddress == end
               for e in pe.DIRECTORY_ENTRY_EXCEPTION)
    assert hashlib.sha256(im[begin:end]).hexdigest().upper() == bodysha
    if name == 'reach_tag_test.exe':
        assert pointer_string(pe,im,0x18AA998) == 'basis'
        assert pointer_string(pe,im,0x18AA9A0).endswith('interface\\chud\\chud_draw.cpp')
        assert [pointer_string(pe,im,0x20875C0+i*8) for i in range(8)] == [
            'parent','top left','top center','top right','center',
            'bottom left','bottom center','bottom right']
        assert im[0x91C4C4:0x91C4C8] == bytes.fromhex('48 8B 7D 77')
        assert im[0x91C5C7:0x91C5CC] == bytes.fromhex('F3 0F 11 4F 2C')
        callers = [0x91B409,0x91AD34,0x921B29]
        for site in callers: assert call(im,site) == begin
        assert call(im,0x91C9C7) == 0x91DCA0
        # Parent anchors are recursive. This guard is essential: the production
        # wrapper adjusts only after the outermost composed basis is complete.
        parent_calls = [m.start()+0x91DCA0 for m in re.finditer(b'\xe8',im[0x91DCA0:0x91E080])
                        if call(im,m.start()+0x91DCA0) == begin]
        assert len(parent_calls) == 1
        # The false sixth-output branches are projected anchors, not world-unit
        # bases. The object chain projects into the same virtual width/height
        # as screen anchors before the helper copies XY to basis+0x28/+0x2C.
        assert call(im,0x91DC39) == 0x925000
        assert call(im,0x925183) == 0x92D980
        assert call(im,0x91DC6C) == 0x91D270
        assert rip_data(im,0x92DF0E,8) == 0x4F4D740
        assert rip_data(im,0x92DF30,9) == 0x4F4D744
        assert im[0x91D2D3:0x91D2D7] == bytes.fromhex('48 8B 41 18')
        assert im[0x91D3E7:0x91D3EC] == bytes.fromhex('F2 0F 11 43 28')
        assert im[0x91D3F6:0x91D3F9] == bytes.fromhex('89 43 30')
    else:
        for symbol, at in [('kReachHudAnchorBasisEntryAob',begin),
                           ('kReachHudAnchorBasisOutputAob',0x2DB65B)]:
            assert [m.start() for m in re.finditer(pattern(symbol),im,re.S)] == [at]
        callers = [0x2DF2D5,0x2DCA43,0x2DEC72,0x2DA8BA]
        for site in callers: assert call(im,site) == begin
        assert call(im,0x2DB707) == 0x2DA818
        assert im[0x2DB56D:0x2DB571] == bytes.fromhex('48 8B 7D 77')
        assert im[0x2DB574:0x2DB578] == bytes.fromhex('4C 8B 75 7F')
        assert hashlib.sha256(im[0x2DA9E8:0x2DAB4B]).hexdigest().upper() == \
            '7F0B521C622B4E562CD2CDDE5B2BF51DEB118F0521239803290D424141D9A46F'
        assert call(im,0x2DB8DC) == 0x2DA9E8
        assert call(im,0x2DBC2C) == 0x2DA9E8
    record = {'sha256':sha, 'basis_function':hex(begin),
              'body_sha256':bodysha, 'caller_edges':[hex(x) for x in callers],
              'native_anchor_cases':[]}
    REPORT['images'][name] = record

    # Own private machine image. No host code, imports, game process, DllMain,
    # rendering, or on-disk game memory is executed or modified.
    base = pe.OPTIONAL_HEADER.ImageBase
    uc = Uc(UC_ARCH_X86,UC_MODE_64)
    uc.mem_map(base,(pe.OPTIONAL_HEADER.SizeOfImage+0xFFF)&~0xFFF)
    uc.mem_write(base,im)
    heap, stack, stop = 0x20000000,0x30000000,0x40000000
    uc.mem_map(heap,0x40000)
    for at in [stack,stop]: uc.mem_map(at,0x10000)
    user_data = heap+0x10000
    def write(rva,fmt,value): uc.mem_write(base+rva,struct.pack(fmt,value))
    if name == 'reach_tag_test.exe':
        write(0x4F4D788,'B',1)  # g_chud_draw_globals.valid
        mocks = {base+0xEB730:heap+0x1000,base+0xE96A40:None,
                 base+0x8BA220:user_data}
        def mock(machine,address,size,user):
            if address not in mocks: return
            if mocks[address] is not None: machine.reg_write(UC_X86_REG_RAX,mocks[address])
            sp=machine.reg_read(UC_X86_REG_RSP)
            target=struct.unpack('<Q',machine.mem_read(sp,8))[0]
            machine.reg_write(UC_X86_REG_RSP,sp+8)
            machine.reg_write(UC_X86_REG_RIP,target)
        uc.hook_add(UC_HOOK_CODE,mock)
        width_rva,height_rva=0x4F4D740,0x4F4D744
        projected = {15:0x90C,21:0x83C,22:0x870,23:0x8A4,24:0x8D8}
    else:
        write(0x4E38C68,'Q',heap+0x1000)
        write(0x4E39F20,'Q',heap+0x2000)
        write(0xD1F72C,'i',0)
        width_rva,height_rva=0xD1F6F0,0xD1F6F4
        # Private retail TLS mirrors the verified HREK per-user lookup; no
        # target math or producer instruction is mocked in the retail image.
        write(0xC17B18,'I',0)
        uc.reg_write(UC_X86_REG_GS_BASE,heap+0x8000)
        uc.mem_write(heap+0x8058,struct.pack('<Q',heap+0x8100))
        uc.mem_write(heap+0x8100,struct.pack('<Q',heap+0x9000))
        uc.mem_write(heap+0x95E8,struct.pack('<Q',user_data))
        projected = {15:0x5C9C,21:0x5BCC,22:0x5C00,23:0x5C34,24:0x5C68}
    for width,height in [(1280,720),(920,690)]:
        write(width_rva,'f',width);write(height_rva,'f',height)
        expected = [(0,0),(width/2,0),(width,0),(width/2,height/2),
                    (0,height),(width/2,height),(width,height)]
        cases = [(anchor,xy,True) for anchor,xy in enumerate(expected,1)]
        cases += [(anchor,(width/4,height*3/4),False) for anchor in projected]
        for anchor,xy,native_flag in cases:
            if anchor in projected:
                valid=user_data+projected[anchor]
                uc.mem_write(valid,b'\x01')
                # Native projection-result XY is +0x18/+0x1C, after the
                # record's +0x10 header. +0x20 disables edge rotation here.
                uc.mem_write(valid+0x10+0x18,struct.pack('<fff',*xy,0))
            rsp=stack+0x8008;basis=heap+0x4000;flag=heap+0x5000
            uc.mem_write(basis,b'\xCD'*0x34)
            uc.mem_write(flag,b'\xCD')
            uc.mem_write(rsp,struct.pack('<Q',stop))
            uc.mem_write(rsp+0x28,struct.pack('<QQ',basis,flag))
            for reg,val in [(UC_X86_REG_RSP,rsp),(UC_X86_REG_RCX,0),
                (UC_X86_REG_RDX,anchor),(UC_X86_REG_R8,heap+0x6000),
                (UC_X86_REG_R9,heap+0x7000)]: uc.reg_write(reg,val)
            try:
                uc.emu_start(base+begin,stop,count=10000)
            except Exception as error:
                raise RuntimeError((name,anchor,hex(uc.reg_read(UC_X86_REG_RIP)))) from error
            assert uc.reg_read(UC_X86_REG_RIP)==stop
            assert uc.reg_read(UC_X86_REG_RAX)&255 == 1
            values=struct.unpack('<13f',uc.mem_read(basis,0x34))
            assert values[10:12] == xy,(name,anchor,values[10:12],xy)
            assert values[:10] == (1,1,0,0,0,1,0,0,0,1)
            assert values[12] == 0 and uc.mem_read(flag,1) == bytes([native_flag])
            record['native_anchor_cases'].append({'canvas':[width,height],
                'anchor':anchor,'native_xy':list(xy),'native_flag':native_flag})

print(json.dumps(REPORT,indent=2))
