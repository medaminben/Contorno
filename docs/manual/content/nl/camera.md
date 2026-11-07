---
title: Waar de schaal vandaan komt
order: 5
summary: De cameragegevens van de foto, de instellingen en de regel die zegt wat gebruikt is.
---

Het oppervlak is het aantal pixels binnen de contour maal het oppervlak van één pixel op de tafel. De grootte van die pixel, de **schaal** in mm/px, volgt uit drie waarden:

::: formula
schaal = afstand tot de tafel × pixelgrootte ÷ brandpuntsafstand
:::

## Eerst de foto, dan de instellingen {#order}

Bij het importeren leest Contorno Indicator de cameragegevens die een camera in haar JPEG- of TIFF-bestanden schrijft (EXIF):

- **Brandpuntsafstand** — uit de foto, als die er een tussen 0.1 en 2000 mm heeft.
- **Pixelgrootte** — uit de brandvlakresolutie van de foto, anders berekend uit zijn kleinbeeldequivalente brandpuntsafstand, als het resultaat tussen 0.1 en 100 µm ligt.
- **Afstand** — altijd uit `Instellen`. De foto kan niet weten hoe hoog de camera hing.

Een waarde die de foto niet heeft, of buiten deze bereiken heeft, komt in plaats daarvan uit `Instellen`, elk op zich.

Het aantal pixels komt altijd uit de foto zelf, nadat hij rechtop is gezet. Een foto die na het nemen verkleind is, wordt in de modus `Sensorgrootte` juist gemeten, omdat de grootte van de sensor verdeeld wordt over de pixels die de foto werkelijk heeft.

## De cameraregel {#line}

Na elke import zegt een regel in het meldingenpaneel waar elke waarde vandaan kwam en welke schaal dat geeft, bijvoorbeeld:

*Camera: brandpuntsafstand 5.56 mm en pixelgrootte 5.23 µm (35 mm-equivalent) uit de foto, afstand 1500 mm uit de instellingen: 1.411 mm/px.*

Andere vormen:

- *geen cameragegevens in de foto, instellingen gebruikt* — de foto heeft geen bruikbare EXIF-gegevens (een PNG, een schermafbeelding, een bewerkte kopie).
- *de brandpuntsafstand … van de foto is ongeldig* — er is een waarde gevonden maar die ligt buiten het bereik; de instelling is gebruikt.
- *de foto is mogelijk bijgesneden* — de zijden van de foto staan niet in dezelfde verhouding als die van de sensor. Is hij bijgesneden, dan is de schaal uit de sensorgrootte fout: gebruik de originele foto.

::: warning
Het kleinbeeldequivalent wordt door de camerafabrikant afgerond, dus een daaruit berekende pixelgrootte is bij benadering. Voor de beste nauwkeurigheid voert u de echte sensor- of pixelgrootte in onder `Instellen` en gebruikt u foto's met de brandvlakresolutie in hun gegevens, of controleert u de schaal met een bekend voorwerp.
:::

## Niet-vierkante pixels {#non-square}

Sommige sensoren hebben pixels die niet vierkant zijn. Geef in de modus `Pixelgrootte` de breedte en hoogte van de pixel op, met de ketting uit. De schaal heeft dan twee waarden, één dwars en één langs de foto, en het oppervlak wordt met beide geteld.
