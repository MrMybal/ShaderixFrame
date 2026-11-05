# Building ShaderixFrame

Use Windows 10/11 x64, Visual Studio 2022 C++ x64 tools, CMake, Ninja and Python 3.
Node.js 22+ generates the local CyDoc documentation. Reading the installed help does
not require Node, npm, a web server or Internet access. Generation uses the pinned
CyDoc archive and the locked packages in `tools/cydoc/package-lock.json`:

```powershell
npm ci --prefix tools/cydoc --omit=peer --ignore-scripts
python tools/build_cydoc.py --check --test
```

The first dependency restoration needs an npm cache or access to the package registry.
The CyDoc archive itself is bundled in `vendor/cydoc/`; no public npm CyDoc package is
assumed. CMake generates and copies the complete `build/help` tree automatically.
See [CyDoc maintenance](manual/README.md) for translations, MCP refresh and updates.
The copied CyUI, Shaderixel, CyDAW v2, Dear ImGui and OpenFX sources are in `vendor/`.
Their original files, licences and SHA-256 fingerprints are retained. The upstream
projects are read only; video-specific code and English labels live in adapters.

## Complete alpha from corresponding source

Download `ShaderixFrame-0.14.0-alpha.1-source.zip` from the matching GitHub prerelease.
It includes this repository and the exact JUCE 8.0.15 dependency. Extract it and
open an x64 Visual Studio developer terminal in `ShaderixFrame-source`:

```powershell
python tools/prepare_dependencies.py --audio-only
cmake -S . -B build/Release -G Ninja -DCMAKE_BUILD_TYPE=Release -DSF_BUILD_VST3_WORKER=ON -DSF_JUCE_SOURCE_DIR="$PWD/juce"
cmake --build build/Release --target ShaderixFrame ofx_gain
python tools/package.py --archive-only
python tools/make_installer.py
```

`SF_BUILD_VST3_WORKER=ON` rebuilds the locked CyDAW IPC-4 worker locally. It does not
use or modify a CyDAW checkout. The corresponding source and dependency notices
are described in [LICENSING.md](LICENSING.md).

## Repository-only checkout

The internal audio/DSP engine, native plugin host and OFX host build from repository
source. The optional JUCE worker is separate. Restore it from the matching Windows
release ZIP, or use the full source build above:

```powershell
python tools/prepare_dependencies.py --bundle path/to/ShaderixFrame-windows-x64.zip
python tools/prepare_dependencies.py --audio-only
build.bat Release
```

Restoration checks the upstream artifact fingerprint or the matching source-built
release worker recorded in `release-worker.lock.json`. Builds, binaries, generated
media, archives and caches stay outside Git and Git LFS.

`--audio-only` generates the missing example soundtrack without overwriting edited
projects. Without the worker, native effects work and VST3 loading reports that its
helper must be installed. Test suites report missing optional VST3 fixtures explicitly.
`--tests` can restore the development-only VST3 fixture from a configured CyDAW checkout.
It is never included in the Windows package.

Compile without running tests with `build.bat Release --safe-compile`. The optional
standalone audio quality target stays disabled. For software D3D11 testing:

```powershell
$env:SF_FORCE_WARP = '1'
ctest --test-dir build/Release --output-on-failure
```

## Upstream maintenance

Configure optional developer roots with `SF_SHADERIXEL_ROOT`, `SF_CYDAW_ROOT` and
`SF_JUCE_ROOT`. No workstation paths are stored in lock files. Without these variables,
the scripts look for sibling Shaderixel and CyDAW_Project directories.

```powershell
python tools/import_shaderixel.py --check
python tools/extract_composition.py --check
python tools/verify_snapshots.py
python tools/sync_upstream.py --check
```

Updates are explicit, preserve prior snapshots under ignored `vendor-history/`,
and reject unrelated local vendor edits. Rebuild and run compatibility checks after
regeneration. The source-built release worker is a separate, accepted artifact;
upstream fingerprints remain available for the original binary.

## Modular CyUI

The pinned source is included in `vendor/cyui`; no network download is required.
`cyui.lock.json` records the exact revision and SHA-256 of every vendored file.
The application supplies its existing Dear ImGui 1.93 docking target through
`CYUI_IMGUI_TARGET`. CyUI's bundled ImGui is deliberately omitted.

Production selects `Core`, `Audio`, `Canvas`, `Plots`, `AudioViews`, `Nodes`,
`Curves`, `Commands`, `CodeEditor`, `Images`, `Inspector` and `Timeline` using
explicit `CyUI::<Module>` targets. CodeEditor pulls `Code`, the pinned TextEditor,
Lexilla and Threads automatically and requires C++20, already used by the app.
Demo, Gallery, tests, CEF, Web, Unreal, ImGuizmo and Python UI adapters are disabled
or absent. Core/Canvas are also needed by the existing engine/presentation bridge;
the remaining module targets belong to the UI targets.

A separate clean configuration is useful when checking this migration:

```powershell
python tools/sync_cyui.py --check
python tools/generate_ui_translations.py --check
python tools/extract_composition.py --check
cmake -S . -B build/CyUIRelease -G Ninja -DCMAKE_BUILD_TYPE=Release -DSF_BUILD_VST3_WORKER=OFF
cmake --build build/CyUIRelease --parallel 4
ctest --test-dir build/CyUIRelease --output-on-failure
```

For an explicit dependency update, first read the new upstream `docs/AI_GUIDE.md`,
`docs/MODULES.md` and `docs/LOCALIZATION.md`, then archive a chosen full commit from
an unmodified local CyUI checkout:

```powershell
python tools/sync_cyui.py --source path/to/CyUI --revision FULL_40_CHARACTER_COMMIT_SHA
```

The tool reads a Git archive, rejects modified current snapshots, saves the old
snapshot under ignored `vendor-history/`, and refreshes the lock. It never writes
to the source checkout. Run the clean build and all compatibility tests after an
update. The native backend still supports Windows 10/11 only; this migration does
not add a Linux/macOS backend.
