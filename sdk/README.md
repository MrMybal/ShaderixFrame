# ShaderixFrame Native SDK v1

Include `ShaderixFramePlugin.h` and export `shaderixframe_plugin_v1` with C linkage.
Return an `SfPluginV1` table with its exact size and `version=1`. All callbacks are required.
`create` prepares a stereo processor at 48 kHz with blocks of at most 2048 frames.
`process` receives four independent float planes and returns 1 on success.
Parameter values are normalized to 0..1, with at most 64 parameters.
Declare fixed latency in samples in `latency_samples` (maximum 480000).

Each instance runs in `ShaderixFramePluginWorker.exe`. A crash or hang is reported to the
calling engine; a failed export preserves an existing destination. Callbacks must not use
editor pointers, create their own UI thread, or keep buffer pointers after `process` returns.

`examples/Gain.cpp` provides a neutral effect at p0=0.5. `examples/Latency.cpp` is a latency
compensation fixture. CMake targets `native_gain` and `native_latency` build them.
SDK DLLs, parameter profiles and headers do not depend on the Shaderixel or CyDAW projects.
HLSL shaders retain their separate API 1/2 manifest.

## VST3 integration

VST3 uses the separately supplied CyDAW v2 worker and IPC protocol 4. Its JUCE licence and
provenance accompany the worker. The native SDK does not depend on JUCE.
Opaque plugin state, native editor windows and full VST3 presets remain separate from native ABI v1.
See [the build guide](../docs/BUILDING.md) for restoring optional binary components.

## OpenFX example

`examples/OfxGain.cpp` exports the official OpenFX ABI independently of the audio SDK.
Build it with the ASWF headers (`vendor/openfx/include` in the source tree, `sdk/openfx`
in the portable ZIP), DLL linkage and an `.ofx` suffix; CMake target: `ofx_gain`.
The gain processes float RGBA Source/Output clips in the Filter context.
ShaderixFrame's OFX host uses its own worker and preserves alpha and HDR values.
