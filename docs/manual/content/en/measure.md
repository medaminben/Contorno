---
title: Measure a hide
order: 4
summary: From the hide on the table to the surface on the readout.
---

The surface is only as good as the photo. A few minutes spent on the set-up give the same correct result every day.

::: step
**Fix the camera above the table, looking straight down.** The sensor must be parallel to the table. A camera on a stand or a ceiling mount keeps the distance the same for every hide.
:::

::: step
**Measure the distance and enter it once.** Measure from the table top to the camera's lens and enter it in `Set up` as `Distance to the table`, with the `Focal length` and the sensor. See [Set Up](set-up#camera).
:::

::: step
**Lay the hide flat, whole and inside the picture.** Smooth out folds and curled edges: a fold hides surface from the camera. Leave a band of table all around the hide. A table whose colour differs from the hide's helps. A mat or cloth under the hide must fill the whole photo.
:::

::: step
**Light it evenly and take the photo.** Avoid strong shadows along the edge and bright reflections. Keep the camera's own photo file: its camera data helps (see [Where the scale comes from](camera)).
:::

::: step
**Import the photo.** Press `Import` and choose the file (JPEG, PNG, BMP or TIFF). The busy bar moves for a few seconds; then the outline appears on the right and the readout shows the surface.
:::

::: step
**Check the outline, then save.** Zoom on the edges: the green line should run along the hide's edge, not along a shadow or the table. Press `Save` to keep the measurement.
:::

## What is measured {#what}

- The **largest object** on the photo. Smaller things on the table, such as dust, labels or a pen, are left out so that the number always names one surface.
- **Holes** in the hide that show the table are cut out of the surface, and drawn as their own green rings. Holes smaller than 100 mm² are filled.
- The surface is the **flat projection** seen from above: a fold or a wave in the hide is counted as the area it covers, not as the leather inside it.
- Objects that touch each other are one object.

## Check the scale with a known object {#check}

Put a flat object of known size on the table, about as large as a hide, for example a board of 1000 × 1000 mm (1.000 m²), photograph it like a hide and import it. Do not use a small object: the outline lies about one pixel outside the edge, which weighs more on a small object, and a calibration on it would make every hide read too low. If the readout differs, check the `Distance to the table` first: it is the value most often wrong.

To calibrate, change `Distance to the table` in `Set up` until the known object reads right, and keep that distance. The surface changes with the square of the distance: if the readout is 2 % too high, lower the distance by about 1 %. This also corrects the small error of treating the lens as a pinhole (see [Reference](reference#accuracy)). Do it again whenever the camera, the lens or the zoom changes.
