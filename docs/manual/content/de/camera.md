---
title: Woher der Maßstab kommt
order: 5
summary: Die Kameradaten des Fotos, die Einstellungen und die Zeile, die sagt, was verwendet wurde.
---

Die Fläche ist die Zahl der Pixel innerhalb der Kontur mal die Fläche eines Pixels auf dem Tisch. Die Größe dieses Pixels, der **Maßstab** in mm/px, ergibt sich aus drei Werten:

::: formula
Maßstab = Abstand zum Tisch × Pixelgröße ÷ Brennweite
:::

## Zuerst das Foto, dann die Einstellungen {#order}

Beim Import liest Contorno Indicator die Kameradaten, die eine Kamera in ihre JPEG- oder TIFF-Dateien schreibt (EXIF):

- **Brennweite** — aus dem Foto, wenn es eine zwischen 0.1 und 2000 mm enthält.
- **Pixelgröße** — aus der Brennebenen-Auflösung des Fotos, sonst aus seiner Kleinbild-äquivalenten Brennweite berechnet, wenn das Ergebnis zwischen 0.1 und 100 µm liegt.
- **Abstand** — immer aus `Einrichten`. Das Foto kann nicht wissen, wie hoch die Kamera war.

Ein Wert, den das Foto nicht hat oder außerhalb dieser Bereiche hat, kommt stattdessen aus `Einrichten`, jeder für sich.

Die Pixelzahl kommt immer aus dem Foto selbst, nachdem es aufgerichtet wurde. Ein nach der Aufnahme verkleinertes Foto wird im Modus `Sensorgröße` richtig gemessen, weil die Größe des Sensors auf die Pixel verteilt wird, die das Foto wirklich hat.

## Die Kamerazeile {#line}

Nach jedem Import sagt eine Zeile in der Meldungsanzeige, woher jeder Wert kam und welcher Maßstab sich ergibt, zum Beispiel:

*Kamera: Brennweite 5.56 mm und Pixelgröße 5.23 µm (35 mm-Äquivalent) aus dem Foto, Abstand 1500 mm aus den Einstellungen: 1.411 mm/px.*

Andere Formen:

- *keine Kameradaten im Foto, Einstellungen verwendet* — das Foto hat keine verwendbaren EXIF-Daten (ein PNG, ein Bildschirmfoto, eine bearbeitete Kopie).
- *die Brennweite … des Fotos ist ungültig* — ein Wert wurde gefunden, liegt aber außerhalb des Bereichs; die Einstellung wurde verwendet.
- *das Foto ist möglicherweise beschnitten* — die Seiten des Fotos stehen nicht im selben Verhältnis wie die des Sensors. Wurde es beschnitten, ist der Maßstab aus der Sensorgröße falsch: Verwenden Sie das Originalfoto.

::: warning
Das Kleinbild-Äquivalent wird vom Kamerahersteller gerundet, daher ist eine daraus berechnete Pixelgröße nur ungefähr. Für die beste Genauigkeit geben Sie unter `Einrichten` die echte Sensor- oder Pixelgröße ein und verwenden Fotos mit der Brennebenen-Auflösung in ihren Daten, oder prüfen den Maßstab mit einem bekannten Objekt.
:::

## Nicht quadratische Pixel {#non-square}

Manche Sensoren haben Pixel, die nicht quadratisch sind. Geben Sie im Modus `Pixelgröße` Breite und Höhe des Pixels an, mit ausgeschalteter Kette. Der Maßstab hat dann zwei Werte, einen quer und einen längs des Fotos, und die Fläche wird mit beiden gezählt.
