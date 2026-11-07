---
title: Eine Haut messen
order: 4
summary: Von der Haut auf dem Tisch zur Fläche auf der Anzeige.
---

Die Fläche ist nur so gut wie das Foto. Ein paar Minuten für den Aufbau ergeben jeden Tag dasselbe richtige Ergebnis.

::: step
**Befestigen Sie die Kamera über dem Tisch, senkrecht nach unten blickend.** Der Sensor muss parallel zum Tisch sein. Eine Kamera auf einem Stativ oder an der Decke hält den Abstand für jede Haut gleich.
:::

::: step
**Messen Sie den Abstand und geben Sie ihn einmal ein.** Messen Sie von der Tischplatte bis zum Objektiv der Kamera und geben Sie ihn unter `Einrichten` als `Abstand zum Tisch` ein, mit der `Brennweite` und dem Sensor. Siehe [Einrichten](set-up#camera).
:::

::: step
**Legen Sie die Haut flach, ganz und innerhalb des Bildes aus.** Glätten Sie Falten und eingerollte Ränder: Eine Falte verdeckt Fläche vor der Kamera. Lassen Sie rundum einen Streifen Tisch frei. Ein Tisch, dessen Farbe sich von der Haut unterscheidet, hilft. Eine Matte oder ein Tuch unter der Haut muss das ganze Foto füllen.
:::

::: step
**Beleuchten Sie gleichmäßig und machen Sie das Foto.** Vermeiden Sie starke Schatten am Rand und helle Spiegelungen. Behalten Sie die Originaldatei der Kamera: Ihre Kameradaten helfen (siehe [Woher der Maßstab kommt](camera)).
:::

::: step
**Importieren Sie das Foto.** Drücken Sie `Import` und wählen Sie die Datei (JPEG, PNG, BMP oder TIFF). Der Fortschrittsbalken läuft einige Sekunden; dann erscheint rechts die Kontur und die Anzeige zeigt die Fläche.
:::

::: step
**Prüfen Sie die Kontur, dann speichern Sie.** Zoomen Sie auf die Ränder: Die grüne Linie soll am Rand der Haut verlaufen, nicht an einem Schatten oder am Tisch. Drücken Sie `Speichern`, um die Messung aufzubewahren.
:::

## Was gemessen wird {#what}

- Das **größte Objekt** auf dem Foto. Kleinere Dinge auf dem Tisch, wie Staub, Etiketten oder ein Stift, werden weggelassen, damit die Zahl immer eine Fläche benennt.
- **Löcher** in der Haut, durch die man den Tisch sieht, werden aus der Fläche ausgeschnitten und als eigene grüne Ringe gezeichnet. Löcher unter 100 mm² werden gefüllt.
- Die Fläche ist die **flache Projektion** von oben gesehen: Eine Falte oder Welle in der Haut zählt mit der Fläche, die sie bedeckt, nicht mit dem Leder darin.
- Objekte, die sich berühren, sind ein Objekt.

## Den Maßstab mit einem bekannten Objekt prüfen {#check}

Legen Sie ein flaches Objekt bekannter Größe auf den Tisch, etwa so groß wie eine Haut, zum Beispiel eine Platte von 1000 × 1000 mm (1.000 m²), fotografieren Sie es wie eine Haut und importieren Sie es. Nehmen Sie kein kleines Objekt: Die Kontur liegt etwa ein Pixel außerhalb des Randes, was bei einem kleinen Objekt stärker ins Gewicht fällt, und eine Kalibrierung damit ließe jede Haut zu klein erscheinen. Weicht die Anzeige ab, prüfen Sie zuerst den `Abstand zum Tisch`: Er ist der Wert, der am häufigsten falsch ist.

Zum Kalibrieren ändern Sie den `Abstand zum Tisch` unter `Einrichten`, bis das bekannte Objekt richtig angezeigt wird, und behalten diesen Abstand. Die Fläche ändert sich mit dem Quadrat des Abstands: Ist die Anzeige 2 % zu hoch, verringern Sie den Abstand um etwa 1 %. Das korrigiert auch den kleinen Fehler, das Objektiv als Lochkamera zu behandeln (siehe [Referenz](reference#accuracy)). Wiederholen Sie es, wann immer sich Kamera, Objektiv oder Zoom ändern.
