"""Execute CE Anniversary FP player visibility production and view admission.

The complete 0x7B2E0 visibility scheduler executes. Its model/palette bridge
calls are reduced to the actual 0x7AFB1 source-player visibility branch after
modeled model lookup; palette conversion, locks and allocation are unrelated
to the tested predicate. The native 0x2CFF20..0x2D019D first-person admission
loop then consumes the resulting flags with both actual view indices and the
private source-player correction. No game process, DLL entry or GPU is used.
"""
from __future__ import annotations

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
from unicorn.x86_const import (UC_X86_REG_RAX, UC_X86_REG_RCX, UC_X86_REG_RDX,
    UC_X86_REG_RBX, UC_X86_REG_RSI, UC_X86_REG_R11, UC_X86_REG_R13,
    UC_X86_REG_R14, UC_X86_REG_R15, UC_X86_REG_RSP, UC_X86_REG_RIP,
    UC_X86_REG_R8, UC_X86_REG_R9)

from verify_ce_render_evidence import verify

SHA = "0A12DC561780F449D3F4D0DF10BB8D3BC7BE7840A5BEB2B236F672EB6CD42E6C"
BASE, HEAP, STACK, STOP = 0x180000000, 0x60000000, 0x70000000, 0x71000000


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


class NativeVisibility:
    def __init__(self, image):
        self.uc = Uc(UC_ARCH_X86, UC_MODE_64)
        self.size = (len(image) + 4095) & ~4095
        self.uc.mem_map(BASE, self.size)
        self.uc.mem_write(BASE, image)
        self.uc.mem_map(HEAP, 0x20000)
        self.uc.mem_map(STACK, 0x10000)
        self.uc.mem_map(STOP, 0x1000)
        self.record, self.model, self.backend, self.views = (HEAP + offset for offset in
            (0x1000, 0x2000, 0x3000, 0x4000))
        self.write(BASE + 0x2B050E8, "<Q", self.record)
        self.write(BASE + 0x2B050F0, "<I", 1)
        self.write(BASE + 0x2E3BDD8, "<Q", self.backend)
        self.write(BASE + 0x2E3B8F1, "<B", 0)
        self.uc.hook_add(UC_HOOK_CODE, self.instruction)
        self.instructions, self.bridge_calls = 0, 0
        self.bridge_saved = None

    def write(self, address, fmt, *values):
        self.uc.mem_write(address, struct.pack(fmt, *values))

    def read(self, address, fmt):
        return struct.unpack(fmt, self.uc.mem_read(address, struct.calcsize(fmt)))

    def native_return(self):
        sp = self.uc.reg_read(UC_X86_REG_RSP)
        self.uc.reg_write(UC_X86_REG_RIP, self.read(sp, "<Q")[0])
        self.uc.reg_write(UC_X86_REG_RSP, sp + 8)

    def instruction(self, uc, address, size, _):
        self.instructions += 1
        if address == STOP or address == BASE + 0x2D019D:
            uc.emu_stop()
            return
        if address == BASE + 0x7AC60:
            tag, player = (uc.reg_read(reg) for reg in (UC_X86_REG_RCX, UC_X86_REG_RDX))
            if not tag:
                self.native_return()
                return
            require(tag == 0x1234 and player == self.source_player, "Unexpected model lookup")
            self.bridge_calls += 1
            self.bridge_saved = [uc.reg_read(reg) for reg in (UC_X86_REG_RSI, UC_X86_REG_R15)]
            uc.reg_write(UC_X86_REG_RSI, self.record)
            uc.reg_write(UC_X86_REG_R15, player)
            uc.reg_write(UC_X86_REG_RIP, BASE + 0x7AFB1)
            return
        if address == BASE + 0x7AFCD:
            require(self.bridge_saved is not None, "Visibility branch without modeled model lookup")
            for reg, value in zip((UC_X86_REG_RSI, UC_X86_REG_R15), self.bridge_saved):
                uc.reg_write(reg, value)
            self.bridge_saved = None
            self.native_return()
            return
        rva = address - BASE
        require(0x7B2E0 <= rva <= 0x7B470 or 0x7AFB1 <= rva < 0x7AFCD or
            0x7B080 <= rva < 0x7B092 or 0x2CFF20 <= rva < 0x2D019D,
            f"Unmodeled native instruction {address:x}")

    def produce(self, source_player, backend_stereo, active):
        self.source_player = source_player
        self.uc.mem_write(self.record, bytes(0x20))
        self.uc.mem_write(self.model, bytes(0x400))
        self.write(self.record, "<II", source_player, 0x1234)
        self.write(self.record + 0x10, "<Q", self.model)
        self.write(self.model + 4, "<I", 0x4400)
        self.write(self.model + 0x18, "<I", 6)
        self.write(self.backend + 0x238, "<I", backend_stereo)
        for player in (0, 1):
            self.write(BASE + 0x1B7AA88 + player * 4, "<I",
                0x1234 if active and player == source_player else 0)
            self.write(BASE + 0x1B7AA98 + player * 4, "<I", 0)
        sp = STACK + 0xF008
        self.write(sp, "<Q", STOP)
        self.uc.reg_write(UC_X86_REG_RSP, sp)
        self.uc.emu_start(BASE + 0x7B2E0, STOP, count=10000)
        require(self.uc.reg_read(UC_X86_REG_RIP) == STOP, "Native producer did not return")
        return self.read(self.model + 8, "<I")[0]

    def consume(self, indices, included=6):
        self.uc.mem_write(self.views, bytes(0xBDE8))
        self.write(self.views + 8, "<I", len(indices))
        self.write(self.model + 0x18, "<I", included)
        self.write(self.model + 0x220, "<Q", 0)
        for eye, index in enumerate(indices):
            self.write(self.views + 0x10 + eye * 0x3C8, "<I", 0x10B if eye == 0 else 0x20B)
            self.write(self.views + 0x1C + eye * 0x3C8, "<I", index)
        for reg, value in ((UC_X86_REG_R15, self.views), (UC_X86_REG_R13, self.model),
            (UC_X86_REG_R14, 0), (UC_X86_REG_R11, 0), (UC_X86_REG_RBX, 0)):
            self.uc.reg_write(reg, value)
        self.uc.emu_start(BASE + 0x2CFF20, BASE + 0x2D019D, count=10000)
        return self.read(self.model + 0x220, "<Q")[0]


def run_native_submission(image, copied, reduce_count, phase):
    """Execute native copy/callsite and complete submit; model named services.

    Native memcpy arguments execute, while the memcpy implementation itself is
    a byte-copy fixture. Worker jobs are observed at their dispatch endpoint;
    neither asynchronous worker execution nor production receipts are modeled.
    """
    uc = Uc(UC_ARCH_X86, UC_MODE_64)
    uc.mem_map(BASE, (len(image) + 4095) & ~4095)
    uc.mem_write(BASE, image)
    uc.mem_map(HEAP, 0x40000)
    uc.mem_map(STACK, 0x20000)
    uc.mem_map(STOP, 0x1000)
    source, destination, state, engine = (HEAP + offset for offset in
        (0x1000, 0x10000, 0x20000, 0x3a000))
    instructions, copies, jobs = 0, [], []

    def write(at, fmt, *values): uc.mem_write(at, struct.pack(fmt, *values))
    def read(at, fmt): return struct.unpack(fmt, uc.mem_read(at, struct.calcsize(fmt)))
    def ret(value=0):
        sp = uc.reg_read(UC_X86_REG_RSP)
        uc.reg_write(UC_X86_REG_RAX, value)
        uc.reg_write(UC_X86_REG_RIP, read(sp, "<Q")[0])
        uc.reg_write(UC_X86_REG_RSP, sp + 8)

    def instruction(_, address, size, user):
        nonlocal instructions
        instructions += 1
        rva = address - BASE
        if address in (STOP, BASE + 0x455289, BASE + 0x45550b):
            uc.emu_stop()
        elif rva == 0x1606fb1:
            target, origin, count = (uc.reg_read(reg) for reg in
                (UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8))
            require((target, origin, count) == (destination, source, 0xbde8),
                "Native complete-list copy arguments changed")
            uc.mem_write(target, bytes(uc.mem_read(origin, count)))
            copies.append(dict(source=hex(origin), destination=hex(target), bytes=count))
            ret(target)
        elif rva == 0x139362f:
            target, byte, count = (uc.reg_read(reg) for reg in
                (UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8))
            require((target, byte, count) == (state + 0x18b08, 0xffffffff, 0x100),
                "Unexpected submit memset service")
            uc.mem_write(target, bytes([byte & 0xff]) * count)
            ret(target)
        elif rva == 0x1391640:
            ret()  # Fixture security-cookie service, not visibility evidence.
        elif rva == 0x4aa440:
            eye = uc.reg_read(UC_X86_REG_R8)
            require(eye in (0, 1), "Unexpected native submission eye")
            write(state + 4 + eye * 4, "<I", 1)
            write(state + 0x1736c + eye * 0x2c0, "<I", 1)
            ret()  # Fixture one native type-1 job for each eligible view.
        elif rva == 0x4ab4e0:
            eye = uc.reg_read(UC_X86_REG_R9)
            actual_list = uc.reg_read(UC_X86_REG_R8)
            actual_phase = read(uc.reg_read(UC_X86_REG_RSP) + 0x38, "<B")[0]
            require(actual_list == destination and
                uc.reg_read(UC_X86_REG_RDX) == destination + 0x40 + eye * 0x3c8,
                "Native worker dispatch did not retain exact copied list/camera")
            require(actual_phase == phase, "Native phase byte was not forwarded")
            jobs.append(dict(eye=eye, phase=actual_phase, list=hex(actual_list)))
            ret()  # Observe asynchronous job dispatch; do not run its worker.
        else:
            require(0x45526c <= rva < 0x455289 or 0x455493 <= rva < 0x45550b or
                0x4aa740 <= rva < 0x4aaa28,
                f"Unmodeled native submission instruction {address:x}")

    uc.hook_add(UC_HOOK_CODE, instruction)
    snapshot = bytes((index * 37 + 19) & 0xff for index in range(0xbde8))
    uc.mem_write(source, snapshot)
    write(source + 8, "<I", 2)
    write(source + 0x10, "<I", 0x10b)
    write(source + 0x3d8, "<I", 0x20b | (4 if reduce_count else 0))
    write(source + 0xbde4, "<I", 1 if reduce_count else 2)
    write(BASE + 0x1bea9e0, "<Q", destination - 0xb0)
    write(BASE + 0x2e3bdd8, "<Q", engine)
    write(BASE + 0x2e3d2b8, "<Q", state)
    write(engine + 0x198, "<II", 17, 23)
    uc.reg_write(UC_X86_REG_RSP, STACK + 0x1f008)
    if copied:
        uc.reg_write(UC_X86_REG_R14, source - 0x70)
        uc.emu_start(BASE + 0x45526c, STOP, count=10000)
        require(uc.reg_read(UC_X86_REG_RIP) == BASE + 0x455289,
            "Native copy callsite did not reach its continuation")
    else:
        uc.mem_write(destination, bytes(uc.mem_read(source, 0xbde8)))
    require(uc.mem_read(source, 0xbde8) == uc.mem_read(destination, 0xbde8),
        "Native complete-list copy changed an opaque tail byte")
    source_before = bytes(uc.mem_read(source, 0xbde8))
    if copied:
        require(phase == 1, "Copied native caller always submits phase one")
        uc.reg_write(UC_X86_REG_RSI, 0)
        uc.emu_start(BASE + 0x455493, STOP, count=10000)
        require(uc.reg_read(UC_X86_REG_RIP) == BASE + 0x45550b,
            "Native copied-list submit did not return to contracted callsite")
        expected_jobs = 1 if reduce_count else 2
    else:
        write(uc.reg_read(UC_X86_REG_RSP), "<Q", STOP)
        uc.reg_write(UC_X86_REG_RCX, 0xffffffffffffffff)
        uc.reg_write(UC_X86_REG_RDX, destination)
        uc.reg_write(UC_X86_REG_R8, 0xfeed0000 | phase)
        uc.emu_start(BASE + 0x4aa740, STOP, count=10000)
        require(uc.reg_read(UC_X86_REG_RIP) == STOP, "Native submit did not return")
        expected_jobs = 1 if reduce_count else 2
    require(len(jobs) == expected_jobs, "Unexpected native visibility job count")
    require(bytes(uc.mem_read(source, 0xbde8)) == source_before,
        "Native submission modified private source list")
    require(read(state + 0x18b00, "<II") == (17, 23),
        "Native submit did not use its global engine context")
    return dict(copied=copied, auxiliary_reduction=reduce_count, phase=phase,
        instructions=instructions, modeled_copies=copies, observed_worker_jobs=jobs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", type=Path, default=ROOT / "out/deps/re-tools/inputs/halo1.dll")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    raw = args.image.read_bytes()
    require(hashlib.sha256(raw).hexdigest().upper() == SHA, "Pinned image mismatch")
    manifest = json.loads((ROOT / "docs/HALOCE-EVIDENCE-MANIFEST.json").read_text(encoding="utf-8"))
    contracts = json.loads((ROOT / "docs/HALOCE-FIRST-PERSON-VISIBILITY-CONTRACTS.json").read_text(encoding="utf-8"))["entries"]
    bindings = verify(args.image, manifest["retail"], contracts)
    image = pefile.PE(data=raw, fast_load=True).get_memory_mapped_image()
    native = NativeVisibility(image)
    cases = []
    for player in (0, 1):
        for stereo in (0, 1):
            for active in (False, True):
                flags = native.produce(player, stereo, active)
                expected_exclusion = 0x180 if not active else 0 if stereo else 0x100 if player == 0 else 0x80
                require(flags & 0x180 == expected_exclusion, "Native producer exclusion mismatch")
                for included in (0, 2, 4, 6):
                    stock = native.consume((0, 1), included)
                    private = native.consume((0, 0), included)
                    expected_stock = sum(1 << eye for eye in (0, 1)
                        if included & (2 << eye) and not flags & (0x80 << eye))
                    expected_private = 3 if included & 2 and not flags & 0x80 else 0
                    require(stock == expected_stock and private == expected_private,
                        "Native consumer source-player mapping mismatch")
                    require(native.read(native.model + 8, "<I")[0] == flags,
                        "Private consumer input changed native model visibility flags")
                    cases.append(dict(source_player=player, native_stereo=stereo, active=active,
                        inclusion=included, exclusion=flags & 0x180, native_eyes=stock,
                        private_source_zero_eyes=private))
    submissions = [run_native_submission(image, copied, reduced, phase)
        for copied in (False, True) for reduced in (False, True)
        for phase in ((1,) if copied else (0, 1))]
    report = dict(status="PASS_NATIVE_FP_VISIBILITY_PRODUCER_AND_CONSUMER", image_sha256=SHA,
        cases=len(cases), instructions=native.instructions, modeled_palette_bridge_calls=native.bridge_calls,
        unique_binding_contracts=len(bindings["checks"]), binding_checks=bindings["checks"],
        submission_cases=len(submissions), submission_instructions=sum(case["instructions"] for case in submissions),
        submissions=submissions, records=cases,
        fixture_services=["model/palette lookup around actual source-player exclusion branch",
            "memcpy implementation after native complete-list argument setup", "submit memset and security-cookie services",
            "per-view job population and asynchronous worker dispatch endpoint"],
        limit="Native visibility predicate/copy/submission proven within named fixture services; runtime hook ownership, asynchronous worker execution and headset output require separate checks.")
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({key: value for key, value in report.items()
        if key not in ("records", "submissions", "binding_checks")}, indent=2))


if __name__ == "__main__":
    main()
