"""Decode initial RGBA/BGRA textures from RenderDoc's official zip.xml export.

No GPU replay, game process, graphics API call, or third-party Python package.
These are capture-BEGIN contents, never the result of replaying the frame.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct
import xml.etree.ElementTree as ET
import zipfile
import zlib


def named(element, name):
    return next((child for child in element if child.get("name") == name), None)


def integer(element, name):
    child = named(element, name)
    if child is None or child.text is None:
        raise ValueError(f"Missing {name}")
    return int(child.text)


def png_chunk(kind, contents):
    return (struct.pack(">I", len(contents)) + kind + contents +
            struct.pack(">I", zlib.crc32(kind + contents)))


def encode_png(width, height, rgba):
    if width <= 0 or height <= 0 or len(rgba) != width * height * 4:
        raise ValueError("RGBA byte count does not match positive image dimensions")
    rows = b"".join(b"\0" + rgba[y * width * 4:(y + 1) * width * 4]
                    for y in range(height))
    return (b"\x89PNG\r\n\x1a\n" +
            png_chunk(b"IHDR", struct.pack(">2I5B", width, height, 8, 6, 0, 0, 0)) +
            png_chunk(b"IDAT", zlib.compress(rows)) + png_chunk(b"IEND", b""))


def extract(xml_path, output, selected):
    root = ET.parse(xml_path).getroot()
    textures, records = {}, []
    chunks = root.findall("./chunks/chunk")
    for chunk in chunks:
        if chunk.get("name") == "ID3D11Device::CreateTexture2D":
            resource = integer(chunk, "pTexture")
            desc = named(chunk, "Descriptor")
        elif chunk.get("name") == "IDXGISwapChain::GetBuffer":
            resource = integer(chunk, "SwapbufferID")
            desc = named(chunk, "BackbufferDescriptor")
        else:
            continue
        textures[resource] = {
            "width": integer(desc, "Width"), "height": integer(desc, "Height"),
            "format": named(desc, "Format").get("string"),
            "mips": integer(desc, "MipLevels"), "array_size": integer(desc, "ArraySize"),
            "samples": integer(named(desc, "SampleDesc"), "Count"),
        }
    output.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(xml_path.with_suffix("")) as payloads:
        for chunk in chunks:
            if chunk.get("name") != "Internal::Initial Contents":
                continue
            resource = integer(chunk, "id")
            if resource not in selected:
                continue
            desc = textures[resource]
            if not (desc["mips"] == desc["array_size"] == desc["samples"] == 1 and
                    integer(chunk, "NumSubresources") == 1):
                raise ValueError(f"Resource {resource} is not a single ordinary 2D subresource")
            omitted = named(chunk, "OmittedContents")
            if omitted is None or omitted.text != "false":
                raise ValueError(f"Resource {resource} has no complete initial contents")
            buffer = named(chunk, "SubresourceContents")
            contents = payloads.read(f"{int(buffer.text):06d}")
            if len(contents) != int(buffer.get("byteLength")):
                raise ValueError(f"Resource {resource} initial payload length is inconsistent")
            width, height, fmt = desc["width"], desc["height"], desc["format"]
            pitch = integer(chunk, "RowPitch")
            if width <= 0 or height <= 0 or pitch < width * 4 or len(contents) != pitch * height:
                raise ValueError(f"Resource {resource} has inconsistent dimensions or row pitch")
            if fmt not in ("DXGI_FORMAT_R8G8B8A8_UNORM", "DXGI_FORMAT_R8G8B8A8_UNORM_SRGB",
                           "DXGI_FORMAT_B8G8R8A8_UNORM", "DXGI_FORMAT_B8G8R8A8_UNORM_SRGB"):
                raise ValueError(f"Resource {resource} requires unsupported format interpretation: {fmt}")
            rgba = bytearray(b"".join(contents[y * pitch:y * pitch + width * 4]
                                    for y in range(height)))
            if fmt.startswith("DXGI_FORMAT_B8G8R8A8_"):
                rgba[0::4], rgba[2::4] = rgba[2::4], rgba[0::4]
            path = output / f"initial-resource-{resource}.png"
            path.write_bytes(encode_png(width, height, rgba))
            records.append({
                "resource": resource, "chunk": int(chunk.get("chunkIndex")),
                "payload": int(buffer.text), **desc, "row_pitch": pitch,
                "payload_sha256": hashlib.sha256(contents).hexdigest(),
                "png_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                "png": str(path.resolve()),
            })
    if set(selected) != {record["resource"] for record in records}:
        raise ValueError("Requested resources not all present in initial contents")
    manifest = {"scope": "Capture-begin texture contents; not replay output",
                "xml": str(xml_path.resolve()), "resources": records}
    (output / "initial-textures.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(json.dumps(manifest, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("xml", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--resources", type=int, nargs="+", required=True)
    arguments = parser.parse_args()
    extract(arguments.xml, arguments.output, arguments.resources)
