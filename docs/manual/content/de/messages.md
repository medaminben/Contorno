---
title: Meldungen und Probleme
order: 7
summary: Was die Meldungsanzeige sagt und was zu tun ist, wenn das Ergebnis falsch aussieht.
---

## Übliche Zeilen {#usual}

| Zeile | Bedeutung |
|---|---|
| cowhide.jpg importiert, 1600 x 1200 px. | Das Foto wird gezeigt; seine Größe in Pixeln. |
| Kamera: … | Woher der Maßstab kommt. Siehe [Woher der Maßstab kommt](camera#line). |
| Analyse läuft… | Die Analyse läuft; der Fortschrittsbalken bewegt sich. |
| 1 Objekt · 1.411 mm/px · 3.2 s | Das Ergebnis: die Objekte, der Maßstab und die benötigte Zeit. |
| Ein Teil der Haut wurde über den Tisch um sie herum gefunden, … | Die KI hat nur einen Teil der Haut erkannt, etwa die dunklen Flecken einer gefleckten Haut. Die Kontur wurde stattdessen über den Tisch um die Haut gefunden. Prüfen Sie sie vor dem Speichern. |
| Kein Objekt gefunden | Nichts auf dem Foto ist groß genug. Die Anzeige zeigt 0.0 cm². |
| … gespeichert / … geöffnet | Die geschriebene oder gelesene Datei. |

## Die KI-Kontur ist nicht verfügbar {#ai}

*KI-Kontur nicht verfügbar: Modell nicht gefunden* — die Datei des IS-Net-Modells fehlt. Die Meldung nennt die durchsuchten Orte. In einem Build aus den Quellen bauen Sie das Ziel isnet_model; in einer Installation installieren Sie neu.

*KI-Kontur nicht verfügbar: Vision wurde ohne LibTorch gebaut* — diese Kopie des Programms wurde ohne die KI gebaut. Sie zeigt das Foto, misst aber nicht: Sie greift nie stillschweigend auf eine ungenauere Methode zurück.

## Die Kontur ist falsch {#outline}

- **Sie folgt einem Schatten.** Beleuchten Sie den Tisch gleichmäßiger oder von oben.
- **Sie schneidet in einen hellen Teil der Haut.** Verwenden Sie einen Tisch, dessen Farbe sich stärker von der Haut unterscheidet.
- **Sie lässt einen großen einfarbigen Teil einer zweifarbigen Haut aus.** Die KI kann eine große gleichmäßige Fläche, etwa das Weiß einer Kuhhaut, für den Tisch halten. Contorno findet die Haut dann über den Tisch um sie herum, und die Meldungsanzeige sagt es. Dazu muss rundum Tisch zu sehen sein: Wo die Haut den Bildrand erreicht, bleibt die Kontur der KI. Prüfen Sie die Kontur immer vor dem Speichern.
- **Sie umfasst zwei Häute.** Zwei Objekte, die sich berühren, sind ein Objekt. Lassen Sie einen Abstand zwischen ihnen oder messen Sie eins nach dem anderen.
- **Sie nimmt den Tisch neben einer Matte mit.** Eine Matte oder ein Tuch, kleiner als das Foto, zeigt am Bildrand zwei Farben, und der Tisch neben der Matte kann für Haut gehalten werden. Verwenden Sie eine Matte oder ein Tuch, das das ganze Foto füllt, oder keines.
- **Ein Zeichen oder Fleck wird als Loch ausgeschnitten.** Nur Löcher, durch die man den Tisch sieht, sollten ausgeschnitten werden. Melden Sie das Foto dem Support.

## Die Fläche ist falsch {#surface}

- Prüfen Sie zuerst den `Abstand zum Tisch`. Ein Fehler von 1 % im Abstand ist ein Fehler von 2 % in der Fläche.
- Prüfen Sie die `Brennweite`: die echte, nicht das Kleinbild-Äquivalent.
- Lesen Sie die Kamerazeile: Nimmt sie die Werte aus dem Foto oder aus den Einstellungen, und stimmen sie?
- Prüfen Sie mit einem Objekt bekannter Größe (siehe [Eine Haut messen](measure#check)).

## Das Handbuch öffnet sich nicht {#manual}

Das ? sucht dieses Handbuch neben dem Programm und öffnet es in Chromium oder Google Chrome (Linux) oder Microsoft Edge (Windows), sonst im Standardbrowser. Gelingt das keinem, nennt eine Meldung die Datei des Handbuchs: Öffnen Sie sie in einem beliebigen Browser.
