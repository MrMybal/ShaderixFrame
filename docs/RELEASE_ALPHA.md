# ShaderixFrame 0.14.0-alpha.1

Windows x64 alpha preview, licensed under GNU AGPL v3.

## Downloads

- **windows-x64.zip**: portable application; extract to a writable folder and run
  `ShaderixFrame.exe`.
- **Setup.exe**: per-user installer; administrator privileges are not required.
- **source.zip**: corresponding source, including the exact JUCE dependency used
  for the bundled VST3 worker and its build recipe.
- **SHA256SUMS.txt**: SHA-256 checksums of the release assets.

Windows 10/11 x64 and a Direct3D 11 device are required. `--warp` selects software
rendering. The main interface, effect labels and MCP descriptions default to English.
Example projects retain their own names and text.

See [the 0.14 changes](RELEASE_0.14.md) for the modular CyUI interface update.

## Included

- Six workspaces: Edit, Effects, Motion, Color Grading, Audio and Deliver.
- Arrangement timeline, clip filmstrips, waveform display, nested clip timelines,
  automation curves, markers, regions, transitions and detachable panels.
- Shaderixel layer/shader graphs and a library of 406 HLSL effects and generators.
- Half-float RGBA compositing, HDR image export, color grading, video scopes and
  1D/2D/3D LUTs.
- CyDAW v2 DSP graph, mixer/buses/sends, native plugins, Windows VST3 hosting and
  supported CPU RGBA OpenFX filters.
- Built-in MCP stdio server with 160 tools, recovery/version history, export queue,
  MP4/WAV/AVI/PNG/EXR/GIF output and EDL/SRT interchange.

## Alpha limitations

This is a prerelease. Save working copies of important projects before evaluation.
The native media/rendering backend targets Windows. OFX hosting implements the
supported CPU RGBA filter/general contracts; it does not implement every OFX suite
or GPU backend. Native ABI plugins must match the documented version/structure.
VST3 plugins must match the Windows x64 architecture; third-party plugins are not
included. Export codecs depend on the Windows Media Foundation installation.

The application and installer are unsigned. Future versions may change project
schemas and plugin/API contracts. Report reproducible issues with the application
version, steps, expected behavior and a minimal project when possible.

See [README](../README.md), [licensing/source details](LICENSING.md) and
[the 0.13 changes](RELEASE_0.13.md) for further information.
