---
title: Enregistrer et ouvrir des inspections
order: 6
summary: Un fichier zip garde la photo, la caméra, le contour et la surface.
---

## Enregistrer {#save}

Appuyez sur `Sauver` dès que l'afficheur montre le résultat. Choisissez un dossier et un nom ; le nom de la photo est proposé. Pendant l'analyse d'une photo, `Sauver` attend le résultat : le panneau de messages indique *Attendez la fin de l'analyse.*

Le fichier est un zip ordinaire que tout programme sait lire :

| Fichier | Contenu |
|---|---|
| original.jpg (ou .png, …) | la photo, octet pour octet telle qu'importée |
| mask.png | l'objet en blanc sur noir, à la taille de la photo |
| camera.json | la distance, la focale et la taille de pixel utilisées, et la provenance de chacune |
| parameters.json | l'aire minimale et la tolérance du contour |
| objects.dxf | le contour et les trous en millimètres, pour la CAO et les machines de découpe |
| result.json | la taille de la photo, l'échelle et la surface en mm² |

## Ouvrir {#open}

Appuyez sur `Ouvrir` et choisissez un zip enregistré. La photo, le contour et la surface reviennent exactement comme ils ont été enregistrés, sans nouvelle analyse, avec la caméra enregistrée. Le panneau de messages termine la ligne par *résultat enregistré*.

Le prochain `Importer` revient à la caméra de `Réglages`.

::: note
Une inspection enregistrée par une ancienne version de Contorno (une inspection de peau) ouvre sa photo et sa caméra et est analysée de nouveau avec le contour IA. Le panneau de messages l'indique.
:::

## Dossiers {#folders}

`Importer`, `Ouvrir` et `Sauver` commencent tous dans le dossier du dernier fichier choisi dans l'un d'eux.
