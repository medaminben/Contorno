---
title: Save and open inspections
order: 6
summary: One zip file keeps the photo, the camera, the outline and the surface.
---

## Save {#save}

Press `Save` once the readout shows the result. Choose a folder and a name; the photo's name is suggested. While a photo is being analysed, `Save` waits for the result: the message panel says *Wait for the analysis to end.*

The file is an ordinary zip that any program can read:

| File | What it holds |
|---|---|
| original.jpg (or .png, …) | the photo, byte for byte as imported |
| mask.png | the object white on black, at the photo's size |
| camera.json | the distance, focal length and pixel size used, and where each came from |
| parameters.json | the minimum area and the outline tolerance |
| objects.dxf | the outline and the holes in millimetres, for CAD and cutting machines |
| result.json | the photo's size, the scale and the surface in mm² |

## Open {#open}

Press `Open` and choose a saved zip. The photo, the outline and the surface come back exactly as they were saved, without a new analysis, with the camera that was saved. The message panel ends the line with *saved result*.

The next `Import` returns to the camera of `Set up`.

::: note
An inspection saved by an older version of Contorno (a hide inspection) opens its photo and camera and is analysed again with the AI outline. The message panel says so.
:::

## Folders {#folders}

`Import`, `Open` and `Save` all start in the folder of the last file chosen in any of them.
