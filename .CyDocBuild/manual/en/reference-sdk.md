---
id: reference-sdk
title: Native audio SDK
description: Declare an isolated stereo audio effect using the C ABI v1.
section: Developer reference
parent: api-links
order: 101
revision: 1
---

## Include and entry point {#L1}

Include the distributed `sdk/ShaderixFramePlugin.h`. Build a Windows x64 DLL and export the C symbol `shaderixframe_plugin_v1` with `SF_PLUGIN_EXPORT` and `SF_PLUGIN_CALL`. The SDK header depends on `<stdint.h>`; clients may use C or C++. The host loads it through `ShaderixFramePluginWorker.exe`.

```cpp
#include "ShaderixFramePlugin.h"
extern "C" SF_PLUGIN_EXPORT const SfPluginV1 *SF_PLUGIN_CALL shaderixframe_plugin_v1(void);
```

## ABI table {#usage}

Return an `SfPluginV1` table with `size = sizeof(SfPluginV1)`, `version = 1`, a valid `name`, all callbacks, `parameter_count` up to 64 and `latency_samples` up to 480000. Parameter values are normalized to 0–1. Declare fixed latency so the mixer can compensate it.

| Member | Public contract |
| --- | --- |
| `parameter_name`, `parameter_default` | Names and initial normalized values by zero-based index |
| `create`, `destroy` | Create and release one processor instance |
| `set_parameter` | Update one parameter on that instance |
| `process` | Read left/right float planes and write separate left/right output planes; return 1 on success |

The current host supplies 48 kHz stereo and blocks of at most 2048 frames. All calls occur inside the worker. Never pass editor pointers, retain audio-buffer pointers or create a UI thread from a callback. Worker failures are reported; a failed export preserves an existing destination.

## Develop and use an effect {#example}

Start from the SDK's short `examples/Gain.cpp` example in the source distribution. Build your DLL, open an audio effect chain and choose **Native / VST3** to load the native effect. Confirm its parameter controls, save the project and check that it restores correctly. A project stores settings, not your DLL.

The callback pages below give exact signatures, arguments, returns and related callbacks. VST3 is a separate host contract requiring the supplied CyDAW worker. [Plugin compatibility](doc:plugins) also covers the supported OFX image-filter path.
