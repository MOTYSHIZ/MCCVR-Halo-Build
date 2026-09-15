"""Execute pinned CE scene refresh and per-view geometry admission in Unicorn.

The complete scene-camera, region-mask, scene-update, static refresh, and
per-view admission functions run from the pinned retail image. Region volume
intersection and geometric tests are controlled fixture inputs; OS locks,
progress/event callbacks and the security-cookie check are modeled. No game
process, DLL entry point, graphics device, capture, or installation is used.
"""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "out/pydeps"))
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64, UC_HOOK_CODE, UC_PROT_READ, UC_PROT_EXEC
from unicorn.x86_const import (UC_X86_REG_RAX, UC_X86_REG_RCX, UC_X86_REG_RDX,
    UC_X86_REG_R8, UC_X86_REG_R9, UC_X86_REG_RSP, UC_X86_REG_RIP)

SHA = "0A12DC561780F449D3F4D0DF10BB8D3BC7BE7840A5BEB2B236F672EB6CD42E6C"
BASE, HEAP, STACK, STOP = 0x180000000, 0x60000000, 0x70000000, 0x71000000
REGION_TEST, ENTER_LOCK, LEAVE_LOCK = STOP + 0x100, STOP + 0x200, STOP + 0x300
FUNCTIONS = {
    "scene_camera": (0x5434B0, 0x543549),
    "region_masks": (0x543550, 0x543826),
    "moving_objects": (0x543830, 0x543AEA),
    "static_refresh": (0x543AF0, 0x543CAA),
    "static_geometry_list": (0x2B2170, 0x2B2432),
    "region_static_flags": (0x542F00, 0x5430CB),
    "scene_update": (0x543CB0, 0x543DBA),
    "base_scene_update": (0x64D450, 0x64D4E4),
    "geometry_admission": (0x2D19A0, 0x2D1B0B),
}


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


class NativeScene:
    def __init__(self, pe):
        image = pe.get_memory_mapped_image()
        self.uc = Uc(UC_ARCH_X86, UC_MODE_64)
        self.image_size = (len(image) + 4095) & ~4095
        self.uc.mem_map(BASE, self.image_size)
        self.uc.mem_write(BASE, image)
        self.uc.mem_map(HEAP, 0x50000)
        self.uc.mem_map(STACK, 0x20000)
        self.uc.mem_map(STOP, 0x1000)
        self.scene, self.database = HEAP + 0x1000, HEAP + 0x10000
        self.regions = [HEAP + 0x20000, HEAP + 0x20400]
        self.tag, self.volume_table = HEAP + 0x21000, HEAP + 0x22000
        self.objects = [HEAP + 0x24000 + index * 0x200 for index in range(3)]
        self.model, self.model_data = HEAP + 0x25000, HEAP + 0x25400
        self.cameras = [HEAP + 0x27000, HEAP + 0x27100]
        self.view_list, self.bounds, self.lod = HEAP + 0x30000, HEAP + 0x31000, HEAP + 0x32000
        self.calls, self.instructions = Counter(), 0
        self.region_membership = [True, True]
        self.geometric_accept = [True, True]
        self.geometric_current_eye = 0
        self.geometric_calls = Counter()
        self.write(BASE + 0x1C33F58, "<Q", self.database)
        self.write(BASE + 0x2B17A50, "<I", 0)
        self.cookie = self.read(BASE + 0x1B75010, "<Q")[0]
        for descriptor in pe.DIRECTORY_ENTRY_IMPORT:
            for imported in descriptor.imports:
                if imported.name == b"EnterCriticalSection":
                    self.write(imported.address, "<Q", ENTER_LOCK)
                elif imported.name == b"LeaveCriticalSection":
                    self.write(imported.address, "<Q", LEAVE_LOCK)
        # Executable image bytes are never changed after fixture globals/imports.
        self.uc.mem_protect(BASE, self.image_size, UC_PROT_READ | UC_PROT_EXEC)
        self.write(self.scene, "<Q", BASE + 0x1819658)
        self.write(self.scene + 0x120, "<Q", HEAP + 0x2000)
        self.write(self.scene + 0x128, "<I", len(self.regions))
        self.write(HEAP + 0x2000, "<2Q", *self.regions)
        self.write(self.volume_table + 0x18, "<Q", REGION_TEST)
        self.write(self.tag + 0xE8, "<I", 0x10)
        for index, region in enumerate(self.regions):
            self.write(region + 0x78, "<Q", self.tag)
            self.write(region + 0x110, "<Q", self.volume_table)
            self.write(region + 0x1A8, "<I", 0 if index == 0 else 0x800)
            self.write(region + 0x1D8, "<Q", HEAP + 0x23000 + index * 0x100)
            self.write(region + 0x1E0, "<I", 1)
            self.write(HEAP + 0x23000 + index * 0x100, "<h", index)
        self.write(self.database + 0x2850, "<I", len(self.objects))
        self.write(self.database + 0x2858, "<Q", HEAP + 0x28000)
        self.write(HEAP + 0x28000, "<3Q", *self.objects)
        self.write(self.database + 0x2860, "<Q", HEAP + 0x29000)
        self.write(self.database + 0x286C, "<I", len(self.objects))
        self.write(self.database + 0x2870, "<Q", HEAP + 0x2A000)
        self.write(self.database + 0x287C, "<I", len(self.objects))
        self.write(self.model + 0x10, "<Q", self.model_data)
        self.write(self.model_data + 0xC0, "<6f", -1, -1, -1, 1, 1, 1)
        for index, obj in enumerate(self.objects):
            self.write(obj + 0x10, "<Q", self.model)
            self.write(obj + 0x18, "<I", 1)
            self.write(obj + 0x1C, "<I", int(index < 2))
            self.write(obj + 0x8E, "<B", 0x81)
        for index, camera in enumerate(self.cameras):
            self.write(camera, "<3f", -0.03 if index == 0 else 0.03, 0, 0)
        self.write(self.view_list + 8, "<I", 2)
        self.write(self.view_list + 0x10, "<I", 0x10B)
        self.write(self.view_list + 0x10 + 0x3C8, "<I", 0x20B)
        self.uc.hook_add(UC_HOOK_CODE, self.instruction)

    def write(self, address, fmt, *values):
        self.uc.mem_write(address, struct.pack(fmt, *values))

    def read(self, address, fmt):
        return struct.unpack(fmt, self.uc.mem_read(address, struct.calcsize(fmt)))

    def return_value(self, value=0):
        sp = self.uc.reg_read(UC_X86_REG_RSP)
        self.uc.reg_write(UC_X86_REG_RAX, value)
        self.uc.reg_write(UC_X86_REG_RIP, self.read(sp, "<Q")[0])
        self.uc.reg_write(UC_X86_REG_RSP, sp + 8)

    def instruction(self, machine, address, size, _):
        self.instructions += 1
        if address == STOP:
            machine.emu_stop()
            return
        if address in (ENTER_LOCK, LEAVE_LOCK):
            require(machine.reg_read(UC_X86_REG_RCX) == self.scene + 0x1B8,
                    "Unexpected critical section")
            self.calls["enter_lock" if address == ENTER_LOCK else "leave_lock"] += 1
            self.return_value()
            return
        if address == REGION_TEST:
            require(machine.reg_read(UC_X86_REG_RCX) == self.regions[0] + 0x110,
                    "Hidden or unknown region reached the volume predicate")
            position = machine.reg_read(UC_X86_REG_RDX)
            require(position in self.cameras, "Unexpected region camera")
            self.calls["modeled_region_volume"] += 1
            self.return_value(int(self.region_membership[self.cameras.index(position)]))
            return
        rva = address - BASE
        if rva in (0x11CA70, 0x11CB10):
            camera_volume = machine.reg_read(UC_X86_REG_RCX)
            eye = (camera_volume - self.view_list - 0x264) // 0x3C8
            require(eye in (0, 1), "Unexpected geometric camera")
            self.geometric_current_eye = eye
            self.geometric_calls[eye] += 1
            self.calls["modeled_bounds_test"] += 1
            self.return_value(0)
            return
        if rva == 0x1298C0:
            self.calls["modeled_geometric_intersection"] += 1
            self.return_value(int(self.geometric_accept[self.geometric_current_eye]))
            return
        if rva in (0x471EF0, 0x5E59F0):
            self.calls["progress" if rva == 0x471EF0 else "event"] += 1
            self.return_value()
            return
        if rva == 0x1391640:
            require(machine.reg_read(UC_X86_REG_RCX) == self.cookie, "Native stack cookie mismatch")
            self.calls["checked_cookie"] += 1
            self.return_value()
            return
        allowed = False
        for name, (start, end) in FUNCTIONS.items():
            if start <= rva < end:
                allowed = True
                if start == rva:
                    self.calls[name] += 1
                break
        require(allowed, f"Unexpected native instruction {rva:#x}")

    def call(self, rva, arguments, fifth=0):
        sp = STACK + 0x1FF08
        self.write(sp, "<Q", STOP)
        self.write(sp + 0x28, "<Q", fifth)
        self.uc.reg_write(UC_X86_REG_RSP, sp)
        for register, value in zip((UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_R9), arguments):
            self.uc.reg_write(register, value)
        self.uc.emu_start(BASE + rva, STOP, count=300000)
        require(self.uc.reg_read(UC_X86_REG_RIP) == STOP, "Native call did not return within instruction budget")
        return self.uc.reg_read(UC_X86_REG_RAX)

    def update(self, secondary, refresh):
        before = self.calls.copy()
        self.call(0x5434B0, (self.scene, int(refresh), self.cameras[0], self.cameras[1] if secondary else 0))
        camera_flags = self.read(self.scene + 0x110, "<I")[0]
        region_flags = [self.read(region + 0x1A8, "<I")[0] for region in self.regions]
        self.call(0x543CB0, (self.scene, self.database))
        require(self.read(self.scene + 0x1E8, "<I")[0] == 0, "Native lock nesting did not restore")
        # The rebuild tail-calls the complete native static-list producer.
        # Verify its published object/bounds arrays, including authored-hidden
        # objects which the later per-view admission independently rejects.
        count = len(self.objects)
        require(self.read(self.database + 0x2868, "<I")[0] == count and
                self.read(self.database + 0x2878, "<I")[0] == count,
                "Native static geometry/bounds counts disagree")
        require(list(self.read(HEAP + 0x29000, "<3Q")) == self.objects,
                "Native static geometry list did not preserve fixture objects")
        for index in range(count):
            require(self.read(HEAP + 0x2A000 + 24 * index, "<6f") == (-1, -1, -1, 1, 1, 1),
                    "Native static geometry bounds do not match the model")
        return dict(secondary=secondary, refresh=refresh, camera_scene_flags=camera_flags,
                    region_flags=region_flags, object_flags=[self.read(obj + 0x8E, "<B")[0] for obj in self.objects],
                    static_geometry_count=count,
                    static_refresh_calls=self.calls["static_refresh"] - before["static_refresh"],
                    scene_flags=self.read(self.scene + 0x110, "<I")[0])

    def admission(self, object_index):
        before = self.geometric_calls.copy()
        result = self.call(0x2D19A0, (self.view_list, self.objects[object_index], self.bounds, self.lod))
        return dict(mask=result, geometric_calls=[self.geometric_calls[eye] - before[eye] for eye in range(2)])


def verify(native):
    cases = []

    def case(name, secondary, refresh, expected_flags, expected_masks, expected_refreshes):
        result = native.update(secondary, refresh)
        admissions = [native.admission(index) for index in range(3)]
        require(result["object_flags"] == expected_flags, f"{name}: wrong native object flags: {result}")
        require([value["mask"] for value in admissions] == expected_masks,
                f"{name}: wrong native geometry admission: {admissions}")
        require(result["static_refresh_calls"] == expected_refreshes, f"{name}: unexpected refresh count")
        require(bool(result["camera_scene_flags"] & 0x1000) == secondary,
                f"{name}: native two-camera state does not match its pointers")
        require(result["region_flags"][1] == 0x800, f"{name}: authored hidden region changed")
        require((result["scene_flags"] & 0x100) == 0, f"{name}: native dirty request was not consumed")
        require(admissions[1]["geometric_calls"] == [0, 0], f"{name}: hidden geometry reached geometric tests")
        cases.append(dict(name=name, **result, admissions=admissions))

    case("native_mono_baseline", False, True, [0x82, 0x81, 0x86], [1, 0, 3], 1)
    case("second_view_without_refresh_retains_mono_cache", True, False, [0x82, 0x81, 0x86], [1, 0, 3], 0)
    require(cases[-1]["region_flags"][0] & 0x60 == 0x60,
            "Second camera failed to update upstream region admission")
    case("second_view_with_refresh_rebuilds_cache", True, True, [0x86, 0x81, 0x86], [3, 0, 3], 1)
    case("stable_two_views_need_no_repeated_refresh", True, False, [0x86, 0x81, 0x86], [3, 0, 3], 0)
    case("return_to_mono_with_refresh", False, True, [0x82, 0x81, 0x86], [1, 0, 3], 1)
    native.region_membership = [False, True]
    case("region_only_contains_second_camera", True, True, [0x84, 0x81, 0x86], [2, 0, 3], 1)
    native.region_membership = [False, False]
    case("neither_camera_in_region", True, True, [0x81, 0x81, 0x86], [0, 0, 3], 1)
    native.region_membership = [True, True]
    native.geometric_accept = [True, False]
    case("refreshed_membership_keeps_independent_geometric_rejection", True, True,
         [0x86, 0x81, 0x86], [1, 0, 1], 1)
    native.geometric_accept = [True, True]
    for flag in (0x200, 0x400):
        native.write(native.scene + 0x110, "<I", flag | 0x1000)
        case(f"existing_native_dirty_flag_{flag:x}", True, False,
             [0x86, 0x81, 0x86], [3, 0, 3], 1)
    require(native.calls["enter_lock"] == native.calls["leave_lock"] == len(cases),
            "Native lock callbacks were not balanced")
    return cases


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", type=Path, default=ROOT / "out/deps/re-tools/inputs/halo1.dll")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    raw = args.image.read_bytes()
    require(hashlib.sha256(raw).hexdigest().upper() == SHA, "Pinned image SHA-256 mismatch")
    native = NativeScene(pefile.PE(data=raw))
    cases = verify(native)
    report = dict(status="PASS_NATIVE_SCENE_REFRESH_AND_ADMISSION", image_sha256=SHA,
                  case_count=len(cases), instructions=native.instructions, calls=dict(native.calls),
                  executed_functions={name: [hex(start), hex(end)] for name, (start, end) in FUNCTIONS.items()},
                  cases=cases,
                  modeled_dependencies=["region volume membership", "bounds/frustum geometric outcomes",
                      "critical section imports", "progress and event callbacks", "security-cookie check"],
                  limit="Native cache refresh and admission only. No native world draw, runtime scene chronology, GPU, or headset image is executed.")
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({key: value for key, value in report.items() if key != "cases"}, indent=2))


if __name__ == "__main__":
    main()
