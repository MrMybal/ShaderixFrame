---
id: luts
title: 1D, 2D and 3D LUTs
description: Import lookup tables into clip, track, layer or container processing.
section: User guide
parent: grading
order: 21
revision: 3
---

Import lookup tables into clip, track, layer or container processing.

## Using this feature {#usage}

Supported LUT inputs include 1D and 3D .cube files, 2D RG image tables and image atlases representing a 3D table. Apply LUT File from Effects Browser adds a native correction; LUT controls in Color Grading and LUT nodes in layer/track graphs expose the processing at different scopes. A container can use the same graph nodes.

```steps
{"items": [{"title": "Step 1", "body": "Import the LUT and choose the matching image layout if it is an atlas/table. A wrong layout produces scrambled colors."}, {"title": "Step 2", "body": "Confirm what input and output color space the LUT expects. A creative SDR LUT is not automatically a camera-log conversion."}, {"title": "Step 3", "body": "Place balancing before the LUT, then set strength and compare against bypass. Check highlights, saturated colors and neutral grays."}, {"title": "Step 4", "body": "Save the project: LUT data is embedded. Test reopening without the original LUT file to verify the intended self-contained correction."}]}
```

[Working color spaces and 16-bit precision](doc:precision)
