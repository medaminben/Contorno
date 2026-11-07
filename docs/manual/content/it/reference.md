---
title: Riferimento
order: 8
summary: Le formule, gli intervalli, la precisione, i file e le impostazioni.
---

## Formule {#formulas}

::: formula
scala in larghezza (mm/px) = distanza × larghezza del pixel ÷ lunghezza focale
scala in altezza (mm/px) = distanza × altezza del pixel ÷ lunghezza focale
dimensione del pixel (modalità Dimensione del sensore) = lato lungo del sensore ÷ lato lungo della foto (px)
superficie (mm²) = pixel dentro il contorno × scala in larghezza × scala in altezza
:::

Il contorno disegnato e salvato è semplificato entro 0.5 mm dal bordo dei pixel; la superficie mostrata è contata sui pixel stessi, quindi è uguale al bianco della maschera.

## Intervalli {#ranges}

| Valore | Intervallo | Predefinito |
|---|---|---|
| Distanza dal tavolo | 1–100000 mm | 1500 mm |
| Lunghezza focale | 0.1–2000 mm | 13.02 mm |
| Larghezza × altezza del sensore | 0.1–100 mm ciascuna | 15.04 × 11.28 mm |
| Larghezza × altezza del pixel | 0.1–100 µm ciascuna | 3.76 × 3.76 µm |
| Oggetto o foro più piccolo conservato | impostazione objects/min_area_mm2 | 100 mm² |

## Precisione {#accuracy}

La superficie cresce con il quadrato della scala, quindi un errore relativo sulla distanza, sulla lunghezza focale o sulla dimensione del pixel conta doppio:

| Errore su | Errore sulla superficie |
|---|---|
| distanza +1 % (15 mm a 1500 mm) | +2 % |
| lunghezza focale +1 % | −2 % |
| fotocamera inclinata di 2°, pelle al centro | meno dello 0.1 % |
| fotocamera inclinata di 2°, pelle a 300 mm dal centro | circa 1.4 % |
| contorno 1 pixel troppo fuori, pelle di 2 m² a 1.4 mm/px | circa +0.5 % |
| obiettivo messo a fuoco sul tavolo (13 mm a 1500 mm) | circa +1.7 % |

La fotocamera è trattata come un foro stenopeico. Un obiettivo reale messo a fuoco sul tavolo rende la scala troppo grande di circa lunghezza focale ÷ distanza; la verifica con un oggetto noto (vedete [Misurare una pelle](measure#check)) elimina questo errore insieme a qualsiasi errore sulla distanza. La distorsione dell'obiettivo non è corretta, e la superficie è la proiezione della pelle sul tavolo. Tenete la pelle al centro dell'immagine, dove la distorsione della maggior parte degli obiettivi è minima.

## L'articolo {#article}

Come si trova il contorno, come la superficie deriva dalla fotocamera, come sono stati verificati entrambi e quanto è grande l'incertezza: vedete l'articolo [Measuring the surface of animal hides from a single photograph](../../articles/contorno-area-measurement/contorno-area-measurement) (in inglese).

## File {#files}

- Foto: JPEG, PNG, BMP, TIFF. I dati della fotocamera si leggono da JPEG e TIFF.
- Ispezioni: zip; vedete [Salvare e aprire ispezioni](inspections#save).
- Impostazioni: indicator.ini, nella cartella di configurazione dell'applicazione (su Linux ~/.config/Contorno/Indicator/).

## Impostazioni conservate {#settings}

La fotocamera di `Imposta`, la lingua, il tema, l'ultima cartella e l'area minima si conservano fra una sessione e l'altra. L'area minima non ha un campo nella finestra; si cambia in indicator.ini (objects/min_area_mm2, uno fra 0, 10, 50, 100, 500, 1000, 5000, 10000, 50000 mm²).

## Foto {#photos}

La pelle bovina degli screenshot è «Stretched Cowhide Skin» di Enda012, Wikimedia Commons (commons.wikimedia.org/wiki/File:Stretched_Cowhide_Skin.jpg), con licenza CC BY 4.0 (creativecommons.org/licenses/by/4.0/). È stata ridotta di dimensioni e mostrata in Contorno Indicator con il suo contorno.
