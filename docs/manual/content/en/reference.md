---
title: Reference
order: 8
summary: The formulas, the ranges, the accuracy, the files and the settings.
---

## Formulas {#formulas}

::: formula
scale across (mm/px) = distance × pixel width ÷ focal length
scale down (mm/px) = distance × pixel height ÷ focal length
pixel size (Sensor size mode) = sensor's long side ÷ photo's long side (px)
surface (mm²) = pixels inside the outline × scale across × scale down
:::

The outline drawn and saved is simplified within 0.5 mm of the pixel edge; the surface shown is counted on the pixels themselves, so it equals the white of the mask.

## Ranges {#ranges}

| Value | Range | Default |
|---|---|---|
| Distance to the table | 1–100000 mm | 1500 mm |
| Focal length | 0.1–2000 mm | 13.02 mm |
| Sensor width × height | 0.1–100 mm each | 15.04 × 11.28 mm |
| Pixel width × height | 0.1–100 µm each | 3.76 × 3.76 µm |
| Smallest object or hole kept | setting objects/min_area_mm2 | 100 mm² |

## Accuracy {#accuracy}

The surface grows with the square of the scale, so a relative error in the distance, the focal length or the pixel size counts twice:

| Error in | Error in the surface |
|---|---|
| distance +1 % (15 mm at 1500 mm) | +2 % |
| focal length +1 % | −2 % |
| camera tilted by 2°, hide centred | under 0.1 % |
| camera tilted by 2°, hide 300 mm off centre | about 1.4 % |
| outline 1 pixel too far out, 2 m² hide at 1.4 mm/px | about +0.5 % |
| lens focused on the table (13 mm lens at 1500 mm) | about +1.7 % |

The camera is treated as a pinhole. A real lens focused on the table makes the scale too large by about focal length ÷ distance; the check with a known object (see [Measure a hide](measure#check)) removes this together with any error in the distance. The lens's distortion is not corrected, and the surface is the hide's projection on the table. Keep the hide in the middle of the picture, where the distortion of most lenses is smallest.

## The article {#article}

How the outline is found, how the surface follows from the camera, how both were tested and how large the uncertainty is: see the article [Measuring the surface of animal hides from a single photograph](../../articles/contorno-area-measurement/contorno-area-measurement).

## Files {#files}

- Photos: JPEG, PNG, BMP, TIFF. Camera data is read from JPEG and TIFF.
- Inspections: zip; see [Save and open inspections](inspections#save).
- Settings: indicator.ini, in the application's settings folder (on Linux ~/.config/Contorno/Indicator/).

## Settings kept {#settings}

The camera of `Set up`, the language, the theme, the last folder and the minimum area are kept between sessions. The minimum area has no field in the window; it can be changed in indicator.ini (objects/min_area_mm2, one of 0, 10, 50, 100, 500, 1000, 5000, 10000, 50000 mm²).

## Photos {#photos}

The cowhide in the screenshots is "Stretched Cowhide Skin" by Enda012, Wikimedia Commons (commons.wikimedia.org/wiki/File:Stretched_Cowhide_Skin.jpg), licensed under CC BY 4.0 (creativecommons.org/licenses/by/4.0/). It was reduced in size and shown in Contorno Indicator with its outline.
