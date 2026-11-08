---
title: Inspecties opslaan en openen
order: 6
summary: Eén zip-bestand bewaart de foto, de camera, de contour en het oppervlak.
---

## Opslaan {#save}

Druk op `Opslaan` zodra de uitlezing het resultaat toont. Kies een map en een naam; de naam van de foto wordt voorgesteld. Terwijl een foto geanalyseerd wordt, wacht `Opslaan` op het resultaat: het meldingenpaneel zegt *Wacht tot de analyse klaar is.*

Het bestand is een gewone zip die elk programma kan lezen:

| Bestand | Inhoud |
|---|---|
| original.jpg (of .png, …) | de foto, byte voor byte zoals geïmporteerd |
| mask.png | het voorwerp wit op zwart, op de grootte van de foto |
| camera.json | de gebruikte afstand, brandpuntsafstand en pixelgrootte, en waar elk vandaan kwam |
| parameters.json | het minimale oppervlak en de tolerantie van de contour |
| objects.dxf | de contour en de gaten in millimeters, voor CAD en snijmachines |
| result.json | de grootte van de foto, de schaal en het oppervlak in mm² |

## Openen {#open}

Druk op `Openen` en kies een opgeslagen zip. Foto, contour en oppervlak komen precies terug zoals ze opgeslagen zijn, zonder nieuwe analyse, met de opgeslagen camera. Het meldingenpaneel sluit de regel af met *opgeslagen resultaat*.

De volgende `Import` keert terug naar de camera van `Instellen`.

::: note
Een inspectie die door een oudere versie van Contorno is opgeslagen (een huidinspectie) opent haar foto en camera en wordt opnieuw geanalyseerd met de AI-contour. Het meldingenpaneel zegt het.
:::

## Mappen {#folders}

`Import`, `Openen` en `Opslaan` beginnen allemaal in de map van het laatste bestand dat in een van hen gekozen is.
