# Building ShaderixFrame

Use Windows 10/11 x64, Visual Studio 2022 C++ x64 tools, CMake, Ninja and Python 3.
Node.js 22+ generates the local CyDoc documentation. Reading the installed help does
not require Node, npm, a web server or Internet access. Generation uses the pinned
CyDoc archive and the locked packages in `.CyDocBuild/package-lock.json`:

```powershell
npm ci --prefix .CyDocBuild --omit=peer --ignore-scripts
python tools/build_cydoc.py --check --test
```

The first dependency restoration needs an npm cache or access to the package registry.
The CyDoc archive itself is bundled in `.CyDocBuild/vendor/cydoc/`; no public npm CyDoc package is
assumed. CMake generates and copies the complete `Documentation/` tree automatically.
See [documentation tooling](../.CyDocBuild/README.md) for translations, MCP refresh and updates.
The copied CyUI, Shaderixel, CyDAW v2, Dear ImGui and OpenFX sources are in `vendor/`.
Their original files, licences and SHA-256 fingerprints are retained. The upstream
projects are read only; video-specific code and English labels live in adapters.

## Complete alpha from corresponding source

Download `ShaderixFrame-0.15.0-alpha.2-source.zip` from the matching GitHub prerelease.
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
`Curves`, `Commands`, `CodeEditor`, `Images`, `Inspector`, `Timeline` and `DocumentViews` using
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

## CyMCPSDK assistant and command transports

`vendor/cymcp` is a selected, unmodified build-source snapshot of CyMCPSDK 0.7.1,
revision `34d4bcae126bc83d1a2925b5047dac36e3d09d92`, recorded in `cymcp.lock.json`.
It was imported read-only from the local source checkout. CyUI remains version
1.2.0 but is updated to `6ef01cfcdb197f3a89d64618ce5ef8818c704728` for ChatUI's
composer/transcript API. Upstream repositories are not changed.

Core, Platform, Http, MCP, AI and ChatUI compile from source. Local engine installation,
SDK examples/tests/benchmarks and its standalone GUI are disabled. Local model
services remain available through the HTTP providers without bundling engines or
weights. ChatUI reuses the existing ImGui target and CyUI DocumentViews.

Verify the pinned snapshots and build in a separate x64 VS developer terminal:

```powershell
python tools/sync_cymcp.py --check
python tools/sync_cyui.py --check
cmake -S . -B build/AssistantRelease -G Ninja -DCMAKE_BUILD_TYPE=Release -DSF_BUILD_VST3_WORKER=OFF -DSF_BUILD_AUDIO_QUALITY_TESTS=OFF
cmake --build build/AssistantRelease --parallel 4
ctest --test-dir build/AssistantRelease --output-on-failure
```

This does not run a GitHub workflow. `assistant_end_to_end` uses local SSE and MCP
fixtures, the real chat controller, the console executable and hidden real GUI
instances. It covers tool calls, cancellation, session restoration, permissions,
JSON/exit codes, same-instance edits, private chunked results and metadata-only
metrics. Fixtures do not establish live authentication/inference for every provider.
`mcp_access_profiles` starts real editor instances for all four profiles. It verifies
Free defaults and configuration migration, tokenless HTTP reads/edits, direct-address
CLI, schema rejection, shared stdio state, protected authentication and controlled
write/discard permissions. The chat fixture also edits through Free without a write
flag. The optional standalone audio quality target remains disabled.

To update a verified SDK snapshot, archive it and import an exact local revision:

```powershell
python tools/sync_cymcp.py --source PATH_TO_CYMCPSDK --revision FULL_COMMIT_SHA --update
```

The importer preserves the preceding snapshot under ignored `build/dependency-snapshots/`
and refuses replacement if the current vendored source has local edits.

`ui_automation` exercises the real hidden editor through the Protected and Free MCP profiles: modes,
docking, commands, file-path injection, dirty-project protection, nested drafts,
widget clicks/text, languages, shortcuts and a connected CLI. `native_ui_controls`
uses a hidden native fixture for scoped Windows accessibility discovery and actions.
ImGui is compiled consistently with its item-hook interface; `src/ui/UiItems.cpp`
implements those hooks locally. No third-party test-engine framework is bundled or
required. Native plugin accessibility uses Windows' system UIAutomation libraries.
The GUI registers its UI extensions into the shared automation registry; standalone
CLI processes retain the 163 headless business tools and address UI tools with `--connect`.

The result is `build/AssistantRelease/ShaderixFrame.exe` plus `ShaderixFrameCLI.exe`.
Use the app's bundled manual page `mcp` for launch and runtime configuration.
The packager includes the CLI, SDK fingerprint lock and dependency notices; it
accepts `--build-dir` to select a tested build and `--output-dir` to validate an
isolated package without replacing an existing portable installation or archive.
For example: `python tools/package.py --archive-only --build-dir build/AssistantRelease --output-dir build/qa/cymcp/package`.
It never includes assistant
connection descriptors, conversations, caches or performance logs.

After committing the reviewed release, generate its corresponding-source ZIP with
`python tools/package_source.py --juce-source path/to/locked-juce-checkout`. If that
checkout is unavailable, `--juce-archive path/to/prior-corresponding-source.zip`
reuses only the exact locked JUCE sources after verifying every dependency file
against the prior archive's source manifest. The application source always comes
from the current commit. Build the installer with `python tools/make_installer.py`.
