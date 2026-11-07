---
title: Référence
order: 8
summary: Les formules, les plages, la précision, les fichiers et les réglages.
---

## Formules {#formulas}

::: formula
échelle en travers (mm/px) = distance × largeur du pixel ÷ focale
échelle en long (mm/px) = distance × hauteur du pixel ÷ focale
taille du pixel (mode Taille du capteur) = grand côté du capteur ÷ grand côté de la photo (px)
surface (mm²) = pixels dans le contour × échelle en travers × échelle en long
:::

Le contour tracé et enregistré est simplifié à 0.5 mm près du bord des pixels ; la surface affichée est comptée sur les pixels eux-mêmes, elle est donc égale au blanc du masque.

## Plages {#ranges}

| Valeur | Plage | Par défaut |
|---|---|---|
| Distance à la table | 1–100000 mm | 1500 mm |
| Focale | 0.1–2000 mm | 13.02 mm |
| Largeur × hauteur du capteur | 0.1–100 mm chacune | 15.04 × 11.28 mm |
| Largeur × hauteur du pixel | 0.1–100 µm chacune | 3.76 × 3.76 µm |
| Plus petit objet ou trou gardé | réglage objects/min_area_mm2 | 100 mm² |

## Précision {#accuracy}

La surface croît avec le carré de l'échelle : une erreur relative sur la distance, la focale ou la taille du pixel compte double.

| Erreur sur | Erreur sur la surface |
|---|---|
| distance +1 % (15 mm à 1500 mm) | +2 % |
| focale +1 % | −2 % |
| caméra inclinée de 2°, peau centrée | moins de 0.1 % |
| caméra inclinée de 2°, peau décalée de 300 mm | environ 1.4 % |
| contour 1 pixel trop à l'extérieur, peau de 2 m² à 1.4 mm/px | environ +0.5 % |
| objectif mis au point sur la table (13 mm à 1500 mm) | environ +1.7 % |

La caméra est traitée comme un sténopé. Un vrai objectif mis au point sur la table rend l'échelle trop grande d'environ focale ÷ distance ; la vérification avec un objet connu (voir [Mesurer une peau](measure#check)) supprime cet écart en même temps que toute erreur sur la distance. La distorsion de l'objectif n'est pas corrigée, et la surface est la projection de la peau sur la table. Gardez la peau au centre de l'image, là où la distorsion de la plupart des objectifs est la plus faible.

## L'article {#article}

Comment le contour est trouvé, comment la surface découle de la caméra, comment les deux ont été testés et quelle est l'incertitude : voir l'article [Measuring the surface of animal hides from a single photograph](../../articles/contorno-area-measurement/contorno-area-measurement) (en anglais).

## Fichiers {#files}

- Photos : JPEG, PNG, BMP, TIFF. Les données de caméra sont lues dans les JPEG et les TIFF.
- Inspections : zip ; voir [Enregistrer et ouvrir des inspections](inspections#save).
- Réglages : indicator.ini, dans le dossier de configuration de l'application (sous Linux ~/.config/Contorno/Indicator/).

## Réglages conservés {#settings}

La caméra de `Réglages`, la langue, le thème, le dernier dossier et l'aire minimale sont conservés d'une session à l'autre. L'aire minimale n'a pas de champ dans la fenêtre ; elle se modifie dans indicator.ini (objects/min_area_mm2, l'une des valeurs 0, 10, 50, 100, 500, 1000, 5000, 10000, 50000 mm²).

## Photos {#photos}

La peau de vache des captures d'écran est « Stretched Cowhide Skin » de Enda012, Wikimedia Commons (commons.wikimedia.org/wiki/File:Stretched_Cowhide_Skin.jpg), sous licence CC BY 4.0 (creativecommons.org/licenses/by/4.0/). Elle a été réduite et montrée dans Contorno Indicator avec son contour.
