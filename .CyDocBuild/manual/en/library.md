---
id: library
title: Effects browser and Shaderixel library
description: Find and apply effects, blends, generators, transitions and audio devices.
section: User guide
parent: composition
order: 13
revision: 3
---

Find and apply effects, blends, generators, transitions and audio devices.

## Using this feature {#usage}

The embedded Shaderixel catalogue contains 406 effects and generators. Shaderixel Library searches names/categories; Effects Browser provides cards with background-rendered previews, including blend and transition families and CyDAW audio effects. Preview on selected clip evaluates cards against the actual selection rather than the reference scene.

| Card type | Double-click result |
| --- | --- |
| Video effect | Adds an effect to the selected clip |
| Generator | Creates a new layer/clip |
| Blend | Sets the selected clip’s track blending |
| Transition | Adds an incoming transition to the clip |
| Audio device | Adds to the Track Effects channel |

Cards can drag onto the timeline; effect cards can also target a track FX button or graph. The clip’s fx badge opens its stack. Use bypass to compare and reorder effects deliberately, because changing their order changes the result. Apply LUT File loads a .cube or image table as a native color correction.

```callout
{"type": "note", "title": "Note", "body": "Preview generation is asynchronous. A card can briefly be waiting while other previews render. Library source is embedded when applied; updating the library does not silently replace an effect already stored in a project."}
```
