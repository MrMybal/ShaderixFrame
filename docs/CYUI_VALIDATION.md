# CyUI migration validation

Validated on Windows x64 with Visual Studio 2022/MSVC, CMake/Ninja, C++20,
Direct3D 11 WARP and a fresh `build/CyUIRelease` Release configuration.
The last application binary is `build/CyUIRelease/ShaderixFrame.exe`.

## Build and automated checks

- Application, native/OFX workers, example plugin fixtures and all relevant test
  executables compiled successfully. The optional Debug/audio-quality target was
  not enabled or executed.
- Full CTest run: **12/12 passed**, zero failures, 68.17 seconds.
- After the final shared DSP layout/right-click grading/pan refinements, all five
  affected CyUI/editor/production suites were rerun: **5/5 passed**, 29.81 seconds.
- Editor suite: **106 interaction assertions**, including shared shader-node
  drag/one-step Undo/Escape, DSP stereo connections and node movement/Undo,
  source span trim, timeline zoom/navigator/filmstrips, layer graph zoom/context
  menu, clip workspaces, and HLSL keyboard isolation from sequence shortcuts.
- CyUI integration checks: stable IDs across live locale changes, host catalogue,
  density/DPI, two-context isolation, exact host shaped/smooth/hold interpolation,
  CRLF/no-final-newline code fidelity, operational HLSL syntax backend and
  node/curve cancellation when the host revision changes.
- Engine, GPU composition, transitions, mixer, background exports, native/OFX
  plugins, MCP stdio and detached workspace modes all passed. Production tests
  exercised the real CyDAW IPC-4 VST3 worker, opaque state, native editor and presets.
- Snapshot verification: 223 CyUI source files match the exact lock; 540 other
  upstream files and 25 OpenFX files pass integrity checks, with no missing external
  artifacts. Available *upstream updates* are reported separately and were not
  automatically imported.
- Component extraction and host translation generation match their generators.
  Python tooling compiles. Whitespace checks pass for the application migration;
  inherited snapshot byte formatting is preserved.
- The explicit CyUI update tool was exercised in an isolated QA fixture: a read-only
  Git archive reproduced every locked byte and saved the previous snapshot. The
  original CyUI checkout remains clean.

CTest suites: `cyui_integration`, `advanced_workflows`, `core_and_gpu`,
`editor_interactions`, `background_exports`, `transitions`, `audio_mixer`,
`production_features`, `cyui_catalogue`, `cyui_snapshot`, `mcp_stdio`,
`workspace_modes`.

## Visual checks

Hidden native application captures were inspected for all five built-in themes:
mid-grey, light-grey, amber-night, violet and navy. Compact and comfortable
settings, English/French, and a 150% DPI override were exercised. Source Images /
Timeline, shader code, graph cards/cables, thumbnails, grading/scopes, command
palette and the unchanged video arranger remain visible and functional. Initial
fit waits for a valid docked canvas size; graph text follows zoom.

Captures and command/test logs are retained under `build/qa/cyui-migration/`,
including `final-shader.png`, `final-layers.png`, `hlsl-light.png`, `source-light.png`,
`dsp-graph.png`, `grading-midgrey.png`, `palette-french.png`, `full.log`,
`tests-final.log` and `tests-regression.log`. They are local QA artifacts and are
excluded from Git and release packages.

## Practical limits

The advanced video arranger, multi-parameter normalized Animation editor and
Composition overlays retain their product-specific behavior; see
[CYUI_MIGRATION.md](CYUI_MIGRATION.md) for the precise CyUI contract gaps.
Some legacy geometry still uses fixed design dimensions. Physical mixed-DPI
multi-monitor operation was not manually tested. Linux/macOS were not built;
the application still has its existing Win32/D3D11 backend only.

The migration checks above preceded the 0.14 alpha packaging.
