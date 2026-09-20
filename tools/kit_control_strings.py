# Mine a Blam editing-kit / mod-tools binary (e.g. HREK reach_tag_test.exe, HCEEK, H2EK tool.exe)
# for control/aim assert + source strings and their RVAs, so the referencing code can be xref'd to
# read the control-struct field offsets BY NAME -- the fast path to the aim drive point.
# Struct offsets are shared with the retail game binary, so what you read here applies to retail.
#
#   py -3 kit_control_strings.py [kit_binary]
#   then: py -3 pedis.py --module <kit_binary> xref <string_rva>   (find the asserting code)
#         py -3 pedis.py --module <kit_binary> fn   <near_rva>      (read the [reg+offset] access)
#
# Worked example (Halo Reach): this surfaces `...\game\player_control.cpp` and
# `player_control->state.desired_angles.pitch`; xref'ing that pins yaw@player_control+0x94,
# pitch@+0x98 -- the networked control input the sim integrates into the unit aim vector. Read-only.
import sys, struct, re
PATH = sys.argv[1] if len(sys.argv) > 1 else r"N:\Program Files (x86)\Steam\steamapps\common\HREK\reach_tag_test.exe"
data = open(PATH, "rb").read()
pe = struct.unpack_from("<I", data, 0x3C)[0]
nsec = struct.unpack_from("<H", data, pe + 6)[0]
optsz = struct.unpack_from("<H", data, pe + 20)[0]
secs = []
off = pe + 24 + optsz
for _ in range(nsec):
    b = data[off:off + 40]
    vs, va, rs, rp = struct.unpack_from("<IIII", b, 8)
    secs.append((va, vs, rp, rs)); off += 40
def off2rva(o):
    for va, vs, rp, rs in secs:
        if rp <= o < rp + rs:
            return va + (o - rp)
    return None

# control/aim struct + field + source-file terms worth surfacing (case-insensitive substring)
kw = [b"player_control", b"control_data", b"unit_control", b"control_state",
      b"desired_aiming", b"desired_facing", b"desired_looking", b"desired_angle", b"aiming_vector",
      b"player_control.cpp", b"unit.cpp", b"units.cpp", b"control.cpp", b"biped.cpp",
      b"unit_control.cpp", b"\\units\\", b"\\players\\", b"aiming", b"facing_vector",
      b"looking_vector", b"throttle", b"analog_control", b"digital_control", b"action_context"]
seen = set()
for m in re.compile(rb"[\x20-\x7e]{6,}").finditer(data):
    s = m.group(); low = s.lower()
    if any(k in low for k in kw) and s not in seen:
        seen.add(s)
        rva = off2rva(m.start())
        if rva is not None:
            print("rva=0x%08X  %s" % (rva, s.decode("latin1")))
print("== %d matches ==" % len(seen))
