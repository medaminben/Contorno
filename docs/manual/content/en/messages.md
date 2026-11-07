---
title: Messages and problems
order: 7
summary: What the message panel says, and what to do when the result looks wrong.
---

## Usual lines {#usual}

| Line | Meaning |
|---|---|
| Imported cowhide.jpg, 1600 x 1200 px. | The photo is shown; its size in pixels. |
| Camera: … | Where the scale came from. See [Where the scale comes from](camera#line). |
| Analysing… | The analysis runs; the busy bar moves. |
| 1 object · 1.411 mm/px · 3.2 s | The result: the objects, the scale and the time taken. |
| Part of the hide was found from the table around it, … | The AI took only part of the hide, for example the dark patches of a spotted hide. The outline was found from the table around the hide instead. Check it before saving. |
| No object found | Nothing on the photo is large enough. The readout shows 0.0 cm². |
| Saved … / Opened … | The file written or read. |

## The AI outline is unavailable {#ai}

*AI outline unavailable: model not found* — the IS-Net model file is missing. The message lists the places looked in. In a build from the sources, build the isnet_model target; in an installation, reinstall.

*AI outline unavailable: Vision was built without LibTorch* — this copy of the program was built without the AI. It shows the photo, but it does not measure: it never falls back to a less exact method without saying so.

## The outline is wrong {#outline}

- **It follows a shadow.** Light the table more evenly, or from above.
- **It cuts into a pale part of the hide.** Use a table whose colour differs more from the hide's.
- **It leaves out a large plain part of a two-coloured hide.** The AI can take a large even area, such as the white of a cowhide, for the table. Contorno then finds the hide from the table around it, and the message panel says so. This needs table all around the hide: where the hide reaches the edge of the photo, the AI's outline stands. Always check the outline before saving.
- **It holds two hides.** Two objects that touch are one object. Leave a gap between them, or measure one at a time.
- **It takes in the table beside a mat.** A mat or cloth smaller than the photo shows two colours at the photo's edge, and the table beside the mat can be taken for hide. Use a mat or cloth that fills the whole photo, or none.
- **A mark or stain is cut out as a hole.** Only holes through which the table is seen should be cut out. Report the photo to support.

## The surface is wrong {#surface}

- Check `Distance to the table` first. An error of 1 % in the distance is an error of 2 % in the surface.
- Check the `Focal length`: the real one, not the 35 mm equivalent.
- Read the camera line: does it take the values from the photo or from the settings, and are they right?
- Check with an object of known size (see [Measure a hide](measure#check)).

## The manual does not open {#manual}

The ? looks for this manual beside the program and opens it in Chromium or Google Chrome (Linux) or Microsoft Edge (Windows), else in the default browser. If none can, a message names the manual's file: open it in any browser.
