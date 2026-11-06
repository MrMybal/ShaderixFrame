---
id: glossary
title: Glossary and processing scopes
description: Keep the different timelines, graphs and processing targets distinct.
section: User guide
parent: welcome
order: 37
revision: 3
---

Keep the different timelines, graphs and processing targets distinct.

## Using this feature {#usage}

| Term | Meaning |
| --- | --- |
| Source | Media file and a range inside it |
| Clip | Placed content with start, duration, source mapping, effects and keys |
| Track / layer | A timeline lane whose clips produce an image surface or audio signal |
| Container | A clip containing an independent local sequence |
| Clip effect | Processing attached to one clip before later composition |
| Track Graph | Processing of a video track surface |
| Layer Graph | Composition and blending of the track surfaces |
| Shader Graph | A typed graph compiled into an image-processing shader |
| DSP Graph | Stereo audio topology compiled through CyDAW’s render plan |
| Bus / send | Shared audio sum / additional routing connection |
| Keyframe | A value at a time, interpolated into automation |
| Work area | A time interval used for looping, caching or export |
| Proxy | Lightweight editing copy; original used for final export |
| RGBA16F | Four-channel half-float intermediate image precision |
| LUT | Color lookup table |
| LUFS / dBTP | Program loudness / true peak |
| Worker | Separate process hosting a compatible plugin |

[Return to the manual introduction](doc:welcome)
