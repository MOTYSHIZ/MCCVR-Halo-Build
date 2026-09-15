"""Build isolated RenderDoc 1.46 replay/fixture tools into ignored out/.

Downloads only official v1.46 public headers. Does not launch either helper,
attach to any process, register capture hooks, or edit RenderDoc preferences.
Requires the exact existing portable DLL, CMake, and VS2022 C++ Build Tools.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import urllib.request

ROOT = Path(__file__).resolve().parents[3]
DLL_SHA256 = "809DA38E3867D9FD09CC5C30DD5310500DEE75E999166D7A14AD1EF6E9ECA65D"
BASE = "https://raw.githubusercontent.com/baldurk/renderdoc/v1.46/renderdoc/api/replay/"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--renderdoc-dir", type=Path, default=ROOT / "out/deps/renderdoc/1.46/RenderDoc_1.46_64")
    parser.add_argument("--work-dir", type=Path, default=ROOT / "out/ce-renderdoc-offline")
    args = parser.parse_args()
    binary_dir = args.renderdoc_dir.resolve()
    work = args.work_dir.resolve()
    if not work.is_relative_to((ROOT / "out").resolve()):
        parser.error("--work-dir must be inside this repository's ignored out directory")
    dll = binary_dir / "renderdoc.dll"
    if hashlib.sha256(dll.read_bytes()).hexdigest().upper() != DLL_SHA256:
        parser.error("renderdoc.dll is not the pinned official portable 1.46 DLL")
    headers = work / "api"
    headers.mkdir(parents=True, exist_ok=True)
    pending = ["renderdoc_replay.h"]
    hashes = {}
    while pending:
        name = pending.pop()
        if name in hashes:
            continue
        if not re.fullmatch(r"[A-Za-z0-9_]+\.h", name):
            raise RuntimeError(f"Unexpected header include: {name}")
        data = urllib.request.urlopen(BASE + name, timeout=30).read()
        (headers / name).write_bytes(data)
        hashes[name] = hashlib.sha256(data).hexdigest()
        pending.extend(re.findall(r'^#include "([^"]+)"', data.decode(), re.M))

    vswhere = Path(os.environ["ProgramFiles(x86)"]) / "Microsoft Visual Studio/Installer/vswhere.exe"
    install = subprocess.check_output([
        str(vswhere), "-latest", "-version", "[17.0,18.0)", "-products", "*", "-requires",
        "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath",
    ], text=True).strip()
    if not install:
        raise RuntimeError("VS C++ tools not found")
    versions = list((Path(install) / "VC/Tools/MSVC").glob("*"))
    compiler = max(versions, key=lambda p: tuple(int(x) for x in p.name.split("."))) / "bin/Hostx64/x64"
    exports = subprocess.check_output([str(compiler / "dumpbin.exe"), "/exports", str(dll)], text=True)
    names = re.findall(r"^\s+\d+\s+[0-9A-F]+\s+[0-9A-F]+\s+(\S+)", exports, re.M)
    if "RENDERDOC_OpenCaptureFile" not in names:
        raise RuntimeError("Expected replay export is missing")
    definition = work / "renderdoc.def"
    definition.write_text("LIBRARY renderdoc\nEXPORTS\n" + "\n".join(names) + "\n")
    library = work / "renderdoc.lib"
    subprocess.run([str(compiler / "lib.exe"), f"/def:{definition}", f"/out:{library}", "/machine:x64"], check=True)
    build = work / "build"
    subprocess.run([
        "cmake", "-S", str(Path(__file__).parent), "-B", str(build),
        "-G", "Visual Studio 17 2022", "-A", "x64",
        f"-DRENDERDOC_API_DIR={headers}", f"-DRENDERDOC_IMPORT_LIBRARY={library}",
        f"-DRENDERDOC_BIN_DIR={binary_dir}",
    ], check=True)
    subprocess.run(["cmake", "--build", str(build), "--config", "Release"], check=True)
    manifest = {"version": "1.46", "dll_sha256": DLL_SHA256, "headers": hashes,
                "executables": {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                                for p in (build / "Release").glob("ce_renderdoc_*.exe")}}
    (work / "build-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"Built isolated tools in {build / 'Release'}; no helper was launched.")


if __name__ == "__main__":
    main()
