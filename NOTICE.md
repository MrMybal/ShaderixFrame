# Component provenance and licences

ShaderixFrame is developed and maintained by Cyberalien. Original ShaderixFrame code
is licensed under GNU Affero General Public License version 3 (AGPL-3.0-only); see LICENSE.
Existing third-party copyright notices and licences remain applicable.

- `vendor/shaderixel/`: owner-authorized copies of Shaderixel's theme, common/JSON
  types, EffectGraph compiler, document graph, NodeEditor, blending shaders and
  **406** effect/generator shaders. Deterministic adapters in `src/adapters` and
  `plugins/shaderixel-library` supply the video/D3D11 boundary and English labels.
  The original snapshots and the upstream project remain unchanged.
- `vendor/cydaw2/`: CyDAW v2 graph definitions, AudioGraph/AudioRenderPlan, buffers,
  analysis, DSP catalogue/processors, widget/theme and graph/mixer/arrangement
  references. CyDAW is licensed under AGPL v3; its licence is preserved in
  `vendor/cydaw2/LICENSE` and included as `CYDAW-LICENSE.txt` in Windows packages.
  The IPC-4 VST3 worker is rebuilt from the locked `PluginWorkerMain.cpp` and
  `PluginIsolation.h`, with JUCE 8.0.15, for this alpha. The test VST3 fixture is
  development-only and is not distributed in the Windows package.
- JUCE 8.0.15: used by the VST3 worker under its AGPL v3 option. Original notices
  are in `vendor/juce-license/LICENSE.md` and `JUCE-LICENSE.md` in the package.
  The matching source release includes the JUCE source used by the worker,
  including its dependencies and their notices. Unused Android Firebase demo
  configurations are omitted. See `docs/LICENSING.md`.
- `vendor/cyui/`: [CyUI](https://github.com/MrMybal/CyUI) 1.2.0, commit
  `8367401fe45fc22bfed5e227cb3de343177f676b`, under [MIT](https://github.com/MrMybal/CyUI/blob/8367401fe45fc22bfed5e227cb3de343177f676b/LICENSE). The licence is also provided locally in `vendor/cyui/LICENSE` and in the package. It supplies common
  controls, themes, fonts, canvases, nodes, curves, source timelines, plots,
  inspectors and code editing. Its own ImGui copy is omitted: the application
  provides the single ImGui target. CodeEditor includes the MIT TextEditor;
  Code includes Lexilla, Scintilla headers and SciTE syntax data under their
  permissive licences. Original licences and notices are preserved in the
  snapshot and copied to `licenses/cyui/` by the binary packager. See
  `vendor/cyui/THIRD_PARTY_NOTICES.md` and `cyui.lock.json`.
- `vendor/imgui/`: the Dear ImGui docking snapshot used by Shaderixel; MIT licence
  in `vendor/imgui/LICENSE.txt`, distributed as `IMGUI-LICENSE.txt`.
- `vendor/openfx/`: ASWF OpenFX headers/examples at commit
  `e40728885390ec16276d11e00025de9b4282060c`, recorded in `openfx.lock.json`.
  BSD 3-Clause licence in `vendor/openfx/LICENSE.md`, distributed as
  `OPENFX-LICENSE.txt`. The unmodified Basic example is a test fixture; the host
  worker and OfxGain example are ShaderixFrame code.
- Root logos were supplied for this project. The procedural imagery, local shaders
  and generated demonstration soundtrack were created for ShaderixFrame.
  Windows Segoe UI and Consolas fonts are loaded from the operating system and
  are not redistributed; Dear ImGui's font provides the fallback.

The native plugin ABI, its worker and Gain/Latency examples, and the video, DSP,
cache and export adapters are ShaderixFrame code. Third-party plugins loaded by
users retain their own licences and are not included in the release.

This licence grant concerns this distribution. It does not change the licensing
or ownership of the separate Shaderixel or CyDAW projects.
