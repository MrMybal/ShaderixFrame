---
id: projects
title: Projects, sequences and media files
description: Understand what a .sframe saves and how to move a project safely.
section: User guide
parent: welcome
order: 3
revision: 3
---

Understand what a .sframe saves and how to move a project safely.

## Using this feature {#usage}

A project contains sequence dimensions, frame rate, duration, tracks, clips, graph data, embedded shader sources, effect settings, automation and nested compositions. Media usually remains referenced by a path. File > New from Template supplies starting formats such as horizontal video, vertical video, podcast and cinema; Sequence > Format changes sequence settings.

```steps
{"items": [{"title": "Step 1", "body": "Use Ctrl+S to save, and a new project filename to keep an independent working copy. Save before a large rearrangement."}, {"title": "Step 2", "body": "Before transferring the edit, use Collect Media and select an archive folder. Check the copied project and its relative media references from that folder."}, {"title": "Step 3", "body": "If media is offline, use File > Relink Missing Media and select the folder containing the relocated files. The search includes subfolders and matches filenames; check the resulting pictures and sound when names are duplicated."}]}
```

Sequence time is measured in seconds and displayed as hours:minutes:seconds:frames. A source in-point selects an offset inside the media; moving a clip changes its sequence start. The time inside a container belongs to its local sequence. Do not change a source file on disk while it is being read for playback or export.

[Versions and recovery](doc:recovery)
