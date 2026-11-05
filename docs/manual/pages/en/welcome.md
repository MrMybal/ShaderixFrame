---
id: welcome
title: About this manual
description: Learn video editing, node compositing, animation and audio in ShaderixFrame. This manual works entirely offline.
section: Getting started
order: 0
revision: 1
---

Learn video editing, node compositing, animation and audio in ShaderixFrame. This manual works entirely offline.

## Using this feature {#usage}

Open **Help > User Manual** or press **Ctrl+F1** in the editor. CyDoc opens from the installed `help/index.html` using a local file URL. It needs no web server, CDN or Internet access.

Choose English or Français at the top. Page, section and media IDs are shared across languages. Search with **Ctrl+K**; browser **Ctrl+F** searches the current page. Use browser Back/Forward or the contents to navigate. Reading positions are retained within the viewer session.

Images use minimal controls: hover or focus to zoom, pan or enlarge inside this window; Escape reduces the image. Touch devices retain visible controls. My space stores bookmarks and personal notes locally when browser file-origin storage allows it. Use Print guide for the complete illustrated guide, or browser Ctrl+P for the current page.

```callout
{"type": "note", "title": "Note", "body": "This guide describes the implemented 0.14 alpha workflows. Demo projects and screenshots may contain French clip names and titles. Those are project content, not the interface language. Technical restrictions are called out in the relevant chapters."}
```

```diagram
{"type": "flow", "title": "Import / Source → Timeline / clips → Effects / graphs / audio → Export / delivery", "nodes": [{"id": "node-0", "label": "Import / Source", "x": 20, "y": 65, "width": 155}, {"id": "node-1", "label": "Timeline / clips", "x": 240, "y": 65, "width": 160}, {"id": "node-2", "label": "Effects / graphs / audio", "x": 465, "y": 65, "width": 175}, {"id": "node-3", "label": "Export / delivery", "x": 705, "y": 65, "width": 175}], "edges": [{"from": "node-0", "to": "node-1"}, {"from": "node-1", "to": "node-2"}, {"from": "node-2", "to": "node-3"}]}
```

[Start your first edit](doc:quickstart)
