---
title: Where the scale comes from
order: 5
summary: The photo's camera data, the settings, and the line that says which was used.
---

The surface is the number of pixels inside the outline times the surface of one pixel on the table. That pixel's size, the **scale** in mm/px, comes from three values:

::: formula
scale = distance to the table × pixel size ÷ focal length
:::

## The photo first, then the settings {#order}

When a photo is imported, Contorno Indicator reads the camera data a camera writes into its JPEG or TIFF files (EXIF):

- **Focal length** — from the photo when it has one between 0.1 and 2000 mm.
- **Pixel size** — from the photo's focal-plane resolution, or else worked out from its 35 mm equivalent focal length, when the result is between 0.1 and 100 µm.
- **Distance** — always from `Set up`. The photo cannot know how high the camera was.

A value the photo does not have, or has outside these ranges, comes from `Set up` instead, on its own.

The pixel count always comes from the photo itself, after turning it upright. A photo made smaller after it was taken is measured correctly in `Sensor size` mode, because the sensor's size is spread over the pixels the photo really has.

## The camera line {#line}

After each import one line in the message panel says where each value came from and the scale that results, for example:

*Camera: focal length 5.56 mm and pixel size 5.23 µm (35 mm equivalent) from the photo, distance 1500 mm from the settings: 1.411 mm/px.*

Other forms:

- *no camera data in the photo, settings used* — the photo has no usable EXIF data (a PNG, a screenshot, an edited copy).
- *the photo's focal length … is not valid* — a value was found but is out of range; the setting was used.
- *the photo may be cropped* — the photo's sides are not in the same ratio as the sensor's. If it was cropped, the scale from the sensor size is wrong: use the original photo.

::: warning
The 35 mm equivalent is rounded by the camera maker, so a pixel size taken from it is approximate. For the best accuracy enter the true sensor or pixel size in `Set up` and use photos with the focal-plane resolution in their data, or check the scale with a known object.
:::

## Non-square pixels {#non-square}

Some sensors have pixels that are not square. Give the pixel's width and height in `Pixel size` mode, with the chain off. The scale then has two values, one across and one down the photo, and the surface is counted with both.
