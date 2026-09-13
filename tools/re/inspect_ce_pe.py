"""Offline CE evidence inspection. Never opens a process or writes a game file.

Requires pefile and capstone (the existing ignored out/pydeps environment).
Disassembly and string address operands are leads, not verified hook bindings.
"""
import argparse
import hashlib
import json
import re
import struct
from pathlib import Path

import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_32, CS_MODE_64


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("identity")
    strings = commands.add_parser("strings")
    strings.add_argument("needles", nargs="+")
    disasm = commands.add_parser("disasm")
    disasm.add_argument("rva", type=lambda s: int(s, 16))
    disasm.add_argument("length", type=lambda s: int(s, 0))
    function = commands.add_parser("function")
    function.add_argument("rva", type=lambda s: int(s, 16))
    pointers = commands.add_parser("pointers")
    pointers.add_argument("rva", type=lambda s: int(s, 16))
    pointers.add_argument("count", type=int)
    args = parser.parse_args()
    raw = args.image.read_bytes()
    pe = pefile.PE(data=raw, fast_load=True)
    machine = pe.FILE_HEADER.Machine
    if machine not in (0x14C, 0x8664):
        raise SystemExit("Only x86/x64 PE inputs are supported")
    if args.command == "identity":
        print(json.dumps({
            "file": args.image.name, "bytes": len(raw),
            "sha256": hashlib.sha256(raw).hexdigest().upper(),
            "machine": hex(machine), "timestamp": hex(pe.FILE_HEADER.TimeDateStamp),
            "image_base": hex(pe.OPTIONAL_HEADER.ImageBase),
            "image_size": hex(pe.OPTIONAL_HEADER.SizeOfImage),
            "sections": [{"name": s.Name.rstrip(b"\0").decode(),
                          "rva": hex(s.VirtualAddress), "size": hex(s.Misc_VirtualSize),
                          "executable": bool(s.Characteristics & 0x20000000)}
                         for s in pe.sections],
        }, indent=2))
    elif args.command == "strings":
        needles = [s.lower() for s in args.needles]
        for match in re.finditer(rb"[ -~]{8,}", raw):
            value = match.group().decode("ascii")
            if not any(s in value.lower() for s in needles):
                continue
            # A printable byte before a C source path can be unrelated data.
            path_offset = value.lower().find("c:\\mcc\\")
            start = match.start() + max(0, path_offset)
            rva = pe.get_rva_from_offset(start)
            print(f"STRING {rva:#x}: {raw[start:match.end()].decode('ascii')}")
            if machine == 0x14C:
                address = struct.pack("<I", pe.OPTIONAL_HEADER.ImageBase + rva)
                for section in pe.sections:
                    if not section.Characteristics & 0x20000000:
                        continue
                    data = section.get_data()
                    offset = 0
                    while (offset := data.find(address, offset)) >= 0:
                        print(f"  UNVERIFIED_ADDRESS_OPERAND {section.VirtualAddress + offset:#x}")
                        offset += 1
    elif args.command == "pointers":
        if not 0 < args.count <= 256:
            raise SystemExit("Pointer count must be 1..256")
        width = 4 if machine == 0x14C else 8
        for index in range(args.count):
            rva = args.rva + width * index
            value = int.from_bytes(pe.get_data(rva, width), 'little')
            target = value - pe.OPTIONAL_HEADER.ImageBase
            print(f'{rva:#x}: {value:#x} image_rva={target:#x}')
    else:
        if args.command == "function":
            if machine != 0x8664:
                raise SystemExit("Function ranges require x64 unwind metadata")
            pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_EXCEPTION']])
            entry = next((e.struct for e in pe.DIRECTORY_ENTRY_EXCEPTION
                          if e.struct.BeginAddress <= args.rva < e.struct.EndAddress), None)
            if entry is None:
                raise SystemExit("No unwind range contains the RVA")
            args.rva = entry.BeginAddress
            args.length = entry.EndAddress - entry.BeginAddress
            print(f"UNWIND_RANGE {args.rva:#x}..{entry.EndAddress:#x}")
        if not 0 < args.length <= 0x10000:
            raise SystemExit("Disassembly length must be 1..65536 bytes")
        section = pe.get_section_by_rva(args.rva)
        if section is None or not section.Characteristics & 0x20000000:
            raise SystemExit("Requested RVA is not in an executable PE section")
        offset = args.rva - section.VirtualAddress
        if offset + args.length > section.SizeOfRawData:
            raise SystemExit("Requested range exceeds file-backed executable data")
        decoder = Cs(CS_ARCH_X86, CS_MODE_32 if machine == 0x14C else CS_MODE_64)
        for instruction in decoder.disasm(pe.get_data(args.rva, args.length), args.rva):
            print(f"{instruction.address:08X}  {instruction.bytes.hex(' '):32} "
                  f"{instruction.mnemonic} {instruction.op_str}")


if __name__ == "__main__":
    main()
