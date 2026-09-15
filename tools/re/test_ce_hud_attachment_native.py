"""Connect native CE depth allocation, frame selection and late HUD binding.

Uses the complete native allocator fixture, then executes the actual frame's
parent-depth assignment and split-surface selector. Never opens a game process.
"""
import argparse
import hashlib
import json
from pathlib import Path

from test_ce_resolution_native import NativeAllocation, ROOT, SHA, BASE, HEAP, STOP, require, pefile
from unicorn.x86_const import UC_X86_REG_RDX, UC_X86_REG_RBX, UC_X86_REG_RBP


class HudAttachment(NativeAllocation):
    phase = False

    def instruction(self, machine, address, size, extra):
        if not self.phase:
            return super().instruction(machine, address, size, extra)
        self.instructions += 1
        if address == BASE + 0x20b510:
            role = machine.reg_read(UC_X86_REG_RDX)
            require(role == 0x08800001, "Frame selected a different depth role")
            entry = next(e for e in self.entries if e["usage"] == role)
            self.ret(self.read(self.pool + 0x10 + entry["index"] * 0x38, "Q")[0])
        elif address == STOP:
            machine.emu_stop()
        elif not (BASE + 0x455a8c <= address < BASE + 0x455aaf or
                  BASE + 0x45e3f0 <= address < BASE + 0x45e406 or
                  BASE + 0xad5f0 <= address < BASE + 0xad650):
            raise RuntimeError(f"Unexpected attachment instruction {address-BASE:#x}")

    def verify_attachment(self):
        allocation = self.verify()
        self.phase = True
        self.uc.reg_write(UC_X86_REG_RBP, HEAP + 0xf1000)
        self.uc.emu_start(BASE + 0x455a8c, BASE + 0x455aaf, count=1000)
        root = self.read(self.backend + 0x318, "Q")[0]
        depth_entry = next(e for e in self.entries if e["usage"] == 0x08800001)
        require(root == self.read(self.pool + 0x10 + depth_entry["index"] * 0x38, "Q")[0],
                "Native frame did not publish its selected depth root")
        packed_entry = next(e for e in self.entries if e["usage"] == 0x04000001)
        packed = self.read(self.pool + 0x10 + packed_entry["index"] * 0x38, "Q")[0]
        packed_size = self.read(packed + 0x10, "2h")
        selector = HEAP + 0xf2000
        self.write(BASE + 0x1bea6b8, "Q", selector)
        selected_sizes = []
        for eye in range(2):
            self.write(selector + 0x1d0, "I", 0x8000 | eye << 16)
            selected = self.call(0xad5f0, root)
            require(selected == self.read(root + 0xa8 + eye * 8, "Q")[0],
                    "Native surface selector did not select its depth child")
            size = self.read(selected + 0x10, "2h")
            require(packed_size[0] == size[0] and packed_size[1] == 2 * size[1],
                    "Expected packed/color versus selected-depth height mismatch absent")
            selected_sizes.append(size)
        # The second output clears the split bit BEFORE the late callback.
        # Its parent depth therefore selects the full root, not an eye child.
        self.uc.reg_write(UC_X86_REG_RBX, 1)
        self.uc.emu_start(BASE + 0x45e3f0, BASE + 0x45e406, count=1000)
        late_selected = self.call(0xad5f0, root)
        require(late_selected == root, "Late HUD did not return to the unsplit depth root")
        late_depth_size = self.read(late_selected + 0x10, "2h")
        late_mismatch = late_depth_size != packed_size
        require(late_mismatch == self.full,
                "Late native root/packed mismatch must distinguish the full-resolution candidate")
        return dict(full_resolution=self.full, depth_role="0x08800001",
                    packed_role="0x04000001", native_root_size=self.read(root+0x10,"2h"),
                    selected_depth_sizes=selected_sizes, packed_size=packed_size,
                    late_depth_size=late_depth_size, late_split_flag_cleared=True,
                    allocation_instructions=allocation["instructions"], instructions=self.instructions,
                    frame_assignment="455A93 -> 20B510 -> 455AA8 parent+318",
                    selector="AD5F0", late_selector_clear="45E3F0..45E406",
                    late_attachment_mismatch=late_mismatch)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", type=Path, default=ROOT/"out/deps/re-tools/inputs/halo1.dll")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    raw = args.image.read_bytes()
    require(hashlib.sha256(raw).hexdigest().upper() == SHA, "Pinned image SHA-256 mismatch")
    pe = pefile.PE(data=raw)
    cases = [HudAttachment(pe, w, h, full).verify_attachment()
             for w, h in ((32, 16), (2912, 2100)) for full in (False, True)]
    report = dict(status="PASS_NATIVE_HUD_ATTACHMENT_MISMATCH", image_sha256=SHA, cases=cases,
                  modeled_dependencies=["Existing native-allocation fixture dependencies",
                      "20B510 role lookup returns the exact initialized native pool entry"],
                  limit="Native allocation, frame parent assignment and surface selection execute; "
                      "D3D rejection and corrected pixels are independently exercised by the production WARP fixture. "
                      "No headset acceptance.")
    if args.output:
        args.output.write_text(json.dumps(report, indent=2)+"\n", encoding="utf-8")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
