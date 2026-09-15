"""Read CE Anniversary .sdc cache streams and disassemble their DXBC payloads.

Reads only the supplied cache; all extracted data stays under the output folder.
This is offline evidence inspection, never a game patch or a shader replacement.
Uses the Windows SDK D3DDisassemble API from the system D3D compiler.
"""

import argparse
import ctypes
from ctypes import wintypes
import hashlib
import json
import os
from pathlib import Path
import struct
import zlib


def disassembler():
    library = ctypes.WinDLL(str(Path(os.environ["SystemRoot"])/"System32/d3dcompiler_47.dll"))
    function = library.D3DDisassemble
    function.argtypes = (ctypes.c_void_p, ctypes.c_size_t, wintypes.UINT,
                         ctypes.c_char_p, ctypes.POINTER(ctypes.c_void_p))
    function.restype = ctypes.c_long

    def disassemble(data):
        source, blob = ctypes.create_string_buffer(data), ctypes.c_void_p()
        status = function(source, len(data), 0, None, ctypes.byref(blob))
        if status < 0 or not blob.value:
            raise ValueError(f"D3DDisassemble failed: {status & 0xFFFFFFFF:#x}")
        methods = ctypes.cast(blob, ctypes.POINTER(ctypes.POINTER(ctypes.c_void_p))).contents
        try:
            pointer = ctypes.WINFUNCTYPE(ctypes.c_void_p, ctypes.c_void_p)(methods[3])(blob)
            size = ctypes.WINFUNCTYPE(ctypes.c_size_t, ctypes.c_void_p)(methods[4])(blob)
            return ctypes.string_at(pointer, size).rstrip(b"\0").decode("utf-8")
        finally:
            ctypes.WINFUNCTYPE(wintypes.ULONG, ctypes.c_void_p)(methods[2])(blob)
    return disassemble


def inspect(source, output):
    data = source.read_bytes()
    if len(data) < 16:
        raise ValueError("Truncated cache header")
    count, keys, compressed_size = struct.unpack_from("<3I", data)
    if not 0 < count <= 10000 or not 0 < compressed_size < len(data)-12:
        raise ValueError("Invalid cache stream count/size")
    encoded = data[len(data)-compressed_size:]
    output.mkdir(parents=True, exist_ok=True)
    disassemble = disassembler()
    streams, shaders = [], []
    for stream in range(count):
        decoder = zlib.decompressobj()
        decoded = decoder.decompress(encoded)
        if not decoder.eof or decoder.unconsumed_tail:
            raise ValueError(f"Incomplete zlib stream {stream}")
        consumed = len(encoded)-len(decoder.unused_data)
        encoded = decoder.unused_data
        streams.append({"index": stream, "compressed_size": consumed, "size": len(decoded)})
        cursor, index = 0, 0
        while (cursor := decoded.find(b"DXBC", cursor)) != -1:
            if cursor+32 > len(decoded):
                raise ValueError("Truncated DXBC header")
            size, chunks = struct.unpack_from("<2I", decoded, cursor+24)
            if size < 32+4*chunks or cursor+size > len(decoded) or not 0 < chunks <= 64:
                raise ValueError("Invalid DXBC size/chunk count")
            blob = decoded[cursor:cursor+size]
            for chunk in range(chunks):
                offset = struct.unpack_from("<I", blob, 32+chunk*4)[0]
                if offset+8 > len(blob) or offset+8+struct.unpack_from("<I", blob, offset+4)[0] > len(blob):
                    raise ValueError("Invalid DXBC chunk bounds")
            name = f"{stream:04d}-{index:03d}"
            text = disassemble(blob)
            (output/(name+".asm")).write_text(text, encoding="utf-8")
            (output/(name+".dxbc")).write_bytes(blob)
            shaders.append({"name": name, "stream_offset": cursor, "size": size,
                            "sha256": hashlib.sha256(blob).hexdigest()})
            cursor += size
            index += 1
    if encoded:
        raise ValueError("Bytes remain after the declared zlib stream count")
    result = {"source": str(source.resolve()), "sha256": hashlib.sha256(data).hexdigest(),
              "size": len(data), "header_keys": keys, "streams": streams, "shaders": shaders,
              "scope": "Cache DXBC contents only; shader draw selection and GPU execution are not established"}
    (output/"manifest.json").write_text(json.dumps(result, indent=2)+"\n", encoding="utf-8")
    return {"source": source.name, "streams": len(streams), "shaders": len(shaders), "output": str(output)}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    arguments = parser.parse_args()
    print(json.dumps(inspect(arguments.source, arguments.output), indent=2))
