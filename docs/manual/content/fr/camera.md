---
title: D'où vient l'échelle
order: 5
summary: Les données de caméra de la photo, les réglages, et la ligne qui dit ce qui a été utilisé.
---

La surface est le nombre de pixels à l'intérieur du contour multiplié par la surface d'un pixel sur la table. La taille de ce pixel, l'**échelle** en mm/px, découle de trois valeurs :

::: formula
échelle = distance à la table × taille du pixel ÷ focale
:::

## La photo d'abord, puis les réglages {#order}

À l'import d'une photo, Contorno Indicator lit les données de caméra qu'une caméra écrit dans ses fichiers JPEG ou TIFF (EXIF) :

- **Focale** — celle de la photo, si elle est comprise entre 0.1 et 2000 mm.
- **Taille du pixel** — d'après la résolution du plan focal de la photo, sinon calculée à partir de sa focale équivalente 35 mm, si le résultat est compris entre 0.1 et 100 µm.
- **Distance** — toujours celle de `Réglages`. La photo ne peut pas savoir à quelle hauteur était la caméra.

Une valeur absente de la photo, ou hors de ces plages, vient de `Réglages` à la place, chacune pour elle-même.

Le nombre de pixels vient toujours de la photo elle-même, une fois redressée. Une photo réduite après la prise de vue est mesurée correctement en mode `Taille du capteur`, car la taille du capteur est répartie sur les pixels que la photo a réellement.

## La ligne de caméra {#line}

Après chaque import, une ligne du panneau de messages dit d'où vient chaque valeur et l'échelle qui en résulte, par exemple :

*Caméra : focale 5.56 mm et taille de pixel 5.23 µm (équivalent 35 mm) de la photo, distance 1500 mm des réglages : 1.411 mm/px.*

Autres formes :

- *aucune donnée de caméra dans la photo, réglages utilisés* — la photo n'a pas de données EXIF utilisables (un PNG, une capture d'écran, une copie retouchée).
- *la focale … de la photo n'est pas valide* — une valeur a été trouvée mais elle est hors plage ; le réglage a été utilisé.
- *la photo est peut-être recadrée* — les côtés de la photo ne sont pas dans le même rapport que ceux du capteur. Si elle a été recadrée, l'échelle tirée de la taille du capteur est fausse : utilisez la photo d'origine.

::: warning
L'équivalent 35 mm est arrondi par le fabricant de la caméra ; une taille de pixel qui en est tirée n'est donc qu'approchée. Pour la meilleure précision, saisissez la vraie taille du capteur ou du pixel dans `Réglages` et utilisez des photos qui portent la résolution du plan focal, ou vérifiez l'échelle avec un objet connu.
:::

## Pixels non carrés {#non-square}

Certains capteurs ont des pixels qui ne sont pas carrés. Indiquez la largeur et la hauteur du pixel en mode `Taille du pixel`, chaîne désactivée. L'échelle a alors deux valeurs, une en travers et une en long de la photo, et la surface est comptée avec les deux.
