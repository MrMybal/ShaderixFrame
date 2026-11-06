---
id: shaders
title: Shader Graph and HLSL
description: Author image effects with Shaderixel nodes or shader source code.
section: User guide
parent: composition
order: 12
revision: 3
---

Author image effects with Shaderixel nodes or shader source code.

## Using this feature {#usage}

```image
{"id": "shaders-image-1", "src": "assets/shader-graph.png", "alt": "An image input, desaturation node, animated parameter and output.", "controls": "minimal"}
```
An image input, desaturation node, animated parameter and output.

```steps
{"items": [{"title": "Step 1", "body": "Select the video clip or graph node to edit, then open Shader Graph. Add nodes from the catalogue or import a compatible Shaderixel graph JSON."}, {"title": "Step 2", "body": "Connect typed ports from the image input through processing to the output. Select a node for parameters; use Fit All, wheel zoom and middle-drag to navigate."}, {"title": "Step 3", "body": "Use parameter nodes to expose values for animation. Compile/apply the graph and inspect the composition. Validation and shader compiler errors identify incompatible ports or invalid code."}, {"title": "Step 4", "body": "Open HLSL for text editing. Start from an existing working library effect, change a small part, compile and compare. Shader source and settings are copied into the project."}]}
```

The native HLSL path supports up to four inputs, multiple passes and frame history for ordinary effects. The exact uniforms, input declarations and metadata depend on the effect contract: copy the declaration style of the supplied shaders. Transitions have a separate two-image/progress contract and remain single-pass. A shader valid in another host can require adaptation.

[Effect and plugin contracts](doc:plugins)

## A minimal tint shader {#a-minimal-tint-shader}

Use this as the replacement code of an ordinary video effect in HLSL, then compile. The metadata creates a color control and a 0–1 blend parameter. InputUV(0, ctx.uv) samples the original input of this effect; alpha is preserved.

```hlsl
//@param tint color 1 0.7 0.3 1 "Tint"
//@param mixAmount float 0.5 0 1 "Amount"
float4 Effect(EffectContext ctx) {
    float4 c = InputUV(0, ctx.uv);
    c.rgb = lerp(c.rgb, c.rgb * tint.rgb, mixAmount);
    return c;
}
```

EffectContext provides px, uv, size, texel, pass, time, localTime, deltaTime, frame and sourceSize. time is sequence time and localTime is mapped source time. InputUV uses normalized coordinates; Input uses pixel coordinates. Input 1 can read lower layers. //@passes N declares 1–8 passes; Prev/PrevUV reads the previous pass. History/HistoryUV and HasHistory() access the previous frame after the clip effect stack. Seeking or editing resets history.

Metadata uses //@param name type defaults min max "Label". color takes four values and a label; bool takes one value and a label. Vector components are addressed as name.0 through name.3 for animation/MCP. Parameter identifiers must be valid HLSL names; the host accepts up to 64 parameters per shader effect.
