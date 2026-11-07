---
title: Mesurer une peau
order: 4
summary: De la peau sur la table à la surface sur l'afficheur.
---

La surface ne vaut que ce que vaut la photo. Quelques minutes consacrées à l'installation donnent chaque jour le même résultat juste.

::: step
**Fixez la caméra au-dessus de la table, visant droit vers le bas.** Le capteur doit être parallèle à la table. Une caméra sur un statif ou fixée au plafond garde la même distance pour chaque peau.
:::

::: step
**Mesurez la distance et saisissez-la une fois.** Mesurez du plateau de la table jusqu'à l'objectif et saisissez-la dans `Réglages` comme `Distance à la table`, avec la `Focale` et le capteur. Voir [Réglages](set-up#camera).
:::

::: step
**Étalez la peau à plat, entière et dans le cadre.** Lissez les plis et les bords roulés : un pli cache de la surface à la caméra. Laissez une bande de table tout autour de la peau. Une table d'une couleur différente de la peau aide. Un tapis ou un tissu sous la peau doit remplir toute la photo.
:::

::: step
**Éclairez uniformément et prenez la photo.** Évitez les ombres fortes le long du bord et les reflets vifs. Gardez le fichier d'origine de la caméra : ses données de caméra aident (voir [D'où vient l'échelle](camera)).
:::

::: step
**Importez la photo.** Appuyez sur `Importer` et choisissez le fichier (JPEG, PNG, BMP ou TIFF). La barre d'activité avance quelques secondes ; puis le contour apparaît à droite et l'afficheur montre la surface.
:::

::: step
**Vérifiez le contour, puis enregistrez.** Zoomez sur les bords : la ligne verte doit suivre le bord de la peau, et non une ombre ou la table. Appuyez sur `Enregistrer` pour garder la mesure.
:::

## Ce qui est mesuré {#what}

- Le **plus grand objet** de la photo. Les choses plus petites sur la table, comme de la poussière, des étiquettes ou un stylo, sont écartées pour que le nombre désigne toujours une seule surface.
- Les **trous** de la peau à travers lesquels on voit la table sont retirés de la surface et tracés comme des anneaux verts à part. Les trous de moins de 100 mm² sont comblés.
- La surface est la **projection à plat** vue d'en haut : un pli ou une ondulation de la peau compte pour la surface qu'il couvre, et non pour le cuir qu'il contient.
- Des objets qui se touchent forment un seul objet.

## Vérifier l'échelle avec un objet connu {#check}

Posez sur la table un objet plat de taille connue, à peu près aussi grand qu'une peau, par exemple un panneau de 1000 × 1000 mm (1.000 m²), photographiez-le comme une peau et importez-le. N'utilisez pas un petit objet : le contour passe environ un pixel à l'extérieur du bord, ce qui pèse davantage sur un petit objet, et un étalonnage sur lui ferait lire chaque peau trop petite. Si l'afficheur diffère, vérifiez d'abord la `Distance à la table` : c'est la valeur la plus souvent fausse.

Pour étalonner, modifiez la `Distance à la table` dans `Réglages` jusqu'à ce que l'objet connu soit lu juste, et gardez cette distance. La surface varie avec le carré de la distance : si l'afficheur est 2 % trop haut, diminuez la distance d'environ 1 %. Cela corrige aussi la petite erreur qui vient de traiter l'objectif comme un sténopé (voir [Référence](reference#accuracy)). Recommencez chaque fois que la caméra, l'objectif ou le zoom change.
