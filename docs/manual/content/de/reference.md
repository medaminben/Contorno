---
title: Referenz
order: 8
summary: Die Formeln, die Bereiche, die Genauigkeit, die Dateien und die Einstellungen.
---

## Formeln {#formulas}

::: formula
Maßstab quer (mm/px) = Abstand × Pixelbreite ÷ Brennweite
Maßstab längs (mm/px) = Abstand × Pixelhöhe ÷ Brennweite
Pixelgröße (Modus Sensorgröße) = lange Seite des Sensors ÷ lange Seite des Fotos (px)
Fläche (mm²) = Pixel innerhalb der Kontur × Maßstab quer × Maßstab längs
:::

Die gezeichnete und gespeicherte Kontur ist innerhalb von 0.5 mm um den Pixelrand vereinfacht; die angezeigte Fläche wird auf den Pixeln selbst gezählt und gleicht daher dem Weiß der Maske.

## Bereiche {#ranges}

| Wert | Bereich | Vorgabe |
|---|---|---|
| Abstand zum Tisch | 1–100000 mm | 1500 mm |
| Brennweite | 0.1–2000 mm | 13.02 mm |
| Sensorbreite × -höhe | je 0.1–100 mm | 15.04 × 11.28 mm |
| Pixelbreite × -höhe | je 0.1–100 µm | 3.76 × 3.76 µm |
| Kleinstes behaltenes Objekt oder Loch | Einstellung objects/min_area_mm2 | 100 mm² |

## Genauigkeit {#accuracy}

Die Fläche wächst mit dem Quadrat des Maßstabs, daher zählt ein relativer Fehler im Abstand, in der Brennweite oder in der Pixelgröße doppelt:

| Fehler in | Fehler in der Fläche |
|---|---|
| Abstand +1 % (15 mm bei 1500 mm) | +2 % |
| Brennweite +1 % | −2 % |
| Kamera um 2° geneigt, Haut mittig | unter 0.1 % |
| Kamera um 2° geneigt, Haut 300 mm außermittig | etwa 1.4 % |
| Kontur 1 Pixel zu weit außen, Haut von 2 m² bei 1.4 mm/px | etwa +0.5 % |
| Objektiv auf den Tisch fokussiert (13-mm-Objektiv bei 1500 mm) | etwa +1.7 % |

Die Kamera wird als Lochkamera behandelt. Ein echtes, auf den Tisch fokussiertes Objektiv macht den Maßstab um etwa Brennweite ÷ Abstand zu groß; die Prüfung mit einem bekannten Objekt (siehe [Eine Haut messen](measure#check)) beseitigt das zusammen mit jedem Fehler im Abstand. Die Verzeichnung des Objektivs wird nicht korrigiert, und die Fläche ist die Projektion der Haut auf den Tisch. Halten Sie die Haut in der Bildmitte, wo die Verzeichnung der meisten Objektive am kleinsten ist.

## Der Artikel {#article}

Wie die Kontur gefunden wird, wie die Fläche aus der Kamera folgt, wie beides geprüft wurde und wie groß die Unsicherheit ist: siehe den Artikel [Measuring the surface of animal hides from a single photograph](../../articles/contorno-area-measurement/contorno-area-measurement) (auf Englisch).

## Dateien {#files}

- Fotos: JPEG, PNG, BMP, TIFF. Kameradaten werden aus JPEG und TIFF gelesen.
- Prüfungen: Zip; siehe [Prüfungen speichern und öffnen](inspections#save).
- Einstellungen: indicator.ini im Einstellungsordner des Programms (unter Linux ~/.config/Contorno/Indicator/).

## Gespeicherte Einstellungen {#settings}

Die Kamera aus `Einrichten`, die Sprache, das Design, der letzte Ordner und die Mindestfläche bleiben zwischen den Sitzungen erhalten. Die Mindestfläche hat kein Feld im Fenster; sie lässt sich in indicator.ini ändern (objects/min_area_mm2, einer der Werte 0, 10, 50, 100, 500, 1000, 5000, 10000, 50000 mm²).

## Fotos {#photos}

Die Kuhhaut in den Bildschirmfotos ist „Stretched Cowhide Skin“ von Enda012, Wikimedia Commons (commons.wikimedia.org/wiki/File:Stretched_Cowhide_Skin.jpg), lizenziert unter CC BY 4.0 (creativecommons.org/licenses/by/4.0/). Sie wurde verkleinert und in Contorno Indicator mit ihrer Kontur gezeigt.
