---
title: Prüfungen speichern und öffnen
order: 6
summary: Eine Zip-Datei bewahrt Foto, Kamera, Kontur und Fläche auf.
---

## Speichern {#save}

Drücken Sie `Speichern`, sobald die Anzeige das Ergebnis zeigt. Wählen Sie einen Ordner und einen Namen; der Name des Fotos wird vorgeschlagen. Während ein Foto analysiert wird, wartet `Speichern` auf das Ergebnis: Die Meldungsanzeige sagt *Warten Sie, bis die Analyse beendet ist.*

Die Datei ist ein gewöhnliches Zip, das jedes Programm lesen kann:

| Datei | Inhalt |
|---|---|
| original.jpg (oder .png, …) | das Foto, Byte für Byte wie importiert |
| mask.png | das Objekt weiß auf schwarz, in der Größe des Fotos |
| camera.json | Abstand, Brennweite und Pixelgröße, die verwendet wurden, und woher jeder Wert kam |
| parameters.json | die Mindestfläche und die Toleranz der Kontur |
| objects.dxf | die Kontur und die Löcher in Millimetern, für CAD und Schneidemaschinen |
| result.json | die Größe des Fotos, der Maßstab und die Fläche in mm² |

## Öffnen {#open}

Drücken Sie `Öffnen` und wählen Sie ein gespeichertes Zip. Foto, Kontur und Fläche kommen genau so zurück, wie sie gespeichert wurden, ohne neue Analyse, mit der gespeicherten Kamera. Die Meldungsanzeige beendet die Zeile mit *gespeichertes Ergebnis*.

Der nächste `Import` kehrt zur Kamera aus `Einrichten` zurück.

::: note
Eine von einer älteren Version von Contorno gespeicherte Prüfung (eine Hautprüfung) öffnet ihr Foto und ihre Kamera und wird mit der KI-Kontur neu analysiert. Die Meldungsanzeige sagt es.
:::

## Ordner {#folders}

`Import`, `Öffnen` und `Speichern` beginnen alle im Ordner der zuletzt in einem von ihnen gewählten Datei.
