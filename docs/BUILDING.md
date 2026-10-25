# Building ShaderixFrame

Use Windows 10/11 x64, Visual Studio 2022 C++ x64 tools, CMake, Ninja and Python 3.
The copied Shaderixel, CyDAW v2, Dear ImGui and OpenFX sources are in `vendor/`.
Their original files, licences and SHA-256 fingerprints are retained. The upstream
projects are read only; video-specific code and English labels live in adapters.

## Complete alpha from corresponding source

Download `ShaderixFrame-0.13.0-alpha.1-source.zip` from the matching GitHub prerelease.
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
