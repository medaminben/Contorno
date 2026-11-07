---
title: Naslag
order: 8
summary: De formules, de bereiken, de nauwkeurigheid, de bestanden en de instellingen.
---

## Formules {#formulas}

::: formula
schaal dwars (mm/px) = afstand × pixelbreedte ÷ brandpuntsafstand
schaal langs (mm/px) = afstand × pixelhoogte ÷ brandpuntsafstand
pixelgrootte (modus Sensorgrootte) = lange zijde van de sensor ÷ lange zijde van de foto (px)
oppervlak (mm²) = pixels binnen de contour × schaal dwars × schaal langs
:::

De getekende en opgeslagen contour is vereenvoudigd binnen 0.5 mm van de pixelrand; het getoonde oppervlak wordt op de pixels zelf geteld en is dus gelijk aan het wit van het masker.

## Bereiken {#ranges}

| Waarde | Bereik | Standaard |
|---|---|---|
| Afstand tot de tafel | 1–100000 mm | 1500 mm |
| Brandpuntsafstand | 0.1–2000 mm | 13.02 mm |
| Sensorbreedte × -hoogte | elk 0.1–100 mm | 15.04 × 11.28 mm |
| Pixelbreedte × -hoogte | elk 0.1–100 µm | 3.76 × 3.76 µm |
| Kleinste voorwerp of gat dat blijft | instelling objects/min_area_mm2 | 100 mm² |

## Nauwkeurigheid {#accuracy}

Het oppervlak groeit met het kwadraat van de schaal, dus een relatieve fout in de afstand, de brandpuntsafstand of de pixelgrootte telt dubbel:

| Fout in | Fout in het oppervlak |
|---|---|
| afstand +1 % (15 mm bij 1500 mm) | +2 % |
| brandpuntsafstand +1 % | −2 % |
| camera 2° gekanteld, huid in het midden | minder dan 0.1 % |
| camera 2° gekanteld, huid 300 mm uit het midden | ongeveer 1.4 % |
| contour 1 pixel te ver naar buiten, huid van 2 m² bij 1.4 mm/px | ongeveer +0.5 % |
| objectief scherpgesteld op de tafel (13 mm bij 1500 mm) | ongeveer +1.7 % |

De camera wordt als gaatjescamera behandeld. Een echt objectief dat op de tafel is scherpgesteld, maakt de schaal ongeveer brandpuntsafstand ÷ afstand te groot; de controle met een bekend voorwerp (zie [Een huid meten](measure#check)) haalt dat weg, samen met elke fout in de afstand. De vertekening van het objectief wordt niet gecorrigeerd, en het oppervlak is de projectie van de huid op de tafel. Houd de huid in het midden van het beeld, waar de vertekening van de meeste objectieven het kleinst is.

## Het artikel {#article}

Hoe de contour gevonden wordt, hoe het oppervlak uit de camera volgt, hoe beide getest zijn en hoe groot de onzekerheid is: zie het artikel [Measuring the surface of animal hides from a single photograph](../../articles/contorno-area-measurement/contorno-area-measurement) (in het Engels).

## Bestanden {#files}

- Foto's: JPEG, PNG, BMP, TIFF. Cameragegevens worden uit JPEG en TIFF gelezen.
- Inspecties: zip; zie [Inspecties opslaan en openen](inspections#save).
- Instellingen: indicator.ini, in de instellingenmap van het programma (onder Linux ~/.config/Contorno/Indicator/).

## Bewaarde instellingen {#settings}

De camera van `Instellen`, de taal, het thema, de laatste map en het minimale oppervlak blijven tussen sessies bewaard. Het minimale oppervlak heeft geen veld in het venster; het is te wijzigen in indicator.ini (objects/min_area_mm2, een van 0, 10, 50, 100, 500, 1000, 5000, 10000, 50000 mm²).

## Foto's {#photos}

De koeienhuid in de schermafbeeldingen is "Stretched Cowhide Skin" van Enda012, Wikimedia Commons (commons.wikimedia.org/wiki/File:Stretched_Cowhide_Skin.jpg), gelicentieerd onder CC BY 4.0 (creativecommons.org/licenses/by/4.0/). Ze is verkleind en in Contorno Indicator met haar contour getoond.
