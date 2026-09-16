"""Execute pinned CE following-camera orientation and perspective instructions.

The complete native unit-control packet writer runs before each orientation
slice. Unit facing/aiming/looking and an independently rotated vehicle object
must not change the following camera's native output-user angle source.
This is offline Unicorn memory only: no game process or installed-file writes.
"""
import argparse
import hashlib
import math
from pathlib import Path
import struct

from test_ce_unit_control_native import Machine, BASE, HEAP, STACK, OWNER, ROOT, SHA, require, pefile
from unicorn import UC_HOOK_MEM_READ
from unicorn.x86_const import UC_X86_REG_RSP, UC_X86_REG_RBP, UC_X86_REG_RSI, UC_X86_REG_RDI, UC_X86_REG_RDX, UC_X86_REG_RBX, UC_X86_REG_RAX


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--image', type=Path, default=ROOT / 'out/deps/re-tools/inputs/halo1.dll')
    args = parser.parse_args()
    raw = args.image.read_bytes()
    require(hashlib.sha256(raw).hexdigest().upper() == SHA, 'pinned CE image mismatch')
    image = pefile.PE(data=raw, fast_load=True).get_memory_mapped_image()
    machine = Machine(image)
    camera, user_input, output, vehicle = (HEAP + offset for offset in (0x23000, 0x24000, 0x25000, 0x26000))
    reads = []
    machine.uc.hook_add(UC_HOOK_MEM_READ, lambda uc, access, address, size, value, data: reads.append((address, size)))

    # E-CE-FP-3 already verifies this native perspective reader. Run its complete
    # following-camera branch; 1 is an observed native result, not a guessed enum.
    for user in range(4):
        director = BASE + 0x2d9b960 + user * 0xf8
        machine.write(director + 0x10, 'Q', BASE + 0xc52ca4)
        machine.write(director + 0x5c, 'H', 0x7777)
        machine.call(0xb14ea4, RCX=user)
        require(machine.uc.reg_read(UC_X86_REG_RAX) & 0xffff == 1, 'following-camera native perspective is not 1')
        require(machine.read(director + 0x5c, 'H')[0] == 1, 'native perspective cache was not updated')

    count = 0
    for user in range(4):
        for yaw in (0.01, 0.3, math.pi / 2, math.pi - 0.01, math.pi + 0.01, 2 * math.pi - 0.01):
            for pitch in (-1.49, 0, 1.49):
                for free_yaw, free_pitch in ((0, 0), (0.12, -0.35), (-0.21, 0.35)):
                    for aim_yaw in (-2.3, 2.3):
                        # Native packet writes use a real local-unit fixture;
                        # camera angles belong to the separate player-control record.
                        packet = bytearray((i * 3 + 7) & 255 for i in range(0x50))
                        struct.pack_into('<BBHBBhh', packet, 0, 3, 1, 0x2000, 0, 0xff, 2, -1)
                        for offset, vector in ((0xc, (0.8, -0.6, 0.2)),
                                               (0x1c, (math.cos(aim_yaw), math.sin(aim_yaw), 0)),
                                               (0x28, (0, math.cos(aim_yaw), math.sin(aim_yaw))),
                                               (0x34, (math.sin(aim_yaw), 0, math.cos(aim_yaw)))):
                            struct.pack_into('<3f', packet, offset, *vector)
                        machine.uc.mem_write(machine.packet, bytes(packet))
                        machine.write(BASE + 0x1b7b630, 'B', 0)
                        for other in range(4):
                            machine.write(machine.control + 0x17c + other * 0x38, '2f', 0.2 + other * 0.3, -0.1)
                        machine.write(machine.control + 0x17c + user * 0x38, '2f', yaw, pitch)
                        control_before = bytes(machine.uc.mem_read(machine.control, 0x300))
                        machine.call(0xafe098, RCX=OWNER, RDX=machine.packet, R8=0xffffffff)
                        require(bytes(machine.uc.mem_read(machine.control, 0x300)) == control_before,
                                'unit packet writer modified native user angles')
                        for source, target in ((0x1c, 0x204), (0x28, 0x210), (0x34, 0x234)):
                            require(machine.uc.mem_read(machine.unit + target, 12) == packet[source:source + 12],
                                    'native unit packet field handoff changed')
                        for offset in (0x30, 0x21c):
                            machine.write(machine.unit + offset, '3f', math.cos(aim_yaw), math.sin(aim_yaw), 0)
                        machine.write(vehicle + 0x30, '3f', math.cos(-aim_yaw), math.sin(-aim_yaw), 0)
                        machine.write(camera + 0x10, '2f', free_yaw, free_pitch)
                        machine.write(user_input, 'H', user)
                        unit_before = bytes(machine.uc.mem_read(machine.unit, 0x500))
                        vehicle_before = bytes(machine.uc.mem_read(vehicle, 0x100))
                        stack = STACK + 0x1f000
                        for register, value in ((UC_X86_REG_RSP, stack), (UC_X86_REG_RBP, stack + 0x90),
                                                (UC_X86_REG_RSI, user_input), (UC_X86_REG_RDI, camera),
                                                (UC_X86_REG_RDX, camera + 0x14), (UC_X86_REG_RBX, output)):
                            machine.uc.reg_write(register, value)
                        reads.clear()
                        # Actual native read, freelook addition, pitch clamp and
                        # trig generation. Stop before camera-track positioning.
                        machine.uc.emu_start(BASE + 0xc52e94, BASE + 0xc52f2b, count=100)
                        native_yaw = struct.unpack('<f', struct.pack('<f', yaw))[0]
                        native_pitch = struct.unpack('<f', struct.pack('<f', pitch))[0]
                        desired_yaw = native_yaw + free_yaw
                        desired_pitch = max(-math.pi / 2, min(math.pi / 2, native_pitch + free_pitch))
                        expected = (math.cos(desired_yaw) * math.cos(desired_pitch),
                                    math.sin(desired_yaw) * math.cos(desired_pitch), math.sin(desired_pitch))
                        actual = machine.read(output + 0x24, '3f')
                        require(all(abs(a - b) < 1e-6 for a, b in zip(actual, expected)),
                                ('following camera heading', user, yaw, pitch, free_yaw, free_pitch, actual, expected))
                        require(bytes(machine.uc.mem_read(machine.control, 0x300)) == control_before,
                                'following camera modified native user angles')
                        require(bytes(machine.uc.mem_read(machine.unit, 0x500)) == unit_before and
                                bytes(machine.uc.mem_read(vehicle, 0x100)) == vehicle_before,
                                'following camera modified unit or vehicle orientation')
                        require(not any(address < end and address + size > start
                                        for address, size in reads
                                        for start, end in ((machine.unit, machine.unit + 0x500), (vehicle, vehicle + 0x100))),
                                'following-camera direction unexpectedly read a unit or vehicle vector')
                        count += 1
    print('PASS: 4 complete native perspective calls identify the following camera as perspective 1.')
    print(f'PASS: {count} native packet-to-following-camera cases across all users, yaw seams, pitch clamps and freelook offsets; independent unit/vehicle vectors do not change camera orientation or native angles.')
    print('LIMIT: CRT trig and biped update notification are modeled. The camera slice excludes pivot/track positioning, collision, interpolation and earlier player-control updates. Vehicle steering consumers and headset behavior require separate evidence.')


if __name__ == '__main__':
    main()
