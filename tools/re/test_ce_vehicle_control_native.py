"""Execute CE's pinned native packet writer and driver/gunner transfer slices.

This runs offline Unicorn memory only. It does not emulate complete vehicle
physics, collision, networking or rendering and does not prove headset feel.
"""
import argparse
import hashlib
import math
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'out/pydeps'))
import pefile
from unicorn.x86_const import *
from test_ce_unit_control_native import Machine, BASE, OWNER, SHA, require


def case(image, yaw, pitch, driver, gunner, suppressed):
    machine = Machine(image)
    uc = machine.uc
    vehicle_offset, gunner_offset = 0x1800, 0x1000
    vehicle = machine.objects + vehicle_offset + 0x34
    gunner_unit = machine.objects + gunner_offset + 0x34
    gunner_handle = OWNER + 1
    machine.write(machine.headers + 0x100 + (gunner_handle & 0xffff)*12 + 8, 'i', gunner_offset)
    machine.write(gunner_unit + 0x1f8, 'I', 0x45670001)
    machine.write(gunner_unit + 0x70, 'H', 0)

    direction = (math.cos(yaw)*math.cos(pitch), math.sin(yaw)*math.cos(pitch), math.sin(pitch))
    other_direction = (-direction[1], direction[0], direction[2])
    throttle = (.75, -.25, .125)
    raw = bytearray((i*7+3) & 255 for i in range(0x50))
    struct.pack_into('<BBHBBhh', raw, 0, 0, 0, 0x7c3f, 0, 0xff, 0, -1)
    struct.pack_into('<3f', raw, 0xc, *throttle)
    struct.pack_into('<f', raw, 0x18, .4)
    for offset in (0x1c, 0x28, 0x34):
        struct.pack_into('<3f', raw, offset, *direction)
    uc.mem_write(machine.packet, bytes(raw))
    machine.call(0xafe098, RCX=OWNER, RDX=machine.packet, R8=0xffffffff)
    require(bytes(uc.mem_read(machine.packet, 0x50)) == raw, 'native writer mutated private packet')
    for offset in (0x1c, 0x28, 0x34):
        struct.pack_into('<3f', raw, offset, *other_direction)
    uc.mem_write(machine.packet, bytes(raw))
    machine.call(0xafe098, RCX=gunner_handle, RDX=machine.packet, R8=0xffffffff)

    original_facing, original_aim, original_look = (1., 0., 0.), (0., 1., 0.), (0., 0., 1.)
    for offset, vector in ((0x204, original_facing), (0x210, original_aim), (0x234, original_look)):
        machine.write(vehicle + offset, '3f', *vector)
    machine.write(vehicle + 0x258, '3f', 0., 0., 0.)
    machine.write(vehicle + 0x264, 'f', .9)
    machine.write(vehicle + 0x304, 'I', OWNER if driver else 0xffffffff)
    machine.write(vehicle + 0x308, 'I', gunner_handle if gunner else 0xffffffff)
    machine.write(vehicle + 0x1d4, 'I', 0x800000 if suppressed else 0)
    # These are normal engine-selected role handles, not VR-authored roles.
    before_driver = bytes(uc.mem_read(machine.unit, 0x500))
    before_gunner = bytes(uc.mem_read(gunner_unit, 0x500))
    before_camera = bytes(uc.mem_read(machine.control, 0x300))
    uc.reg_write(UC_X86_REG_R12, machine.objects)
    uc.reg_write(UC_X86_REG_R14, vehicle_offset)
    uc.reg_write(UC_X86_REG_RDI, vehicle + 0x1d4)
    uc.reg_write(UC_X86_REG_RBX, 0)
    uc.emu_start(BASE + 0xafc228, BASE + 0xafc58b, count=2000)
    require(uc.reg_read(UC_X86_REG_RIP) == BASE + 0xafc58b, 'native vehicle handoff did not reach boundary')
    def vector_equal(offset, expected):
        actual = machine.read(vehicle + offset, '3f')
        require(all(abs(a-b) < 1e-6 for a, b in zip(actual, expected)), ('vehicle vector', hex(offset), actual, expected))
    vector_equal(0x204, direction if driver and not suppressed else original_facing)
    vector_equal(0x258, throttle if driver and not suppressed else (0., 0., 0.))
    vector_equal(0x210, other_direction if gunner and not suppressed else original_aim)
    vector_equal(0x234, other_direction if gunner and not suppressed else original_look)
    expected_flags = ((0x3f if driver else 0) | (0x7c00 if gunner else 0)) if not suppressed else 0
    require(machine.read(vehicle + 0x1d8, 'I')[0] == expected_flags, 'native role-specific buttons changed')
    require(bytes(uc.mem_read(machine.unit, 0x500)) == before_driver, 'vehicle handoff mutated driver')
    require(bytes(uc.mem_read(gunner_unit, 0x500)) == before_gunner, 'vehicle handoff mutated gunner')
    require(bytes(uc.mem_read(machine.control, 0x300)) == before_camera, 'vehicle handoff mutated native camera angles')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--image', type=Path, default=ROOT/'out/deps/re-tools/inputs/halo1.dll')
    args = parser.parse_args()
    raw = args.image.read_bytes()
    require(hashlib.sha256(raw).hexdigest().upper() == SHA, 'pinned halo1.dll hash mismatch')
    image = pefile.PE(data=raw).get_memory_mapped_image()
    count = 0
    for yaw, pitch in ((0., 0.), (math.pi/2, .4), (-math.pi/2, -.4), (.78, .9), (math.pi, -.9)):
        for driver, gunner in ((False, False), (True, False), (False, True), (True, True)):
            for suppressed in (False, True):
                case(image, yaw, pitch, driver, gunner, suppressed)
                count += 1
    print(f'PASS: {count} native driver/gunner handoffs, {count*2} complete packet writes; native camera angles unchanged')


if __name__ == '__main__':
    main()
