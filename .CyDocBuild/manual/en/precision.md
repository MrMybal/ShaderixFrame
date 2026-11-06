---
id: precision
title: Color spaces and high precision
description: Distinguish processing precision from source and delivery bit depth.
section: User guide
parent: grading
order: 22
revision: 3
---

Distinguish processing precision from source and delivery bit depth.

## Using this feature {#usage}

Compositing, ordinary effects and containers use RGBA16F intermediate surfaces: four half-float channels preserve values beyond the display range during processing. Working spaces include sRGB, linear Rec.709 and linear Rec.2020. Choose the sequence working setup consistently; decoding a source and displaying the result are separate operations.

| Path | Current precision / scope |
| --- | --- |
| Intermediate composition | RGBA16F, including nested compositions |
| PNG16 sequence/frame | 16-bit integer image export before display quantization |
| EXR half sequence/frame | Half-float image export, including HDR values |
| Video decode/encode | Current paths remain 8-bit |
| Display / scopes | SDR display/tone mapping; not an HDR monitor pipeline |

```callout
{"type": "note", "title": "Note", "body": "RGBA16F is not a promise of 16-bit video decoding, 10-bit H264/HEVC, full OCIO/ACES management or HDR-monitor mastering. A third-party effect can also impose its own precision. Use PrecisionStudy.sframe and an EXR half export to inspect high-range results."}
```
