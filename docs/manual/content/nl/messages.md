---
title: Meldingen en problemen
order: 7
summary: Wat het meldingenpaneel zegt en wat u doet als het resultaat er fout uitziet.
---

## Gewone regels {#usual}

| Regel | Betekenis |
|---|---|
| cowhide.jpg geïmporteerd, 1600 x 1200 px. | De foto wordt getoond; zijn grootte in pixels. |
| Camera: … | Waar de schaal vandaan komt. Zie [Waar de schaal vandaan komt](camera#line). |
| Analyseren… | De analyse loopt; de voortgangsbalk beweegt. |
| 1 voorwerp · 1.411 mm/px · 3.2 s | Het resultaat: de voorwerpen, de schaal en de benodigde tijd. |
| Een deel van de huid is gevonden vanuit de tafel eromheen, … | De AI nam maar een deel van de huid, bijvoorbeeld de donkere vlekken van een gevlekte huid. De contour is in plaats daarvan gevonden vanuit de tafel rond de huid. Controleer hem vóór het opslaan. |
| Geen voorwerp gevonden | Niets op de foto is groot genoeg. De uitlezing toont 0.0 cm². |
| … opgeslagen / … geopend | Het geschreven of gelezen bestand. |

## De AI-contour is niet beschikbaar {#ai}

*AI-contour niet beschikbaar: model niet gevonden* — het bestand van het IS-Net-model ontbreekt. De melding noemt de plaatsen waar gezocht is. In een build uit de broncode bouwt u het doel isnet_model; in een installatie installeert u opnieuw.

*AI-contour niet beschikbaar: Vision is zonder LibTorch gebouwd* — deze kopie van het programma is zonder de AI gebouwd. Ze toont de foto maar meet niet: ze valt nooit stil terug op een minder nauwkeurige methode.

## De contour is fout {#outline}

- **Hij volgt een schaduw.** Belicht de tafel gelijkmatiger, of van boven.
- **Hij snijdt in een licht deel van de huid.** Gebruik een tafel waarvan de kleur meer van de huid verschilt.
- **Hij laat een groot effen deel van een tweekleurige huid weg.** De AI kan een groot egaal vlak, zoals het wit van een koeienhuid, voor de tafel aanzien. Contorno vindt de huid dan vanuit de tafel eromheen, en het meldingenpaneel zegt dat. Daarvoor moet rondom de huid tafel te zien zijn: waar de huid de rand van de foto raakt, blijft de contour van de AI staan. Controleer de contour altijd vóór het opslaan.
- **Hij omvat twee huiden.** Twee voorwerpen die elkaar raken, zijn één voorwerp. Laat ruimte tussen ze, of meet ze een voor een.
- **Hij neemt de tafel naast een mat mee.** Een mat of doek die kleiner is dan de foto toont twee kleuren aan de rand van de foto, en de tafel naast de mat kan voor huid worden aangezien. Gebruik een mat of doek die de hele foto vult, of geen.
- **Een merkteken of vlek wordt als gat uitgesneden.** Alleen gaten waardoor de tafel te zien is, horen uitgesneden te worden. Meld de foto bij de ondersteuning.

## Het oppervlak is fout {#surface}

- Controleer eerst de `Afstand tot de tafel`. Een fout van 1 % in de afstand is een fout van 2 % in het oppervlak.
- Controleer de `Brandpuntsafstand`: de echte, niet het kleinbeeldequivalent.
- Lees de cameraregel: neemt hij de waarden uit de foto of uit de instellingen, en kloppen ze?
- Controleer met een voorwerp van bekende grootte (zie [Een huid meten](measure#check)).

## De handleiding opent niet {#manual}

Het ? zoekt deze handleiding naast het programma en opent hem in Chromium of Google Chrome (Linux) of Microsoft Edge (Windows), anders in de standaardbrowser. Lukt dat geen enkele, dan noemt een melding het bestand van de handleiding: open het in een willekeurige browser.
