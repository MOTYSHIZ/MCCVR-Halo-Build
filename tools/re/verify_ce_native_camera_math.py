"""Run pinned CE camera instructions in an isolated x64 emulator.

This never loads the game DLL into a Windows process, runs its entry point, or
opens a game process. The engine image is read-only emulator memory. Only the
camera/constant routines below may execute; CRT sqrtf/tanf/atanf and the stack
cookie check are explicitly modeled. Optional integration starts with the real
CE-to-Saber producer, runs the production C++ tracking adapter, and executes the
native CPU shader-constant writer and the queued mesh worker's camera-setup
prefix. The worker stops before command-list/D3D operations. This does not run
scene shaders, GPU resource routing, or headset rendering.

Dependencies: pefile and unicorn (install into the existing ignored out/pydeps).
"""

import argparse
import hashlib
import json
import math
import struct
import subprocess
from pathlib import Path

import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64, UC_HOOK_CODE, UC_PROT_READ, UC_PROT_EXEC
from unicorn.x86_const import (
    UC_X86_REG_RSP, UC_X86_REG_RIP, UC_X86_REG_RCX, UC_X86_REG_RDX,
    UC_X86_REG_R8, UC_X86_REG_R9, UC_X86_REG_XMM0, UC_X86_REG_XMM2,
    UC_X86_REG_XMM3,
)


PINNED_SHA256 = "0A12DC561780F449D3F4D0DF10BB8D3BC7BE7840A5BEB2B236F672EB6CD42E6C"
BASE = 0x180000000
SCRATCH = 0x200000000
STACK = SCRATCH + 0x60000
RETURN = SCRATCH + 0xF000
CAMERA = SCRATCH + 0x1000
# Renderer fields reach +BE98; keep its full range separate from every camera,
# backend object and constant buffer so aliasing cannot hide an erroneous write.
RENDERER = SCRATCH + 0x20000
PROJECTION = SCRATCH + 0x4000
CAMERA_ARRAY = SCRATCH + 0x5000
NATIVE_POSITION = SCRATCH + 0x5100
NATIVE_UP = SCRATCH + 0x5120
NATIVE_FORWARD = SCRATCH + 0x5140
SCENE = SCRATCH + 0x6000
BACKEND_CONFIG = SCRATCH + 0x7000
BACKEND = SCRATCH + 0x8000
CONSTANT_BLOCK = SCRATCH + 0x9000
SHADER_DATA = SCRATCH + 0x10000
WORLD_BATCH = SCRATCH + 0x12000


def dot(a, b):
    return sum(x * y for x, y in zip(a, b))


def plus(a, b):
    return tuple(x + y for x, y in zip(a, b))


def scale(a, value):
    return tuple(x * value for x in a)


def cross(a, b):
    return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])


def rotate(v, axis, angle):
    c, s = math.cos(angle), math.sin(angle)
    return plus(plus(scale(v, c), scale(cross(axis, v), s)), scale(axis, dot(axis, v)*(1-c)))


def transform(point, matrix):
    values = (*point, 1)
    return tuple(sum(values[row]*matrix[4*row+column] for row in range(4)) for column in range(4))


class NativeCamera:
    # Bodies are bounded by the pinned executable's unwind entries or the
    # disassembled return for the leaf bounding-box helper.
    CODE_RANGES = ((0x11ABA0, 0x11ADA6), (0x11AE90, 0x11AFF5),
                   (0x11E5F0, 0x11F3E6), (0x12AB90, 0x12AE5E),
                   (0x7B480, 0x7B7EC), (0x11B020, 0x11B087),
                   (0x11B0B0, 0x11B117),
                   (0x2EB9F0, 0x2EBBD6), (0x2EBBE0, 0x2EBF9C),
                   (0x2351E0, 0x2357AD), (0x235860, 0x235948),
                   (0x2357B0, 0x2357F6), (0x310930, 0x310CA4),
                   (0x496D0, 0x496D3),
                   (0x2EBFA0, 0x2EC20E), (0xF9B80, 0xF9CCB),
                   (0xFED50, 0xFF468))

    def __init__(self, path):
        data = path.read_bytes()
        digest = hashlib.sha256(data).hexdigest().upper()
        if digest != PINNED_SHA256:
            raise ValueError(f"Pinned CE image mismatch: {digest}")
        pe = pefile.PE(data=data, fast_load=True)
        if pe.OPTIONAL_HEADER.ImageBase != BASE:
            raise ValueError("Unexpected CE image base")
        self.machine = Uc(UC_ARCH_X86, UC_MODE_64)
        size = (pe.OPTIONAL_HEADER.SizeOfImage + 0xFFF) & ~0xFFF
        self.machine.mem_map(BASE, size)
        self.machine.mem_write(BASE, pe.get_memory_mapped_image())
        self.machine.mem_protect(BASE, size, UC_PROT_READ | UC_PROT_EXEC)
        self.machine.mem_map(SCRATCH, 0x80000)
        self.machine.hook_add(UC_HOOK_CODE, self.on_instruction)
        self.instructions = 0
        self.calls = 0
        self.math_calls = {"sqrtf": 0, "sqrt": 0, "tanf": 0, "atanf": 0}
        self.worker_prefix = False

    def on_instruction(self, machine, address, size, data):
        self.instructions += 1
        rva = address-BASE
        if address == RETURN:
            machine.emu_stop()
            return
        if self.worker_prefix and rva == 0x310CA4:
            # Stop after the real world/FP constant writes, before worker
            # state setup, thread APIs, or command-list/D3D operations. This
            # is explicitly a native prefix test, not a completed scene job.
            machine.reg_write(UC_X86_REG_RIP, RETURN)
            machine.emu_stop()
            return
        if rva == 0x7B4F8:
            # The producer clears an ancillary native camera/debug status byte.
            # Its value does not feed the camera arithmetic. Keep the image
            # read-only and model this one side effect instead of writing it.
            machine.reg_write(UC_X86_REG_RIP, address+7)
            return
        if rva in (0x1607041, 0x1607047, 0x1607053, 0x1722E00, 0x1391640):
            if rva == 0x1607041:
                value = struct.unpack("<d", struct.pack("<Q", machine.reg_read(UC_X86_REG_XMM0) & 0xFFFFFFFFFFFFFFFF))[0]
                self.math_calls["sqrt"] += 1
                machine.reg_write(UC_X86_REG_XMM0, struct.unpack("<Q", struct.pack("<d", math.sqrt(value)))[0])
            elif rva != 0x1391640:
                value = struct.unpack("<f", struct.pack("<I", machine.reg_read(UC_X86_REG_XMM0) & 0xFFFFFFFF))[0]
                name, operation = {0x1607047: ("tanf", math.tan),
                                   0x1607053: ("sqrtf", math.sqrt),
                                   0x1722E00: ("atanf", math.atan)}[rva]
                self.math_calls[name] += 1
                machine.reg_write(UC_X86_REG_XMM0, struct.unpack("<I", struct.pack("<f", operation(value)))[0])
            stack = machine.reg_read(UC_X86_REG_RSP)
            destination = struct.unpack("<Q", machine.mem_read(stack, 8))[0]
            machine.reg_write(UC_X86_REG_RSP, stack+8)
            machine.reg_write(UC_X86_REG_RIP, destination)
            return
        if not any(begin <= rva < end for begin, end in self.CODE_RANGES):
            raise RuntimeError(f"Unexpected native execution at {rva:#x}")

    def call(self, rva, args=(), float_args=()):
        machine = self.machine
        # Windows x64 entry RSP is 8 modulo 16; reserve caller shadow space.
        stack = STACK-8
        machine.mem_write(stack, struct.pack("<Q", RETURN))
        machine.reg_write(UC_X86_REG_RSP, stack)
        for index, argument in enumerate(args):
            if index < 4:
                machine.reg_write((UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_R9)[index], argument)
            else:
                machine.mem_write(stack+0x28+(index-4)*8, struct.pack("<Q", argument))
        for register, value in float_args:
            machine.reg_write(register, struct.unpack("<I", struct.pack("<f", value))[0])
        machine.emu_start(BASE+rva, RETURN, timeout=1_000_000, count=50_000)
        if machine.reg_read(UC_X86_REG_RIP) != RETURN:
            raise RuntimeError("Native camera instruction budget exceeded")
        self.calls += 1

    def build(self, position, right, up, forward, width=2912, height=1050, half_y=.98):
        camera = bytearray(0x398)
        pose = (*right, 0, *up, 0, *forward, 0, *position, 1)
        struct.pack_into("<16f", camera, 0, *pose)
        struct.pack_into("<2f", camera, 0x80, .025, 10000)
        half_x = math.atan(math.tan(half_y)*width/height)
        struct.pack_into("<7f", camera, 0x13C, 0, 0, width, height,
                         math.degrees(half_x)*2, math.degrees(half_y)*2, height/width)
        self.machine.mem_write(CAMERA, bytes(camera))
        return self.rebuild()

    def rebuild(self):
        self.machine.mem_write(RENDERER+0x90, struct.pack("<f", 1))
        self.call(0x11ABA0, (CAMERA,))
        self.call(0x11AE90, (CAMERA,))
        self.call(0x11E5F0, (CAMERA,))
        self.call(0x2EBFA0, (RENDERER, CAMERA, 0, 0, PROJECTION),
                  ((UC_X86_REG_XMM2, .025), (UC_X86_REG_XMM3, 10000)))
        rebuilt = self.machine.mem_read(CAMERA, 0x398)
        view = struct.unpack_from("<16f", rebuilt, 0x40)
        projection = struct.unpack("<16f", self.machine.mem_read(PROJECTION, 64))
        self.corners = [struct.unpack_from("<3f", rebuilt, 0x318+index*12) for index in range(8)]
        self.bounds = struct.unpack_from("<6f", rebuilt, 0x378)
        return view, projection, math.radians(struct.unpack_from("<f", rebuilt, 0x14C)[0])*.5

    def produce(self, position, up, forward, width, height, offset, bias, vertical=0):
        # Seed only the source objects/configuration that the real producer
        # reads; its pose and view fields start poisoned, never preconstructed.
        camera = bytearray(b"\xCD"*0x398)
        struct.pack_into("<2f", camera, 0x80, .025, 10000)
        struct.pack_into("<5f", camera, 0x13C, 0, 0, width, height, 100)
        struct.pack_into("<f", camera, 0x154, height/width)
        self.machine.mem_write(CAMERA, bytes(camera))
        self.machine.mem_write(CAMERA_ARRAY, struct.pack("<Q", CAMERA))
        self.machine.mem_write(NATIVE_POSITION, struct.pack("<3f", *position))
        self.machine.mem_write(NATIVE_UP, struct.pack("<3f", *up))
        self.machine.mem_write(NATIVE_FORWARD, struct.pack("<3f", *forward))
        # Host-side setup seeds synthetic globals; emulated native instructions
        # still cannot write the protected mapped image.
        for rva, value in ((0x2B17B90, struct.pack("<Q", CAMERA_ARRAY)),
                           (0x2B17B98, struct.pack("<I", 1)),
                           (0x2E3C418, struct.pack("<Q", SCENE)),
                           (0x2E3B87E, b"\0"), (0x2E3B826, b"\0"),
                           (0x2B05118, struct.pack("<3f", *offset)),
                           (0x2E3B838, struct.pack("<f", bias))):
            self.machine.mem_write(BASE+rva, value)
        self.machine.mem_write(SCENE+0x130, struct.pack("<Q", 0))
        self.call(0x7B480, (0, NATIVE_POSITION, NATIVE_UP, NATIVE_FORWARD,
                  struct.unpack("<I", struct.pack("<f", 100))[0],
                  struct.unpack("<I", struct.pack("<f", vertical))[0]))
        return bytes(self.machine.mem_read(CAMERA, 0x398))

    def upload(self, renderer_flags=1):
        # Renderer creation 4F05F6 assigns this exact vtable. Its +48 slot is
        # 2351E0; that calls +110=235860 to write the real shader constant layout.
        # No D3D operation is modeled as an upload: native instructions write
        # the synthetic CPU-side constant storage themselves.
        self.machine.mem_write(BASE+0x1BEA9E0, struct.pack("<Q", RENDERER))
        self.machine.mem_write(BASE+0x2E3BDD8, struct.pack("<Q", BACKEND_CONFIG))
        self.machine.mem_write(RENDERER, struct.pack("<Q", BASE+0x1815378))
        self.machine.mem_write(RENDERER+0x9C, struct.pack("<I", renderer_flags))
        self.machine.mem_write(BACKEND_CONFIG+0x128, struct.pack("<I", 1 << 26))
        self.machine.mem_write(BACKEND+0xD8, struct.pack("<Q", CONSTANT_BLOCK))
        self.machine.mem_write(CONSTANT_BLOCK+8, struct.pack("<Q", SHADER_DATA))
        self.machine.mem_write(SHADER_DATA, b"\xCD"*0x400)
        self.call(0x2EB9F0, (0, BACKEND, CAMERA))
        selected = struct.unpack("<Q", self.machine.mem_read(RENDERER+0xBE98, 8))[0]
        if selected != CAMERA:
            raise AssertionError("Native uploader did not select its supplied camera")
        origin = struct.unpack("<4f", self.machine.mem_read(SHADER_DATA+0x240, 16))
        packed = struct.unpack("<16f", self.machine.mem_read(SHADER_DATA+0x270, 64))
        # Native CPU storage contains a transposed rotation-only projection
        # and a separate origin. Check the camera-relative convention those
        # values imply; actual shader subtraction remains outside this test.
        matrix = tuple(packed[column*4+row] for row in range(4) for column in range(4))
        return origin, matrix

    def prepare_worker_constants(self, eye):
        # Native3112E0 sets batch+38 to an active view record. Use two real
        # array slots and their distinct native primary flags;310930 performs
        # the actual view-pointer/camera selection and both constant uploads.
        view = RENDERER+0xC0+eye*0x3C8
        self.machine.mem_write(view, bytes(0x30))
        self.machine.mem_write(view, struct.pack("<I", 0x10B if eye == 0 else 0x20B))
        self.machine.mem_write(view+0xC, struct.pack("<I", eye))
        self.machine.mem_write(view+0x30, bytes(self.machine.mem_read(CAMERA, 0x398)))
        self.machine.mem_write(WORLD_BATCH, bytes(0x118))
        self.machine.mem_write(WORLD_BATCH+0x38, struct.pack("<Q", view))
        self.machine.mem_write(SHADER_DATA, b"\xCD"*0x400)
        self.worker_prefix = True
        try:
            self.call(0x310930, (0, WORLD_BATCH, BACKEND))
        finally:
            self.worker_prefix = False
        origin = struct.unpack("<4f", self.machine.mem_read(SHADER_DATA+0x320, 16))
        matrices = []
        for offset in (0x270, 0x2B0):
            packed = struct.unpack("<16f", self.machine.mem_read(SHADER_DATA+offset, 64))
            matrices.append(tuple(packed[column*4+row] for row in range(4) for column in range(4)))
        return origin, *matrices


def to_saber(vector):
    return (vector[0], vector[2], -vector[1])


def verify_bridge(native, executable, output_dir):
    records = []
    checks = 0
    for position in ((0, 0, 0), (32.45, -99.76, 59.27), (3300, -820, 230)):
        for yaw, pitch in ((0, 0), (.71, -.32), (-1.95, .63)):
            forward, up = (1, 0, 0), (0, 0, 1)
            for axis, angle in (((0, 0, 1), yaw), ((0, 1, 0), pitch)):
                forward, up = rotate(forward, axis, angle), rotate(up, axis, angle)
            right = cross(forward, up)
            for width, height in ((2912, 1050), (1920, 1080), (2048, 2048)):
                for offset, bias, vertical in (((0, 0, 0), 0, 0), ((12, -3, 9), .3, 67)):
                    camera = native.produce(position, up, forward, width, height, offset, bias, vertical)
                    pose = struct.unpack_from("<16f", camera)
                    saber_right, saber_up, saber_forward = (to_saber(axis) for axis in (right, up, forward))
                    expected_position = plus(plus(scale(to_saber(position), 3.048), offset), scale(saber_forward, bias))
                    expected_pose = (*saber_right, 0, *saber_up, 0, *saber_forward, 0, *expected_position, 1)
                    if max(abs(a-b) for a, b in zip(pose, expected_pose)) > .002:
                        raise AssertionError(f"Native CE-to-Saber producer convention mismatch: {pose=} {expected_pose=}")
                    view = struct.unpack_from("<16f", camera, 0x40)
                    if max(abs(value) for value in transform(pose[12:15], view)[:3]) > .002:
                        raise AssertionError("Produced camera does not map its own origin to zero")
                    records.append((camera, pose))
                    checks += 1
    output_dir.mkdir(parents=True, exist_ok=True)
    source, staged = output_dir/"native-cameras.bin", output_dir/"staged-eyes.bin"
    source.write_bytes(struct.pack("<I", len(records))+b"".join(camera for camera, _ in records))
    subprocess.run([str(executable.resolve()), "--native-camera-adapter", str(source.resolve()), str(staged.resolve())], check=True)
    data = staged.read_bytes()
    if len(data) != 4+len(records)*2*(0x398+12) or struct.unpack_from("<I", data)[0] != len(records):
        raise AssertionError("Production adapter output count/length mismatch")
    cursor = 4
    for _, producer_pose in records:
        stock_right, stock_up, stock_forward = producer_pose[0:3], producer_pose[4:7], producer_pose[8:11]
        center = producer_pose[12:15]
        angle = math.pi/6
        forward = plus(scale(stock_right, -math.sin(angle)), scale(stock_forward, math.cos(angle)))
        up = stock_up
        right = cross(forward, up)
        for eye in range(2):
            native.machine.mem_write(CAMERA, data[cursor:cursor+0x398])
            pose = struct.unpack_from("<16f", data, cursor)
            cover = struct.unpack_from("<3f", data, cursor+0x398)
            cursor += 0x398+12
            offset_x = -.0355 if eye == 0 else .0355
            local = (.21+offset_x*math.cos(angle), .13, -.17-offset_x*math.sin(angle))
            position = plus(center, plus(plus(scale(stock_right, local[0]), scale(stock_up, local[1])), scale(stock_forward, -local[2])))
            expected_pose = (*right, 0, *up, 0, *forward, 0, *position, 1)
            if max(abs(a-b) for a, b in zip(pose, expected_pose)) > .003:
                raise AssertionError("Production tracked eye disagrees with native producer convention")
            view, projection, half_x = native.rebuild()
            if abs(half_x-cover[1]) > 1e-5:
                raise AssertionError("Native consumed horizontal FOV disagrees with production cover")
            # Project an independently constructed point in front of the
            # requested tracked eye, using native derived/consumed matrices.
            world = plus(position, plus(scale(forward, 3), plus(scale(right, .3), scale(up, -.2))))
            local_point = transform(world, view)
            clip = transform(local_point[:3], projection)
            expected_ndc = (.3/(3*math.tan(cover[1])), -.2/(3*math.tan(cover[2])))
            if max(abs(clip[index]/clip[3]-expected_ndc[index]) for index in range(2)) > .001:
                raise AssertionError("Native producer -> production adapter -> native consumer projection mismatch")
            origin, shader_matrix = native.upload()
            if max(abs(origin[index]-pose[12+index]) for index in range(3)) > 1e-5 or origin[3] != 1:
                raise AssertionError("Native shader constant camera origin disagrees with the staged eye")
            relative = tuple(world[index]-origin[index] for index in range(3))
            shader_clip = transform(relative, shader_matrix)
            if max(abs(shader_clip[index]/shader_clip[3]-expected_ndc[index]) for index in range(2)) > .001:
                raise AssertionError("Native shader constant matrix disagrees with tracked-eye geometry")
            worker_origin, worker_world, worker_fp = native.prepare_worker_constants(eye)
            if max(abs(a-b) for a, b in zip(worker_origin, origin)) > 1e-5:
                raise AssertionError("Native queued worker selected a different eye origin")
            if max(abs(a-b) for a, b in zip(worker_world, shader_matrix)) > 1e-5:
                raise AssertionError("Native queued worker disagrees with the main world projection")
            fp_clip = transform(relative, worker_fp)
            # Native310930 replaces the private FP camera's VERTICAL FOV.
            # This checks the observed alternate lens, not a desired VR fix.
            fp_half_y = math.radians(struct.unpack("<f", struct.pack("<I", 0x425DA6D7))[0])*.5
            aspect = struct.unpack("<f", native.machine.mem_read(CAMERA+0x154, 4))[0]
            fp_half_x = math.atan(math.tan(fp_half_y)/aspect)
            fp_ndc = (.3/(3*math.tan(fp_half_x)), -.2/(3*math.tan(fp_half_y)))
            if max(abs(fp_clip[index]/fp_clip[3]-fp_ndc[index]) for index in range(2)) > .001:
                raise AssertionError("Native queued worker FP projection does not match its fixed FOV")
            if abs(fp_clip[1]/fp_clip[3]-expected_ndc[1]) < .01:
                raise AssertionError("Fixture failed to distinguish native fixed FP lens from the tracked world lens")
            checks += 1
    return {"status": "PASS", "native_producer_cameras": len(records), "checks": checks,
            "queued_worker_eye_setups": len(records)*2,
            "queued_worker_world_matches_main": True,
            "queued_worker_fp_uses_fixed_vertical_fov": True,
            "source": str(source), "staged": str(staged),
            "scope": "Native producer, production C++ adapter, native camera/CPU constants and queued-worker camera prefix; stops before D3D command-list begin; no GPU or scene draw"}


def verify_oblique_projection(native):
    # Native surface callbacks set renderer mode 0x20 for oblique clipping.
    # The failed runtime's camera pointer/prefix ledger does not inspect this
    # mode. Execute its real matrix inverse and uploader instead of assuming
    # that the ordinary upload branch describes every native projection.
    checks = 0
    worst_xyw_error = 0.0
    for position in ((0, 0, 0), (98.925, 180.641, 304.072)):
        for yaw in (0.0, .71):
            right = rotate((1, 0, 0), (0, 1, 0), yaw)
            forward = rotate((0, 0, 1), (0, 1, 0), yaw)
            native.build(position, right, (0, 1, 0), forward)
            normal_origin, normal_matrix = native.upload()
            for point, normal in (((0, -10, 0), (0, 1, 0)),
                                  ((0, 2, 0), (0, 1, 0)),
                                  ((0, 0, 10), (0, 0, 1)),
                                  ((10, 0, 0), (1, 0, 0))):
                native.machine.mem_write(RENDERER+0x10, struct.pack("<6f", *point, *normal))
                native.machine.mem_write(BASE+0x2E3BB88, struct.pack("<3f", *position))
                origin, matrix = native.upload(renderer_flags=0x21)
                if origin != normal_origin or not all(math.isfinite(value) for value in matrix):
                    raise AssertionError("Native oblique projection changed the origin or produced a nonfinite matrix")
                error = max(abs(matrix[index]-normal_matrix[index])
                            for index in range(16) if index % 4 != 2)
                worst_xyw_error = max(worst_xyw_error, error)
                if error > 1e-5:
                    raise AssertionError("Native oblique projection changed clip x/y/w")
                if max(abs(matrix[index]-normal_matrix[index]) for index in (2, 6, 10, 14)) < 1e-5:
                    raise AssertionError("Native oblique branch did not change clip depth")
                checks += 1
    return {"checks": checks, "worst_clip_xyw_error": worst_xyw_error,
            "result": "Mode 0x20 changes clip depth only in these cases; it does not displace the supplied world camera",
            "scope": "Native CPU matrix result with synthetic planes; runtime mode and scene raster remain unobserved"}


def verify(path, adapter_exe=None, output_dir=None):
    native = NativeCamera(path)
    checks = 0
    worst_view_error = 0.0
    worst_projection_error = 0.0
    # Include the far-world coordinates from the failed user's diagnostic and
    # non-axis-aligned yaw/pitch/roll; identity-only matrices miss translation.
    for position in ((0, 0, 0), (98.925, 180.641, 304.072), (10000, -2500, 700)):
        for angles in ((0, 0, 0), (.71, -.32, .19), (-1.95, .63, -.41)):
            right, up, forward = (0, 0, 1), (0, 1, 0), (1, 0, 0)
            for axis, angle in zip(((0, 1, 0), (0, 0, 1), (1, 0, 0)), angles):
                right, up, forward = [rotate(vector, axis, angle) for vector in (right, up, forward)]
            for width, height in ((2912, 1050), (1920, 1080), (2048, 2048)):
                view, projection, half_x = native.build(position, right, up, forward, width, height)
                if not all(math.isfinite(value) for point in native.corners for value in point):
                    raise AssertionError("Nonfinite native culling corners")
                for axis in range(3):
                    low = min(point[axis] for point in native.corners)
                    high = max(point[axis] for point in native.corners)
                    if native.bounds[axis] != low or native.bounds[axis+3] != high:
                        raise AssertionError("Native bounding box does not enclose its eight corners")
                for index, corner in enumerate(native.corners):
                    local = transform(corner, view)
                    depth = .025 if index < 4 else 10000
                    expected = (depth*math.tan(half_x), depth*math.tan(.98), depth)
                    # The widest far corner is over 40,000 native Saber units;
                    # allow its float-precision error without hiding pose errors.
                    tolerance = .003 if index < 4 else .04
                    if any(abs(abs(local[axis])-expected[axis]) > tolerance for axis in range(3)):
                        raise AssertionError(f"Native culling corner disagrees with view/FOV: {local=} {expected=}")
                checks += 1
                for local in ((0, 0, 0), (0, 0, .025), (0, 0, 1), (.25, -.3, 3), (-3, 2, 20)):
                    world = plus(position, plus(plus(scale(right, local[0]), scale(up, local[1])), scale(forward, local[2])))
                    observed = transform(world, view)
                    error = max(abs(observed[index]-local[index]) for index in range(3))
                    worst_view_error = max(worst_view_error, error)
                    if error > .003 or abs(observed[3]-1) > 1e-5:
                        raise AssertionError(f"Native view inverse mismatch: {position=} {angles=} {local=} {observed=}")
                    if local[2] > .5:
                        clip = transform(local, projection)
                        ndc = (clip[0]/clip[3], clip[1]/clip[3])
                        expected = (local[0]/(local[2]*math.tan(half_x)), local[1]/(local[2]*math.tan(.98)))
                        error = max(abs(a-b) for a, b in zip(ndc, expected))
                        worst_projection_error = max(worst_projection_error, error)
                        if error > 1e-5:
                            raise AssertionError(f"Native projection mismatch: {ndc=} {expected=}")
                    checks += 1
                # Eye translation must move both world geometry and depth with
                # identical sign/scale, and create symmetric horizontal disparity.
                world = plus(position, scale(forward, 2))
                eye_ndc = []
                for displacement in (-.0355, .0355):
                    eye_position = plus(position, scale(right, displacement))
                    eye_view, eye_projection, _ = native.build(eye_position, right, up, forward, width, height)
                    clip = transform(transform(world, eye_view)[:3], eye_projection)
                    eye_ndc.append((clip[0]/clip[3], clip[1]/clip[3]))
                expected_disparity = .071/(2*math.tan(half_x))
                observed_disparity = eye_ndc[0][0]-eye_ndc[1][0]
                if abs(observed_disparity-expected_disparity) > .0015 or abs(eye_ndc[0][1]-eye_ndc[1][1]) > .0015:
                    raise AssertionError(f"Native stereo disparity mismatch: {eye_ndc=} {expected_disparity=}")
                checks += 1
    oblique = verify_oblique_projection(native)
    bridge = verify_bridge(native, adapter_exe, output_dir) if adapter_exe else None
    return {"status": "PASS", "pinned_sha256": PINNED_SHA256, "checks": checks,
            "native_calls": native.calls, "native_instructions": native.instructions,
            "modeled_crt_calls": native.math_calls,
            "worst_view_error_saber_units": worst_view_error,
            "worst_projection_error_ndc": worst_projection_error,
            "native_oblique_projection": oblique,
            "native_producer_production_adapter": bridge,
            "scope": "Native camera arithmetic and optional production adapter/CPU constants; no scene shader, GPU resource, or headset result"}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    parser.add_argument("--adapter-exe", type=Path,
                        help="Built ce_view_pair_tests executable for native producer -> production C++ adapter verification")
    parser.add_argument("--output-dir", type=Path, default=Path("out/ce-native-camera-bridge"))
    args = parser.parse_args()
    print(json.dumps(verify(args.image, args.adapter_exe, args.output_dir), indent=2))
