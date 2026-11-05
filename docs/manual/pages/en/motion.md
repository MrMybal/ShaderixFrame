---
id: motion
title: Motion design and widget hierarchy
description: Build animated layouts from text, shapes, images and groups.
section: Editing and graphics
order: 15
revision: 1
---

Build animated layouts from text, shapes, images and groups.

## Using this feature {#usage}

```image
{"id": "motion-image-1", "src": "assets/motion-design.png", "alt": "Widget hierarchy, title selection and rendered layout.", "controls": "minimal"}
```
Widget hierarchy, title selection and rendered layout.

```steps
{"items": [{"title": "Step 1", "body": "Open examples/MotionStudy.sframe and switch to Motion (F3). The hierarchy lists groups, shapes, title and accent elements."}, {"title": "Step 2", "body": "Create rectangle, ellipse, text, image/video or group elements. Select in the tree or canvas. Move or resize the selection with its handles."}, {"title": "Step 3", "body": "Set the parent to build a hierarchy. Anchor describes attachment in parent space; pivot describes the element’s transform origin. Adjust position, width and height while watching the canvas."}, {"title": "Step 4", "body": "Use visibility and child clipping to control groups. Animate position/size at successive times with K, then refine curves in Animation."}, {"title": "Step 5", "body": "Open an element’s clip for local effects or nested timelines when a more complex animation is required. Apply changes to the parent and check final composition."}]}
```

For a reusable lower third, group a background shape with name and role text. Anchor the group near a safe-area edge, animate its arrival and exit, then keep the whole assembly inside a container. Test long text and the target output aspect ratio before export.
