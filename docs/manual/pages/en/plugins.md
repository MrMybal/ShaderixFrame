---
id: plugins
title: Native, VST3, OFX and shader plugins
description: Install compatible effects and understand the supported host contracts.
section: Audio and delivery
order: 27
revision: 1
---

Install compatible effects and understand the supported host contracts.

## Using this feature {#usage}

Plugins & MCP exposes catalogue and integration controls. Shader plugins declare plugin.json with relative HLSL/graph assets under the plugin folder. Reloading the catalogue discovers changes; applied shader sources remain embedded in projects. Keep SDK and manifest ABI versions aligned when updating shared components.

| Format | Host path and limits |
| --- | --- |
| Native audio | Windows x64 DLL; ShaderixFramePlugin.h ABI v1; isolated native worker; up to 64 parameters |
| VST3 audio | Windows x64 effects through CyDAWPluginWorker.exe; up to 512 controls; GUI, opaque state and .sfvstpreset |
| OFX image | Supported x64 RGBA float filters in an isolated OFX worker; HDR and alpha preserved |
| HLSL / graph | Manifest-driven shader effects, generators and transitions; copied into project on use |

```steps
{"items": [{"title": "Step 1", "body": "For audio, choose Native / VST3 from a compatible effect chain and select the installed plugin. Open its editor and adjust a simple control before loading a complex preset."}, {"title": "Step 2", "body": "For OFX, use Properties > + Plugin OFX on a video clip. Select a supported filter and test a short region, including alpha and high-range values if required."}, {"title": "Step 3", "body": "Save, close and reopen the project to verify parameter/state restoration. A project stores plugin state, not the plugin binary: the same compatible plugin must exist on the other computer."}, {"title": "Step 4", "body": "If a plugin worker reports a crash or timeout, remove/bypass that instance and test the plugin/version separately. Do not assume every OFX suite, context, GPU API or plugin architecture is hosted."}]}
```

```callout
{"type": "note", "title": "Note", "body": "VST3 requires the separately supplied CyDAW worker. OFX support is a defined filter subset, not universal OFX compatibility. The source package includes SDK headers and licensing information; plugin licences remain their authors’ responsibility."}
```

## Shader plugin manifest {#shader-plugin-manifest}

For the tint example, place plugin.json and tint.hlsl in plugins/studio-tint/. Give the effect a unique ID, then reload plugins in Plugins & MCP. The shader file must remain inside its package directory. API 1 describes ordinary effects; transitions use API 2 and kind: "transition".

```json
{
  "apiVersion": 1,
  "id": "studio.tint-package",
  "name": "Studio Tint",
  "version": "1.0.0",
  "effects": [
    {"id":"studio.tint","name":"Tint","category":"Color","shader":"tint.hlsl"}
  ]
}
```

```callout
{"type": "note", "title": "Note", "body": "OFX requires Filter/General context, Source/Output clips and float depth. Mandatory additional inputs, temporal access, overlays, GPU-only effects, generators and transitions are not supported. Numeric controls are exposed as p0, p1, etc. in plugin units; string/custom parameters stay at defaults. Audio VST3 instruments/MIDI and sidechains are outside this host path."}
```
