"""Inspect retained CE common constant buffers in an official RenderDoc export.

The supplied eye origins come from the preserved run log. Matching initial
bytes do not establish when, or whether, the previous frame drew a buffer.
"""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
import math
from pathlib import Path
import struct
import xml.etree.ElementTree as ET
import zipfile


def named(element, name):
    return next((child for child in element if child.get("name") == name), None)


def group_values(records, key):
    groups = Counter(tuple(record[key]) for record in records)
    return [{"value": value, "buffers": count} for value, count in groups.most_common()]


def inspect(xml_path, output, origins):
    if len(origins) != 2 or any(len(origin) != 3 or not all(math.isfinite(v) for v in origin)
                                for origin in origins):
        raise ValueError("Two finite three-component eye origins are required")
    chunks = ET.parse(xml_path).findall("./chunks/chunk")
    records = []
    with zipfile.ZipFile(xml_path.with_suffix("")) as payloads:
        for chunk in chunks:
            if chunk.get("name") != "ID3D11Device::CreateBuffer":
                continue
            desc, data = named(chunk, "pDesc"), named(chunk, "InitialData")
            if data is None or data.tag == "null":
                continue
            if int(named(desc, "BindFlags").text) != 4:
                continue
            # Actual CE common CB0 instances in these frames are 1440 bytes.
            # Other shader stages/material buffers use different layouts.
            if int(named(desc, "ByteWidth").text) != 1440:
                continue
            raw = payloads.read(f"{int(data.text):06d}")
            if len(raw) != 1440 or int(data.get("byteLength")) != 1440:
                raise ValueError("Common constant-buffer payload length is inconsistent")
            origin = struct.unpack_from("<4f", raw, 0x240)
            eye = next((eye for eye, expected in enumerate(origins)
                        if origin[3] == 1 and all(abs(a-b) < 0.000001
                                                for a, b in zip(origin[:3], expected))), None)
            if eye is None:
                continue
            record = {"eye_origin_match": eye, "resource": int(named(chunk, "pBuffer").text),
                      "creation_chunk": int(chunk.get("chunkIndex")), "payload": int(data.text),
                      "sha256": hashlib.sha256(raw).hexdigest(),
                      "origin240": origin, "origin320": struct.unpack_from("<4f", raw, 0x320),
                      "world_matrix270": struct.unpack_from("<16f", raw, 0x270),
                      "world_matrix270_hex": raw[0x270:0x2b0].hex(),
                      "fp_matrix2b0": struct.unpack_from("<16f", raw, 0x2b0),
                      "model380_first48": struct.unpack_from("<12f", raw, 0x380)}
            if not all(math.isfinite(value) for key in ("origin240", "origin320", "world_matrix270",
                                                        "fp_matrix2b0", "model380_first48")
                       for value in record[key]):
                raise ValueError(f"Non-finite common constants in resource {record['resource']}")
            records.append(record)
    by_eye = [[record for record in records if record["eye_origin_match"] == eye] for eye in range(2)]
    if not all(by_eye):
        raise ValueError("Both specified origins must be present")
    groups = [{"eye_origin_match": eye, "buffers": len(items),
               **{key: group_values(items, key) for key in
                  ("origin240", "origin320", "world_matrix270", "fp_matrix2b0", "model380_first48")}}
              for eye, items in enumerate(by_eye)]
    matrices = [group["world_matrix270"][0]["value"] for group in groups]
    # Float equality deliberately treats +0 and -0 as equal. A claim about
    # original GPU bytes additionally requires one common encoding across
    # every instance of each numerically dominant matrix.
    matrix_bytes = [{record["world_matrix270_hex"] for record in items
                     if tuple(record["world_matrix270"]) == tuple(matrices[eye])}
                    for eye, items in enumerate(by_eye)]
    eye_origins = [group["origin240"][0]["value"][:3] for group in groups]
    separation = math.dist(*eye_origins)
    midpoint = [(a+b)*0.5 for a, b in zip(*eye_origins)]
    # The captured shader uses row dot products after subtracting the origin.
    # Use the common world matrix, not model data or the FP selector matrix.
    forward = matrices[0][12:15]
    right = matrices[0][0:3]
    up = matrices[0][4:7]
    right_length, up_length = math.hypot(*right), math.hypot(*up)
    if right_length == 0 or up_length == 0:
        raise ValueError("Dominant world matrix has a degenerate projection basis")
    right = [x / right_length for x in right]
    up = [x / up_length for x in up]
    clips = []
    for distance in (0.25, 1.0, 10.0, 100.0, 1000.0):
        for horizontal, vertical in ((0, 0), (-0.5, 0.5), (0.5, -0.5)):
            point = [midpoint[axis] + distance * (forward[axis] +
                     horizontal*right[axis] + vertical*up[axis]) for axis in range(3)]
            eye_clips = []
            for origin, matrix in zip(eye_origins, matrices):
                relative = [point[axis]-origin[axis] for axis in range(3)] + [1]
                clip = [sum(matrix[row*4+column]*relative[column] for column in range(4))
                        for row in range(4)]
                if clip[3] <= 0 or not all(math.isfinite(value) for value in clip):
                    raise ValueError("Sample world point has nonpositive or non-finite clip W")
                eye_clips.append({"clip": clip, "ndc": [value/clip[3] for value in clip[:3]]})
            clips.append({"distance": distance, "world_point": point, "eyes": eye_clips,
                          "ndc_delta": [eye_clips[1]["ndc"][axis]-eye_clips[0]["ndc"][axis]
                                        for axis in range(3)]})
    report = {"scope": "Retained initial bytes; origin matching is not a prior-frame draw association",
              "xml": str(xml_path.resolve()), "buffers": records, "groups": groups,
              "matching_buffer_count": len(records), "eye_origin_separation": separation,
              "dominant_world_matrices_identical": matrices[0] == matrices[1],
              "dominant_world_matrices_bit_identical": len(matrix_bytes[0]) == 1 and matrix_bytes[0] == matrix_bytes[1],
              "dominant_world_matrix_byte_variants": [len(values) for values in matrix_bytes],
              "all_matched_origins240_equal320": all(record["origin240"] == record["origin320"] for record in records),
              "same_world_point_clip_samples": clips}
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({key: value for key, value in report.items()
                      if key not in ("buffers", "groups", "same_world_point_clip_samples")}, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("xml", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--left-origin", type=float, nargs=3, required=True)
    parser.add_argument("--right-origin", type=float, nargs=3, required=True)
    args = parser.parse_args()
    inspect(args.xml, args.output, [args.left_origin, args.right_origin])
