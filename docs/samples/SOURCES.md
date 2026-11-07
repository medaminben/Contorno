# Where the sample photos come from

Checked on 2026-10-30 from each file's metadata and by an exact-file (SHA-1)
lookup on Wikimedia Commons, or (the Kürschner photos) downloaded from
Commons with their licence read from the API. A visual reverse image search
was not done.

| File | Origin | Licence |
|---|---|---|
| leather/calf_black.jpg | Wikimedia Commons, [File:Kalb schwarzgefaerbt.jpg](https://commons.wikimedia.org/wiki/File:Kalb_schwarzgefaerbt.jpg), author "kuerschner", own work; identical file (SHA-1 935588f9…) | public domain (released worldwide by the author) |
| leather/cowhide_stretched.jpg | Wikimedia Commons, [File:Stretched Cowhide Skin.jpg](https://commons.wikimedia.org/wiki/File:Stretched_Cowhide_Skin.jpg), author "Enda012" (TECNO CN5c, 2026-09-29 13:03:25, 4064 × 3048), resized here to 1600 × 1200; identical in picture (mean difference 2.5 grey levels after resizing) | **CC BY 4.0**: credit the author, the licence and the source, and say it was changed |
| leather/cowhide_marked.png | cowhide_stretched.jpg with an outline drawn on it (derived) | CC BY 4.0, as cowhide_stretched.jpg |
| leather/calf_spotted.jpg | Wikimedia Commons, [File:Calf fur skin, spotted.jpg](https://commons.wikimedia.org/wiki/File:Calf_fur_skin,_spotted.jpg), author "Kürschner", 2008-11-11; resized to ≤ 1600 px, re-encoded, metadata dropped | public domain |
| leather/chekiang_lamb.jpg | Wikimedia Commons, [File:Chekiang lamb fur skin.jpg](https://commons.wikimedia.org/wiki/File:Chekiang_lamb_fur_skin.jpg), author "Kürschner", 2009-06-26; resized to ≤ 1600 px, re-encoded, metadata dropped | public domain |
| leather/indian_lamb.jpg | Wikimedia Commons, [File:Indian lamb fur skin.jpg](https://commons.wikimedia.org/wiki/File:Indian_lamb_fur_skin.jpg), author "Kürschner", 2008-10-19; resized to ≤ 1600 px, re-encoded, metadata dropped | public domain |
| leather/reindeer.jpg | Wikimedia Commons, [File:Reindeer fur skin 2.jpg](https://commons.wikimedia.org/wiki/File:Reindeer_fur_skin_2.jpg), author "Kürschner", 2009-06-01; resized to ≤ 1600 px, re-encoded, metadata dropped | public domain |
| cowhide_stretched.zip, cowhide_analysis_bad_result.zip | Contorno exports derived from leather/cowhide_stretched.jpg (saved while testing, see `image-sources.md` §3.1) | CC BY 4.0, as cowhide_stretched.jpg |
| synthetic/speckled_kraft.jpg, synthetic/speckled_kraft_mask.png | Made by this project: procedural hide photo and its true mask (white = hide), synthesised from noise, no photo pixels; `generate_hides.py` sample 22 | own work, no third-party rights |
| synthetic/tricolour_kraft.jpg, synthetic/tricolour_kraft_mask.png | Made by this project, as above; sample 4 | own work, no third-party rights |

Removed on 2026-10-30: `leather.jpg` (a customer's photo from an Amazon review) and
its derivatives, and `leather_black.jpg` / `leather_black_dim.jpg` (origin unknown).
Removed on 2026-11-05: `leather/roe_deer_leather_side.jpg` (the skin is nailed to a board and
tilted back, not flat, so its area in the photo is not its area).
Removed on 2026-11-06: `leather/broadtail.jpg`, `leather/donkali_kid_1.jpg`, `leather/donkali_kid_2.jpg`,
`leather/goat.jpg`, `leather/karakul_lamb.jpg` and the exports `leather/calf_spotted.zip`,
`leather/cowhide_stretched.zip`, `leather/karakul_lamb.zip` (the owner's choice).
Details, sizes and uses: `docs/samples/image-sources.md`.
