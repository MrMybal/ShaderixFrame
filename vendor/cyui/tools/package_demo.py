"""Rebuild and package the standalone Windows x64 gallery with its notices."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import zipfile


ROOT = Path(__file__).resolve().parents[1]
THEMES = ("mid-grey", "light-grey", "amber-night", "violet", "navy")
NOTICES = (
    "LICENSE",
    "THIRD_PARTY_NOTICES.md",
    "third_party/imgui/LICENSE.txt",
    "third_party/texteditor/LICENSE",
    "third_party/lexilla/License.txt",
    "third_party/scintilla/License.txt",
    "third_party/SciTE-Properties-License.txt",
)


def git(*args: str) -> str:
    return subprocess.check_output(
        ["git", "-c", f"safe.directory={ROOT.as_posix()}", *args],
        cwd=ROOT, text=True, encoding="utf-8",
    ).strip()


def check_executable(path: Path) -> None:
    raw = path.read_bytes()
    if raw[:2] != b"MZ":
        raise RuntimeError("The demo is not a Windows executable")
    offset = struct.unpack_from("<I", raw, 0x3C)[0]
    if raw[offset:offset + 4] != b"PE\0\0" or struct.unpack_from("<H", raw, offset + 4)[0] != 0x8664:
        raise RuntimeError("The demo must be a Windows x64 executable")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "build/releases")
    args = parser.parse_args()
    if os.name != "nt":
        parser.error("The standalone demo currently requires Windows and MSVC x64")

    version = re.search(r'#define CYUI_VERSION "([0-9.]+)"',
                        (ROOT / "include/cyui/CyUI.h").read_text(encoding="utf-8"))
    if version is None:
        raise RuntimeError("Cannot read CYUI_VERSION")
    version = version.group(1)
    revision = git("rev-parse", "HEAD")
    dirty = bool(git("status", "--porcelain"))
    # Recompile every time; never package an unrelated existing executable.
    subprocess.run([os.environ.get("COMSPEC", "cmd.exe"), "/d", "/c", "build_demo.bat"],
                   cwd=ROOT, check=True)
    executable = ROOT / "build/cyui_demo.exe"
    check_executable(executable)

    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    name = f"CyUI-Demo-{version}-windows-x64"
    archive = output / f"{name}.zip"
    contents = {"cyui_demo.exe": executable.read_bytes()}
    for notice in NOTICES:
        contents[notice] = (ROOT / notice).read_bytes()
    # Retain both complete stb license alternatives from the actual bundled headers.
    stb_notices = []
    for header in ("imstb_rectpack.h", "imstb_textedit.h", "imstb_truetype.h"):
        text = (ROOT / "third_party/imgui" / header).read_text(encoding="utf-8")
        marker = text.rfind("This software is available under 2 licenses")
        if marker < 0:
            raise RuntimeError(f"Cannot locate the stb license in {header}")
        end = text.index("*/", marker)
        stb_notices.append(f"{header}\n\n{text[marker:end].rstrip()}\n")
    contents["third_party/imgui/STB-LICENSE.txt"] = "\n".join(stb_notices).encode("utf-8")
    contents["Launch-French.cmd"] = (
        '@echo off\r\ncd /d "%~dp0"\r\nstart "" "%~dp0cyui_demo.exe" --locale fr %*\r\n'
    ).encode("ascii")
    contents["README.txt"] = f"""CyUI {version} - portable Windows x64 demo

Extract the entire ZIP, then double-click cyui_demo.exe.
Use Launch-French.cmd for French; the default language is English.
Themes and language can also be changed inside the application.

Requirements: Windows x64 with DirectX 11 support (hardware or Windows WARP).
Visual Studio, Python and a separate Visual C++ runtime installation are not needed.
The application uses system fonts. Its settings are saved in imgui.ini in the
working directory; extract to a writable folder to retain your layout.

Examples (PowerShell):
  .\\cyui_demo.exe --locale fr
  .\\cyui_demo.exe --theme violet --comfortable
  .\\cyui_demo.exe --gallery-only --tab 21
  .\\cyui_demo.exe --gallery-only --tab 7 --image-graph

Explore widgets, audio displays, curves, node graphs, image previews, code/diff,
content browsers, property inspectors, timelines, commands and quick filters.
The gallery uses synthetic sample data, with host-generated preview images.
The Web page is a preview; interactive browsing needs a separate CEF-enabled
build and its runtime. Unreal Editor integration is available separately as
source. This package does not contain Linux or macOS demo binaries.

CyUI is MIT licensed; dependency notices are included under third_party/.
Source and integration guides: https://github.com/MrMybal/CyUI
Version: {version}
Source commit: {revision}
""".encode("utf-8")
    contents["build-info.json"] = (json.dumps({
        "version": version, "commit": revision, "working_tree_modified": dirty,
        "platform": "windows-x64", "backend": "Win32 / DirectX 11",
        "runtime": "MSVC static", "cef": False,
    }, indent=2) + "\n").encode("utf-8")
    contents["SHA256SUMS.txt"] = "".join(
        f"{hashlib.sha256(data).hexdigest()}  {path}\n" for path, data in sorted(contents.items())
    ).encode("utf-8")

    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as zipped:
        for path, data in sorted(contents.items()):
            zipped.writestr(path, data)

    # Exercise the actual extracted archive without picking up files from build/.
    with tempfile.TemporaryDirectory(prefix="demo-package-", dir=ROOT / "build") as scratch:
        scratch = Path(scratch).resolve()
        if not scratch.is_relative_to((ROOT / "build").resolve()):
            raise RuntimeError("Package checks must stay inside build/")
        extracted, checks = scratch / "extracted", scratch / "checks"
        checks.mkdir()
        with zipfile.ZipFile(archive) as zipped:
            if set(zipped.namelist()) != set(contents) or zipped.testzip() is not None:
                raise RuntimeError("The archive contents do not match the package manifest")
            zipped.extractall(extracted)
        for path, data in contents.items():
            if (extracted / path).read_bytes() != data:
                raise RuntimeError(f"Archive verification failed: {path}")
        for theme in THEMES:
            for language in ("en", "fr"):
                capture = checks / f"{theme}-{language}.bmp"
                subprocess.run([str(extracted / "cyui_demo.exe"), "--capture", str(capture),
                                "--theme", theme, "--locale", language, "--tab", "21",
                                "--gallery-only"], cwd=checks, check=True, timeout=60)
                raw = capture.read_bytes()
                if raw[:2] != b"BM" or len(raw) < 1024:
                    raise RuntimeError(f"The extracted demo did not render: {theme}, {language}")
                print(f"Rendered extracted demo: {theme}, {language}", flush=True)
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    archive.with_suffix(".zip.sha256").write_text(f"{digest}  {archive.name}\n", encoding="ascii")
    print(f"Packaged {archive.name} ({archive.stat().st_size:,} bytes)")
    if dirty:
        print("Development package: the source working tree contains changes.")


if __name__ == "__main__":
    main()
