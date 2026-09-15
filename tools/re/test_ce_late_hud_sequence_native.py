"""Execute CE's pinned packed output and natural late HUD ownership sequence.

This is offline emulation, never process access. The actual output descriptor
construction, late-frame guards, callback wrapper borrowing and cleanup execute
from halo1.dll. D3D/engine services and gameplay drawing are named fixtures.
It does not establish complete gameplay draw replay safety or headset pixels.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "out/pydeps"))
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64, UC_HOOK_CODE
from unicorn.x86_const import *

SHA = "0A12DC561780F449D3F4D0DF10BB8D3BC7BE7840A5BEB2B236F672EB6CD42E6C"
BASE, HEAP, STACK, STOP = 0x180000000, 0x60000000, 0x70000000, 0x71000000


def run(pe, frame_flags=0x10, list_flags=1, players=1, initialized=True):
    uc = Uc(UC_ARCH_X86, UC_MODE_64)
    uc.mem_map(BASE, (pe.OPTIONAL_HEADER.SizeOfImage + 4095) & ~4095)
    uc.mem_write(BASE, pe.get_memory_mapped_image())
    uc.mem_map(HEAP, 0x200000)
    uc.mem_map(STACK, 0x20000)
    uc.mem_map(STOP, 0x1000)
    backend, backend_vt, parent, parent_vt = HEAP+0x1000, HEAP+0x3000, HEAP+0x4000, HEAP+0x5000
    renderer, registry, selector = HEAP+0x6000, HEAP+0x16000, HEAP+0x19000
    packed, source, previous, secondary = HEAP+0x20000, HEAP+0x21000, HEAP+0x22000, HEAP+0x23000
    native_kind1, native_kind2 = HEAP+0x24000, HEAP+0x25000
    config, player_globals, engine, gameplay = HEAP+0x26000, HEAP+0x27000, HEAP+0x28000, HEAP+0x29000
    events, transfers, hud_targets, viewport_heights = [], [], [], []
    lock_depth = 0
    def write(at, fmt, *values): uc.mem_write(at, struct.pack(fmt, *values))
    def read(at, fmt): return struct.unpack(fmt, uc.mem_read(at, struct.calcsize(fmt)))
    def ret(value=0):
        sp = uc.reg_read(UC_X86_REG_RSP)
        uc.reg_write(UC_X86_REG_RAX, value)
        uc.reg_write(UC_X86_REG_RIP, read(sp, "<Q")[0])
        uc.reg_write(UC_X86_REG_RSP, sp+8)
    def event(name): events.append(name)
    stub_targets = {0x1dc1e0, 0x1dc2f0, 0x1dc5d0, 0xae0e0, 0xb3dcbc,
                    0xae0180, 0xbabd38, 0xac8844, 0xac6688, 0xad9fe8,
                    0xb31398, 0xb115b0, 0x5d5c0, 0x20a2c0, 0x1391640,
                    0x425660, 0x4512d0}
    def service(machine, address, size, _):
        nonlocal lock_depth
        if address in (STOP, BASE+0x4573e1, BASE+0x456eff):
            machine.emu_stop()
        elif address == BASE+0x740b0:
            event("natural_callback_enter")
        elif address == BASE+0x20b510:
            result = packed if uc.reg_read(UC_X86_REG_RDX)==0x4000001 else secondary
            ret(result)
        elif address == BASE+0x20b600:
            assert uc.reg_read(UC_X86_REG_RDX)==secondary
            event("secondary_release")
            ret()
        elif address == BASE+0xb29438:
            write(BASE+0x29af2b8, "<h", (uc.reg_read(UC_X86_REG_RCX)&0xffff)-0x10000 if uc.reg_read(UC_X86_REG_RCX)&0x8000 else uc.reg_read(UC_X86_REG_RCX)&0xffff)
            ret()
        elif address == BASE+0xb31d88:
            # Fixture is deliberately the native interface draw endpoint.
            # Static witness B32106 independently proves its B12B08 dispatch.
            assert lock_depth==1
            current=read(BASE+0x2e3b918,"<Q")[0]
            hud_targets.append(current)
            assert current==packed
            event("gameplay_interface_draw")
            ret()
        elif address == BASE+0x1dbe50:
            assert uc.reg_read(UC_X86_REG_RCX)==backend
            assert uc.reg_read(UC_X86_REG_RDX)==packed
            sp=uc.reg_read(UC_X86_REG_RSP)
            event("pre_hud_packed_target_bind" if read(sp,"<Q")[0]==BASE+0x456eff else "post_hud_packed_target_bind")
            ret(1)
        elif address == BASE+0x45e490:
            event("final_composite")
            ret()
        elif address == STOP+0x100:
            ret(640)
        elif address == STOP+0x110:
            ret(360)
        elif address == STOP+0x120:
            at=uc.reg_read(UC_X86_REG_RDX)
            request=read(at,"<QQiiiiiiii")
            transfers.append(request)
            event("packed_eye_copy")
            ret(1)
        elif address == STOP+0x130:
            event("native_backend_service")
            ret()
        elif address == STOP+0x140:
            sp=uc.reg_read(UC_X86_REG_RSP)
            viewport_heights.append(read(sp+0x28,"<I")[0])
            ret()
        elif address == STOP+0x150:
            lock_depth+=1
            event("native_lock_enter")
            ret()
        elif address == STOP+0x160:
            lock_depth-=1
            assert lock_depth>=0
            event("native_lock_leave")
            ret()
        elif BASE <= address < BASE+pe.OPTIONAL_HEADER.SizeOfImage and address-BASE in stub_targets:
            if address-BASE==0x425660: event("native_hud_tail")
            if address-BASE==0x4512d0: event("per_view_overlay")
            ret(1)
    uc.hook_add(UC_HOOK_CODE, service)
    write(backend,"<Q",backend_vt);write(parent,"<Q",parent_vt)
    for offset in [0x28,0x38,0x40,0xa8]:write(backend_vt+offset,"<Q",STOP+0x130)
    write(backend_vt+0x120,"<Q",STOP+0x140)
    write(backend_vt+0x190,"<Q",STOP+0x120)
    for offset in [0xd0,0xe0]:write(parent_vt+offset,"<Q",STOP+0x130)
    write(source,"<Q",HEAP+0x2a000)
    write(HEAP+0x2a000+0x18,"<Q",STOP+0x100);write(HEAP+0x2a000+0x20,"<Q",STOP+0x110)
    write(BASE+0x2e3bde0,"<Q",backend);write(BASE+0x2e3bdd8,"<Q",parent)
    write(BASE+0x1bea9e0,"<Q",renderer);write(BASE+0x1bea8a0,"<Q",registry)
    write(BASE+0x1bea6b8,"<Q",selector);write(BASE+0x1b7b114,"<B",1)
    write(BASE+0x2e3d0c8,"<Q",previous)
    write(BASE+0x2e3b918,"<Q",native_kind1);write(BASE+0x2e3b920,"<Q",native_kind2)
    write(BASE+0x2d9bdd1,"<B",initialized);write(BASE+0x2b23700,"<B",0)
    write(BASE+0x2d91330,"<Q",engine);write(engine+2,"<B",1)
    write(BASE+0x1b7aa84,"<I",1);write(BASE+0x2e3b829,"<B",1)
    write(BASE+0x2ea2d90,"<Q",player_globals);write(player_globals+0xb4,"<h",players)
    write(BASE+0x2ea0208,"<Q",gameplay)
    write(parent+0x118,"<Q",config);write(config+0x10,"<II",640,720)
    write(BASE+0x2e3cd78,"<Q",HEAP+0x2b000)
    write(BASE+0x1c33fe0,"<Q",BASE+0x740b0)
    constant, constants = HEAP+0x2c000, HEAP+0x2d000
    write(backend+0xd8,"<Q",constant);write(constant+8,"<Q",constants)
    # Pin imports by their PE names instead of assuming thunk addresses.
    for desc in pe.DIRECTORY_ENTRY_IMPORT:
        for imp in desc.imports:
            if imp.name==b"EnterCriticalSection":write(imp.address,"<Q",STOP+0x150)
            if imp.name==b"LeaveCriticalSection":write(imp.address,"<Q",STOP+0x160)
    for eye in range(2):
        write(BASE+0x1b7b11c,"<i",0);write(BASE+0x2d62890,"<Q",source)
        sp=STACK+0x1f008;write(sp,"<Q",STOP)
        uc.reg_write(UC_X86_REG_RSP,sp);uc.reg_write(UC_X86_REG_RCX,eye)
        uc.emu_start(BASE+0x45e2b0,STOP,count=100000)
    assert len(transfers)==2
    assert [v[1] for v in transfers]==[packed,packed]
    assert [v[7] for v in transfers]==[0,360],transfers
    # The real full frame explicitly binds packed color BEFORE the native
    # worker cleanup and late callback. This is not just a kind-1 alias.
    sp=STACK+0x1f008
    uc.reg_write(UC_X86_REG_RSP,sp);uc.reg_write(UC_X86_REG_RSI,0)
    uc.emu_start(BASE+0x456ece,BASE+0x456eff,count=100000)
    write(renderer+0xb0,"<I",list_flags);write(renderer+0xb8,"<I",2)
    sp=STACK+0x1f008;write(sp+0x54,"<I",frame_flags)
    uc.reg_write(UC_X86_REG_RSP,sp);uc.reg_write(UC_X86_REG_RCX,parent)
    uc.reg_write(UC_X86_REG_R14,0)
    uc.emu_start(BASE+0x4572a5,BASE+0x4573e1,count=100000)
    admitted=bool(frame_flags&0x10) and not bool(list_flags&2)
    assert events.count("natural_callback_enter")==int(admitted)
    assert len(hud_targets)==(players if admitted and initialized else 0)
    assert lock_depth==0
    assert read(BASE+0x2e3b918,"<Q")[0]==native_kind1
    assert read(BASE+0x2e3b920,"<Q")[0]==native_kind2
    if hud_targets:
        assert events.index("gameplay_interface_draw")>max(i for i,v in enumerate(events) if v=="packed_eye_copy")
        assert events.index("gameplay_interface_draw")>events.index("pre_hud_packed_target_bind")
        assert events.index("post_hud_packed_target_bind")>events.index("gameplay_interface_draw")
        assert viewport_heights[-1]==720,viewport_heights
    return dict(frame_flags=frame_flags,list_flags=list_flags,players=players,initialized=initialized,
                copies=len(transfers),natural_callbacks=events.count("natural_callback_enter"),
                gameplay_draws=len(hud_targets),restored_wrappers=True,balanced_callback_lock=True,events=events)


def native_scissor_states(pe):
    """Native rasterizer descriptor construction; only D3D creation is a stub."""
    uc=Uc(UC_ARCH_X86,UC_MODE_64)
    uc.mem_map(BASE,(pe.OPTIONAL_HEADER.SizeOfImage+4095)&~4095)
    uc.mem_write(BASE,pe.get_memory_mapped_image())
    uc.mem_map(HEAP,0x20000);uc.mem_map(STACK,0x20000);uc.mem_map(STOP,0x1000)
    state,device,vtable,source=HEAP+0x1000,HEAP+0x2000,HEAP+0x3000,HEAP+0x4000
    descriptors=[]
    def write(at,fmt,*values):uc.mem_write(at,struct.pack(fmt,*values))
    def read(at,fmt):return struct.unpack(fmt,uc.mem_read(at,struct.calcsize(fmt)))
    def ret():
        sp=uc.reg_read(UC_X86_REG_RSP)
        uc.reg_write(UC_X86_REG_RIP,read(sp,"<Q")[0]);uc.reg_write(UC_X86_REG_RSP,sp+8)
        uc.reg_write(UC_X86_REG_RAX,0)
    def service(machine,address,size,_):
        if address==STOP:machine.emu_stop()
        elif address==BASE+0x139362f:
            uc.mem_write(uc.reg_read(UC_X86_REG_RCX),bytes([uc.reg_read(UC_X86_REG_RDX)&255])*uc.reg_read(UC_X86_REG_R8));ret()
        elif address==BASE+0x1391640:ret()
        elif address in (STOP+0x100,STOP+0x200,STOP+0x300):
            assert uc.reg_read(UC_X86_REG_RCX)==device
            if address==STOP+0x300:
                desc=read(uc.reg_read(UC_X86_REG_RDX),"<IIIiffIIII")
                assert desc[7]==1,desc
                descriptors.append(desc)
            write(uc.reg_read(UC_X86_REG_R8),"<Q",HEAP+0x18000+address-STOP);ret()
    uc.hook_add(UC_HOOK_CODE,service)
    write(device,"<Q",vtable)
    for offset,target in [(0xa0,0x100),(0xa8,0x200),(0xb0,0x300)]:write(vtable+offset,"<Q",STOP+target)
    for mode in range(3):
        for bits in range(16):
            uc.mem_write(state,b"\0"*0x100);uc.mem_write(source,b"\0"*0x40)
            write(source+0x28,"<B",mode);write(source,"<I",bits)
            sp=STACK+0x1f008;write(sp,"<Q",STOP)
            uc.reg_write(UC_X86_REG_RSP,sp);uc.reg_write(UC_X86_REG_RCX,state)
            uc.reg_write(UC_X86_REG_RDX,device);uc.reg_write(UC_X86_REG_R8,source)
            uc.emu_start(BASE+0x202d70,STOP,count=100000)
    assert len(descriptors)==48,len(descriptors)
    return dict(native_state_constructions=len(descriptors),scissor_enabled_in_every_descriptor=True,
                limit="Native descriptor constructor runs; D3D object creation and security-cookie service are fixtures.")


def native_callback_clear_state(pe):
    """Execute the native late callback's actual backend ClearState service."""
    uc=Uc(UC_ARCH_X86,UC_MODE_64)
    uc.mem_map(BASE,(pe.OPTIONAL_HEADER.SizeOfImage+4095)&~4095)
    uc.mem_write(BASE,pe.get_memory_mapped_image())
    uc.mem_map(HEAP,0x100000);uc.mem_map(STACK,0x20000);uc.mem_map(STOP,0x1000)
    backend,parent,context,vtable,config,constants,values=[HEAP+v for v in
        (0x1000,0x70000,0x71000,0x72000,0x73000,0x74000,0x75000)]
    events=[]
    def write(at,fmt,*v):uc.mem_write(at,struct.pack(fmt,*v))
    def ret():
        sp=uc.reg_read(UC_X86_REG_RSP)
        uc.reg_write(UC_X86_REG_RIP,struct.unpack("<Q",uc.mem_read(sp,8))[0]);uc.reg_write(UC_X86_REG_RSP,sp+8)
    def service(machine,address,size,_):
        if address==STOP:machine.emu_stop()
        elif address==STOP+0x100:
            assert uc.reg_read(UC_X86_REG_RCX)==context
            events.append("D3D_ClearState");ret()
        elif address==BASE+0x1dbd50:
            assert uc.reg_read(UC_X86_REG_RCX)==backend
            events.append("constant_rebind_fixture");ret()
        elif address==BASE+0x1dbb60:
            assert uc.reg_read(UC_X86_REG_RCX)==backend
            events.append("buffer_rebind_fixture");ret()
    uc.hook_add(UC_HOOK_CODE,service)
    descriptor=bytes(range(0x48))
    uc.mem_write(backend+0x18,descriptor)
    write(backend+0xce0,"<Q",context);write(context,"<Q",vtable);write(vtable+0x370,"<Q",STOP+0x100)
    write(backend+0xcf8,"<I",1);write(backend+0xd20,"<Q",HEAP+0x76000)
    write(BASE+0x2e3bdd8,"<Q",parent);write(parent+0x118,"<Q",config);write(config+0x10,"<II",640,720)
    write(backend+0xd8,"<Q",constants);write(constants+8,"<Q",values)
    sp=STACK+0x1f008;write(sp,"<Q",STOP)
    uc.reg_write(UC_X86_REG_RSP,sp);uc.reg_write(UC_X86_REG_RCX,backend)
    uc.emu_start(BASE+0x203c00,STOP,count=10000)
    assert events==["D3D_ClearState"]+["constant_rebind_fixture"]*5+["buffer_rebind_fixture"],events
    assert uc.mem_read(backend+0x18,0x48)==descriptor
    assert struct.unpack("<I",uc.mem_read(backend+0xcf8,4))[0]==0
    assert struct.unpack("<Q",uc.mem_read(backend+0xd20,8))[0]==0
    return dict(native_backend_service="203C00",events=events,descriptor_preserved=True,
                cached_color_count=0,cached_depth_view=0,
                limit="Actual reset instructions execute; D3D ClearState and constant/buffer rebind endpoints are fixtures.")


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image",type=Path,default=ROOT/"out/deps/re-tools/inputs/halo1.dll")
    args=parser.parse_args();raw=args.image.read_bytes()
    if hashlib.sha256(raw).hexdigest().upper()!=SHA:raise SystemExit("Pinned CE image hash mismatch")
    pe=pefile.PE(data=raw)
    cases=[run(pe),run(pe,frame_flags=0),run(pe,list_flags=3),run(pe,initialized=False),run(pe,players=2)]
    scissor=native_scissor_states(pe);clear_state=native_callback_clear_state(pe)
    print(json.dumps(dict(status="PASS_NATIVE_EMULATION_ONLY",sha256=SHA,cases=cases,scissor_states=scissor,
        callback_clear_state=clear_state,
        limit="Real packed descriptor construction, late guards, callback owner borrowing/restoration and normal lock order; D3D/engine services and gameplay draws are fixtures. No full HUD replay safety or headset acceptance."),indent=2))


if __name__=="__main__":main()
