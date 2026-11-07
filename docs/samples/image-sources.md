# Sample images: where each one comes from

Every photograph in the repository that Contorno did not make itself, with
its origin, licence, what was changed, and what it is used for. Checked on
2026-10-30 against the Wikimedia Commons API (`extmetadata`: author, licence,
date). The machine-readable short list is `SOURCES.md`, in this folder;
keep both in step when a sample is added or removed.

Rules for a new sample:

- Only public-domain or openly licensed photos whose author and licence can be
  shown (Commons file page or equivalent). No photos taken from shops, reviews,
  social media or image search results.
- Record the source URL, the author, the licence, the original size, and every
  change made, here and in `SOURCES.md`, in the same commit.
- A CC BY photo needs its credit wherever it, or anything drawn from it, is
  published (article, manual, screenshots).

## 1. Photos in `docs/samples/leather/`

All Commons files: `https://commons.wikimedia.org/wiki/File:<Commons file>`.
"Re-encoded" means decoded and saved again as JPEG quality 90 with all
metadata (EXIF, XMP, ICC comments) dropped; "resized" means scaled down so the
longest side is at most 1600 px.

| File in the repo | Commons file | Author, date | Licence | Original → stored (px) | Changes |
|---|---|---|---|---|---|
| `calf_black.jpg` | [Kalb schwarzgefaerbt.jpg](https://commons.wikimedia.org/wiki/File:Kalb_schwarzgefaerbt.jpg) | kuerschner (own work) | public domain | 1334 × 1602 → same | none: byte-identical (SHA-1 935588f9…) |
| `cowhide_stretched.jpg` | [Stretched Cowhide Skin.jpg](https://commons.wikimedia.org/wiki/File:Stretched_Cowhide_Skin.jpg) | Enda012 (own work), 2026-09-29 | **CC BY 4.0** | 4064 × 3048 → 1600 × 1200 | resized |
| `calf_spotted.jpg` | [Calf fur skin, spotted.jpg](https://commons.wikimedia.org/wiki/File:Calf_fur_skin,_spotted.jpg) | Kürschner, 2008-11-11 | public domain | 1531 × 1414 → same | re-encoded |
| `chekiang_lamb.jpg` | [Chekiang lamb fur skin.jpg](https://commons.wikimedia.org/wiki/File:Chekiang_lamb_fur_skin.jpg) | Kürschner, 2009-06-26 | public domain | 1880 × 1259 → 1600 × 1071 | resized, re-encoded |
| `indian_lamb.jpg` | [Indian lamb fur skin.jpg](https://commons.wikimedia.org/wiki/File:Indian_lamb_fur_skin.jpg) | Kürschner, 2008-10-19 | public domain | 1656 × 1503 → 1600 × 1452 | resized, re-encoded |
| `reindeer.jpg` | [Reindeer fur skin 2.jpg](https://commons.wikimedia.org/wiki/File:Reindeer_fur_skin_2.jpg) | Kürschner, 2009-06-01 | public domain | 1399 × 1079 → 1280 × 987 | Commons thumbnail, re-encoded |

Credit lines:

- Cowhide (required by CC BY 4.0, also for every figure, screenshot or file
  drawn from it): *"Stretched Cowhide Skin" by Enda012, Wikimedia Commons,
  CC BY 4.0 (https://creativecommons.org/licenses/by/4.0/), resized; outlines
  added.*
- Public-domain photos: no credit is required; as a courtesy, name
  *"Kürschner / kuerschner, Wikimedia Commons, public domain"*.

## 2. What each photo is good for

| File | Shows | Use |
|---|---|---|
| `calf_black.jpg` | black calf, whole, flat, light background | measurement; tests (`Objects.Calf*`, the Indicator tests, self-test); article Fig. 4, Table 5 |
| `cowhide_stretched.jpg` | cowhide stretched on a frame | measurement; article Figs. 2, 3, 8, 13; manual screenshots; model comparison |
| `chekiang_lamb.jpg` | whole skin, top-down, background visible | measurement (`real.csv`); test `Objects.WholeSkinDoesNotReachBorder` |
| `reindeer.jpg` | whole skin, top-down, background visible | measurement (`real.csv`) |
| `indian_lamb.jpg` | whole skin, close to the frame | measurement (`real.csv`); test `Window.ImportCountsFromTheAreaShown` |
| `calf_spotted.jpg` | black and white calf on a dark carpet; the skin touches the frame, so the area is not fully in the photo | robustness only; article Fig. 5 (the outline only, no area) and `Objects.SpottedCalf*`, `Objects.BackgroundCheck*` |
| a drawn photo (two brown ellipses on grey, written by the test) | one large and one small object | test `Objects.OnlyTheLargestIsKept` |
| `../cowhide_analysis_bad_result.zip` | a version 2 export | tests `Inspection.V2StillOpens`, `V2KeepsSavedScale`, `ReopenedV2ExportsConsistentCamera` |

The article's experiment (`docs/articles/contorno-area-measurement/scripts/experiment/measure.py`) runs on all six and writes `real.csv`.

## 3. Files derived from these photos

| File | Made from | Licence |
|---|---|---|
| `docs/samples/leather/cowhide_marked.png` | cowhide, outline drawn | CC BY 4.0 (credit as above) |
| `docs/samples/*.zip` | Contorno exports of the cowhide saved while testing, see §3.1 | CC BY 4.0 (credit as above) |
| `docs/articles/contorno-area-measurement/model_comparison/cowhide_stretched__*`, `grid_cowhide_stretched.jpg` | cowhide, cut-out models | CC BY 4.0 (credit as above) |
| `docs/articles/contorno-area-measurement/figures/*`, `docs/manual/**/*.png` | the cowhide, the calves, or synthetic | the photo's licence, as above; synthetic figures are the project's own |

### 3.1 Saved test exports: where the contour was wrong

The project's author saved these exports from the application when the
algorithm failed to find the exact contour of the cowhide. Each holds the photo as analysed
(`original.*`), the drawn result (`processed.png`), the camera and the
parameters, the outline as DXF and the result as JSON. Same photo, same
camera (1500 mm, 1600 × 1200 px), so the areas compare directly.

| File | Method | Result | What went wrong |
|---|---|---|---|
| `docs/samples/cowhide_stretched.zip` (2026-10-28) | classical OpenCV pipeline (old `conqueror-hide` format) | outline 628 points; 69 "defects" of 771 174 mm²; usable area 526 320 mm² | The colour threshold reads the black patches and spots of the coat as defects, so more than half the hide is subtracted. The outer outline is right; the classification inside it is not. |
| `docs/samples/cowhide_analysis_bad_result.zip` (2026-10-29) | IS-Net, before commit 9c5f8b2; input was `cowhide_marked.png`, a processed image opened as a photo | 1 object with **1 hole** (2132 points); area 1 156 616 mm² | The red marks on the hide left the model unsure in the lower black patch (mean probability 0.40), and that patch became a false hole: **−11.8 %** of area. |

The fix is held by the test `Objects.MarkedHideHasNoFalseHole`
(`libraries/Vision/test/src/test_Objects.cpp`), which runs on
`cowhide_marked.png`. `cowhide_analysis_bad_result.zip` is the version 2
export the Indicator tests open (`WxInspection.V2StillOpens`, `WxInspection.V2KeepsSavedScale`,
`WxInspection.ReopenedV2ExportsConsistentCamera`); `WxInspection.SampleZipsOpen` opens every
zip under `docs/samples`.

## 4. Removed on 2026-10-30

| File | Why |
|---|---|
| `leather/leather.jpg` | a customer's photo from an Amazon review: the customer holds the rights (§ 72 UrhG); the licence to Amazon does not reach third parties |
| `leather/leather_black.jpg`, `leather_black_dim.jpg` | origin unknown: no metadata, not found on Commons |
| `leather_analysis.png`, `model_comparison/leather__*`, `grid_leather.jpg`, `zoom_leather.jpg` | derived from `leather.jpg` |

They are gone from the working tree but still in the git history; remove them
from the history (e.g. `git filter-repo --path … --invert-paths`) before the
repository is published.

## 5. Removed on 2026-11-06

| File | Why |
|---|---|
| `leather/broadtail.jpg`, `leather/donkali_kid_1.jpg`, `leather/donkali_kid_2.jpg`, `leather/goat.jpg`, `leather/karakul_lamb.jpg` | the owner's choice |
| `leather/calf_spotted.zip`, `leather/cowhide_stretched.zip`, `leather/karakul_lamb.zip` | the owner's choice |
