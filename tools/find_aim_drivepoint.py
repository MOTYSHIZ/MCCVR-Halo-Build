# Reusable RE helper: find who WRITES a (derived) aim field and where each write SOURCES from,
# so the search for the real upstream drive point does not have to be hand-disassembled per game.
#
#   py -3 find_aim_drivepoint.py [module.dll] [target_offset_hex]
#   defaults: retail haloreach.dll, 0x214 (Reach's derived unit aiming vector)
#
# For every writer of [base+TARGET] it prints, per writer function:
#   - which aim-family offsets it also touches (many -> a real unit-aim function; few -> another struct)
#   - the ONE-STEP-BACK source of the write:
#       read [reg+off] AIM-FAMILY   -> a copy / derived chain
#       read [reg+off] OTHER-STRUCT -> a control-input candidate (inspect that offset/struct)
#       CALL return / arithmetic    -> computed; the control input is further upstream via that call
#
# Cross-game: pass each title's module and its derived-aim offset. AIM_FAMILY below is Halo Reach's
# (unit facing/aiming/looking desired+current vec3 pairs); set it per game, or widen it. Read-only.
import sys, struct
from capstone import Cs, CS_ARCH_X86, CS_MODE_64
from capstone.x86 import X86_OP_MEM, X86_OP_REG, X86_REG_RIP

PATH = sys.argv[1] if len(sys.argv) > 1 else \
    r"G:\SteamLibrary\steamapps\common\Halo The Master Chief Collection\haloreach\haloreach.dll"
TARGET = int(sys.argv[2], 16) if len(sys.argv) > 2 else 0x214
AIM_FAMILY = {0x1e4, 0x1f0, 0x208, 0x20c, 0x210, 0x214, 0x218, 0x21c, 0x22c, 0x238}  # Reach unit aim

data = open(PATH, "rb").read()
pe = struct.unpack_from("<I", data, 0x3C)[0]
nsec = struct.unpack_from("<H", data, pe + 6)[0]
optsz = struct.unpack_from("<H", data, pe + 20)[0]
opt = pe + 24
secs = []
off = pe + 24 + optsz
for _ in range(nsec):
    b = data[off:off + 40]
    vs, va, rs, rp = struct.unpack_from("<IIII", b, 8)
    ch = struct.unpack_from("<I", b, 36)[0]
    secs.append(dict(va=va, vs=vs, raw=rp, rs=rs, ex=bool(ch & 0x20000000)))
    off += 40

def rva2off(rva):
    for s in secs:
        if s["va"] <= rva < s["va"] + max(s["vs"], s["rs"]):
            d = rva - s["va"]
            if d < s["rs"]:
                return s["raw"] + d
    return None

pdata_rva, pdata_size = struct.unpack_from("<II", data, opt + 112 + 3 * 8)
pdo = rva2off(pdata_rva)
funcs = []
for o in range(pdo, pdo + pdata_size, 12):
    b, e, _ = struct.unpack_from("<III", data, o)
    if b or e:
        funcs.append((b, e))
funcs.sort()

def fbounds(rva):
    lo, hi = 0, len(funcs) - 1
    while lo <= hi:
        m = (lo + hi) // 2
        b, e = funcs[m]
        if b <= rva < e:
            return (b, e)
        if rva < b:
            hi = m - 1
        else:
            lo = m + 1
    return None

md = Cs(CS_ARCH_X86, CS_MODE_64)
md.detail = True

def store_src(i, disp):
    if not i.mnemonic.startswith(("movss", "movsd")) or len(i.operands) < 2:
        return None
    d, s = i.operands[0], i.operands[1]
    if (d.type == X86_OP_MEM and d.mem.base not in (0, X86_REG_RIP)
            and d.mem.index == 0 and (d.mem.disp & 0xFFFFFFFF) == disp):
        return s
    return None

writers = set()
for s in [x for x in secs if x["ex"]]:
    code = data[s["raw"]:s["raw"] + s["rs"]]
    pos, n = 0, len(code)
    while pos < n:
        last = pos
        for i in md.disasm(code[pos:], s["va"] + pos):
            last = i.address - s["va"] + i.size
            if store_src(i, TARGET) is not None:
                fb = fbounds(i.address)
                if fb:
                    writers.add((fb, i.address))
        pos = last + 1 if last <= pos else last

byfn = {}
for fb, addr in writers:
    byfn.setdefault(fb, []).append(addr)
print("== float writers of [base+0x%X]: %d sites in %d functions ==" % (TARGET, len(writers), len(byfn)))

for fb in sorted(byfn):
    b, e = fb
    o = rva2off(b)
    insns = list(md.disasm(data[o:o + (e - b)], b))
    idx = {ins.address: k for k, ins in enumerate(insns)}
    aimreads, otherreads, ncalls = set(), set(), 0
    for ins in insns:
        if ins.mnemonic == "call":
            ncalls += 1
        for op in ins.operands:
            if op.type == X86_OP_MEM and op.mem.base not in (0, X86_REG_RIP) and op.mem.index == 0:
                dsp = op.mem.disp & 0xFFFFFFFF
                if 0x40 <= dsp < 0x400:
                    (aimreads if dsp in AIM_FAMILY else otherreads).add(dsp)
    is_unit_aim = len(aimreads & {0x1e4, 0x1f0, 0x22c, 0x238}) >= 2
    print("\n-- FUNC 0x%X..0x%X  writes 0x%X x%d  calls=%d  %s"
          % (b, e, TARGET, len(byfn[fb]), ncalls, "<== UNIT-AIM writer" if is_unit_aim else ""))
    print("   aim-family: %s" % sorted(hex(x) for x in aimreads))
    for waddr in byfn[fb]:
        k = idx.get(waddr)
        if k is None:
            continue
        src = insns[k].operands[1]
        if src.type != X86_OP_REG:
            print("   @0x%X src not reg" % waddr)
            continue
        sreg = src.reg
        found = "?"
        for j in range(k - 1, max(0, k - 40), -1):
            p = insns[j]
            if p.operands and p.operands[0].type == X86_OP_REG and p.operands[0].reg == sreg:
                if p.mnemonic in ("movss", "movsd", "movups", "movaps") and len(p.operands) > 1 \
                        and p.operands[1].type == X86_OP_MEM:
                    m = p.operands[1].mem
                    if m.base == X86_REG_RIP:
                        found = "const/rip @0x%X" % p.address
                    else:
                        dsp = m.disp & 0xFFFFFFFF
                        fam = "AIM-FAMILY" if dsp in AIM_FAMILY else "OTHER-STRUCT"
                        found = "read [reg+0x%X] %s @0x%X" % (dsp, fam, p.address)
                elif p.mnemonic == "call":
                    tgt = p.operands[0].imm if p.operands[0].type == 2 else 0
                    found = "CALL 0x%X return @0x%X" % (tgt, p.address)
                else:
                    found = "%s (computed) @0x%X" % (p.mnemonic, p.address)
                break
        print("   @0x%X  write src <- %s" % (waddr, found))
