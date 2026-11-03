# Modular CyUI migration

This is a source migration after the published 0.13 alpha; it does not replace the
published alpha or change the project schema. Existing local engine/audio changes
were retained. The integration is included in 0.14.0-alpha.1; upstream checkouts remain unmodified.

## Dependency boundary

CyUI **1.2.0**, commit `8367401fe45fc22bfed5e227cb3de343177f676b`, is an
unmodified source snapshot in `vendor/cyui`. `cyui.lock.json` fingerprints every
included file. See [BUILDING.md](BUILDING.md) for verification and explicit updates.
Upstream AI_GUIDE, MODULES and LOCALIZATION were read before integration. Browser,
CEF, Unreal, Python GUI and ImGuizmo adapters are not used.

The existing Dear ImGui **1.93 docking** target is passed to CyUI; CyUI's second
ImGui copy is excluded. C++20 was already the application standard and also meets
CodeEditor's requirement. Production uses explicit module targets, never `All` or
`CyUI::CyUI`. Demo, Gallery, library tests and CEF are disabled. Audio is explicitly
on because mixer, device, DSP and timeline controls use it. No service, UI runtime,
font binary or media dependency is downloaded by this integration.

| Targets | Actual application use |
| --- | --- |
| Core | Semantic palette, fonts, glyph buttons, toggles, cards, locale and notifications |
| Audio | Knobs, faders, meters in mixer, device chain, track headers and DSP parameters |
| Canvas | Common transforms and navigation, including motion-design viewport |
| Nodes | Shader graph, track/layer composition graph, DSP graph and read-only audio routing graph |
| Plots + AudioViews | Host-computed waveform, spectrum and oscilloscope |
| Curves | DSP automation and grading curves; host interpolation/constraints stay authoritative |
| Timeline | Source monitor's trim/move span, time ruler and seeking |
| Images | Source image/video view with zoom, pan and fit |
| Inspector | Validated sequence duration/frame-rate fields |
| Commands | Command ranking, modal palette, navigation and activation |
| CodeEditor (+ automatic Code) | Editable HLSL and DSP JSON, generated read-only HLSL, shared syntax backend |

Code's dependency on Lexilla and system threads, and CodeEditor's TextEditor
backend, are resolved by upstream CMake. The application does not compile a second
syntax editor or include Scintilla UI. The snapshot retains the applicable MIT,
Lexilla/Scintilla and SciTE notices; the package script copies them to
`licenses/cyui/`.

## Replacements and ownership

- Removed generated `CyDawWidgets.h/.cpp`, `ShaderixelNodeVisuals.h` and
  `ShaderixelNodeDrawing.inl`. The generator now extracts graph/blend algorithms
  and parameter captions only. It no longer copies controls, cards, mixer drawing,
  node drawing or arranger drawing.
- Replaced Shaderixel's compiled theme implementation with a thin local bridge
  to the current CyUI context. `FrameStyle` delegates its controls and semantic
  roles. Saved track colors and RGB scope/image data remain document/render data.
- Removed the local command ranker/modal renderer, manual node hit testing,
  cables, selection and navigation in the three editable graph canvases, manual
  grading curve gestures and simple source picture/scrub rendering.
- Mixer strips and device chains compose common controls around host routing,
  effects, drag/drop and plugin actions. No DSP or engine algorithm moved into
  the UI library. The routing overview uses the shared layout/node renderer.
- HLSL compilation, media decoding, GPU textures, FFT/signal analysis, project
  persistence, validation, automation interpolation and history remain local.

Node/pin IDs derive from stable document IDs and port numbers. DSP boundary IDs
are explicitly mapped. Common editors preview locally; host data changes on
commit, with a single history checkpoint for a drag. Revisions cancel stale
node/curve edits. Endpoint constraints, hue continuity and the grading limit of
32 points remain host policies. Properties above the shader canvas occupy a fixed
scrollable region so selection cannot move the canvas during a gesture.

Windows' legacy `pascal` macro is scoped out around the public Code header in the
host adapter. The snapshot itself remains unchanged. Shared command ranking is
ASCII-case-folding only; the host uses Windows Unicode decomposition for its
accent-insensitive search policy and supplies translated search aliases. Keypad
Enter remains a host shortcut. Code widgets own their keyboard focus so typing,
Delete, Space and Ctrl+Z do not edit the sequence behind them.

## Appearance, locale and workspaces

English remains the default. **View > Appearance** selects five CyUI themes,
compact/comfortable density and English/French. Preferences are saved in
`ui-preferences.json` beside the executable and excluded from Git. Hidden/capture
runs use temporary in-memory preferences. Host translations come from the
existing English-presentation map; missing strings fall back to English. Saved
panel names/IDs, command IDs, file paths and user content are not translated.
This preserves existing docking layouts and nested clip workspace identities.

The main window reads its Win32 DPI and reapplies common metrics before each
frame. Windows can still detach/dock across monitors. Per-context theme/font and
locale state is supplied by CyUI. Physical mixed-DPI multi-monitor interaction
has not been tested; automated captures exercise a 150% scale override. Legacy
custom timeline/overlay geometry still has some fixed design dimensions.

For repeatable hidden visual checks, combine `--hidden --ui-snapshot=PATH` with
`--ui-theme=mid-grey|light-grey|amber-night|violet|navy`,
`--ui-density=compact|comfortable`, `--ui-locale=en|fr` and `--ui-scale=1.5`.
These switches do not save personal appearance settings.

## Product-specific views retained

The main arranger remains a product adapter. CyUI Timeline exposes flat lanes,
clip move/trim, markers, range selection and seeking. It does not provide the
existing nested track groups, sticky image row, linked video/audio edits,
ripple/roll/slip/stretch/slide tools, track effects popovers, transitions,
filmstrip/proxy decoding or per-parameter automation lanes. Replacing the entire
arranger with it would remove these behaviors. Its source-monitor-compatible
subset is migrated; the main arranger reuses common controls, roles and canvas
math while keeping its editing model and specialized rendering.

The Animation panel overlays parameters with independent normalized ranges,
cross-effect multi-selection, clipboard addressing, video-frame snapping and
Alt-bending using CyDAW shaped interpolation. CyUI Curves has one time/value axis
per editor and does not represent those combined interactions. That product view
is retained without copying CyUI. DSP automation and grading, which fit the
shared contract, now use Curves. The richer Composition view retains pixel lock,
hand/Shift-wheel navigation, minimap, comparison wipes, guides and grading picks;
the basic source viewer uses Images. Color wheels, qualifier masks and scope RGB
accumulation remain domain-specific GPU/color tools.

The horizontal grading stack is an ordered effect-selector toolbar, not an
independently editable routing graph. Routing edits use the shared Nodes views.

## Validation

The validation report for the current working tree is recorded separately in
[CYUI_VALIDATION.md](CYUI_VALIDATION.md). Checks include a fresh Release configuration, application
and relevant test targets, snapshot/generator integrity, engine/export/plugin/MCP
suites, actual mouse/keyboard editor interactions, stale-revision cancellation,
locale/identity/context isolation, code text fidelity, themes, density and DPI
captures. Debug audio-quality binaries remain disabled because of the previously
reported antivirus detection. No antivirus exclusion is added.

Only **Windows x64** was built and exercised. The existing Win32/D3D11 backend
already rejects non-Windows configurations; Linux/macOS support is not claimed.
