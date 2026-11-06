---
id: media
title: Media bin, importing and proxies
description: Bring video, images and sound into an edit and manage their previews.
section: User guide
parent: projects
order: 4
revision: 3
---

Bring video, images and sound into an edit and manage their previews.

## Using this feature {#usage}

File > Import Media (Ctrl+I) or dropping files imports them. Video with audio creates linked picture and audio clips on separate tracks. The Media tab in Media & Effects holds the bin; adding a bin item can be separate from placing it on the timeline. Double-click a bin card to open Source. Hover a video card to skim its source and inspect duration and format.

Windows Media Foundation decodes video/audio and WIC reads common images; codec availability depends on the machine. Import an actual file to confirm its streams. A filename extension alone does not guarantee decoding. Stills can be trimmed like other clips; generator layers and titles do not need an external image file.

```steps
{"items": [{"title": "Step 1", "body": "Enable Preview Proxies for large media. Automatic proxies can prepare a 720p H264 preview after import; progress appears while background work runs."}, {"title": "Step 2", "body": "Use Prepare Proxy for Selected Clip when needed. A proxy makes editing lighter; keep the original source available because final exports use it."}, {"title": "Step 3", "body": "Use Remove Unused to remove bin entries that are not used by the edit. It does not delete the source files on disk. Relink absent files rather than reimporting a replacement over every cut."}]}
```

[Select source ranges before inserting](doc:source)
