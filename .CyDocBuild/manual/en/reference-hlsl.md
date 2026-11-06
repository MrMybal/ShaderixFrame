---
id: reference-hlsl
title: HLSL effect interface
description: Use public shader inputs, time values, passes and frame history.
section: Developer reference
parent: api-links
order: 102
revision: 1
---

## Effect entry point {#api}

Ordinary image effects use `float4 Effect(EffectContext ctx)`. The host supplies `ctx.px`, `ctx.uv`, `ctx.size`, `ctx.texel`, `ctx.pass`, `ctx.time`, `ctx.localTime`, `ctx.deltaTime`, `ctx.frame` and `ctx.sourceSize`. Sequence time and mapped clip-source time are distinct. Return the output RGBA value, retaining alpha when the effect should not alter it.

| Public function | Coordinates and behavior |
| --- | --- |
| `Input(index, px)` / `InputUV(index, uv)` | Pixel / normalized coordinates for an input image, up to four inputs |
| `Prev(px)` / `PrevUV(uv)` | Previous pass of a multipass effect |
| `History(px)` / `HistoryUV(uv)` | Previous frame after the clip effect stack |
| `HasHistory()` | Whether valid frame history is available |
| `HasInput(index)` | Whether that input image is available |
| `Premultiply(c)` / `Unpremultiply(c)` | Convert an RGBA value to/from alpha-premultiplied form |

`//@passes N` requests 1–8 passes. Seeking or editing resets history. Input 1 can read lower layers. These are host-provided shader functions, not C++ SDK includes. Transitions use their own two-image/progress contract and remain single-pass. `ctx.transitionProgress` contains their current progress.

## A short example {#example}

```hlsl
//@param amount float 0.5 0 1 "Amount"
float4 Effect(EffectContext ctx) {
    float4 c = InputUV(0, ctx.uv);
    c.rgb = lerp(c.rgb, c.rgb * float3(1.0, 0.8, 0.6), amount);
    return c;
}
```

See [Shader Graph and HLSL](doc:shaders#a-minimal-tint-shader) for color controls and vector components, and [plugin manifests](doc:plugins#shader-plugin-manifest) for packaging the shader. Shader Graph builds image processing; Layer Graph blends timelines; DSP Graph processes stereo audio.
