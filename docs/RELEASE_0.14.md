# ShaderixFrame 0.14.0-alpha.1

## Modular interface

- Integrate CyUI 1.2.0 at a locked revision, with explicit CMake modules and the
  application's existing Dear ImGui docking target.
- Use common node canvases for shader graphs, layer/track composition graphs,
  DSP graphs and audio-routing overview, with zoom, pan, selection and connections.
- Replace copied audio widgets with shared knobs, faders and meters in mixer,
  track controls and device chains.
- Use shared grading/DSP curve gestures, preserving host interpolation,
  grading constraints and project Undo/Redo.
- Add syntax-aware HLSL/JSON editing, shared command palette and sequence inspector.
- Use common image navigation and trim/move timeline for the Source monitor.
- Add five themes, compact/comfortable density and English/French preferences
  under View > Appearance. English remains the default.
- Remove generated copies of widget/card/node-rendering implementations while
  retaining Shaderixel graph compilation/compositing and CyDAW DSP processing.

## Audio and track workflows

- Include the expanded CyDAW stereo DSP catalogue and processing families.
- Provide track device chains, an effects browser, and per-video-track effect graphs.
- Preserve plugin isolation, bus/send routing, automation and nested clip workspaces.

## Dependencies and source

CyUI is MIT-licensed. The licence and all
applicable syntax/editor notices are bundled with the portable application,
installer and corresponding-source package. See [LICENSING.md](LICENSING.md).

## Alpha scope

Windows 10/11 x64, Win32/D3D11. The advanced video arranger and multi-parameter
Animation view retain product-specific interactions not covered by CyUI's generic
contracts. See [CYUI_MIGRATION.md](CYUI_MIGRATION.md). Physical mixed-DPI
multi-monitor operation has not been manually tested. OFX remains limited to the
supported CPU RGBA filter/general contracts. Third-party plugins are not bundled.

Existing project names, text and source paths remain user data. Test edits on
working copies; this is a prerelease. The application and installer are unsigned.
