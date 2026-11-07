---
title: Da dove viene la scala
order: 5
summary: I dati della fotocamera nella foto, le impostazioni e la riga che dice che cosa è stato usato.
---

La superficie è il numero di pixel dentro il contorno moltiplicato per la superficie di un pixel sul tavolo. La dimensione di quel pixel, la **scala** in mm/px, deriva da tre valori:

::: formula
scala = distanza dal tavolo × dimensione del pixel ÷ lunghezza focale
:::

## Prima la foto, poi le impostazioni {#order}

Quando si importa una foto, Contorno Indicator legge i dati che una fotocamera scrive nei suoi file JPEG o TIFF (EXIF):

- **Lunghezza focale** — quella della foto, se è fra 0.1 e 2000 mm.
- **Dimensione del pixel** — dalla risoluzione del piano focale della foto, altrimenti calcolata dalla sua lunghezza focale equivalente 35 mm, se il risultato è fra 0.1 e 100 µm.
- **Distanza** — sempre da `Imposta`. La foto non può sapere a che altezza era la fotocamera.

Un valore che la foto non ha, o ha fuori da questi intervalli, viene preso invece da `Imposta`, ciascuno per conto suo.

Il numero di pixel viene sempre dalla foto stessa, dopo averla raddrizzata. Una foto rimpicciolita dopo lo scatto è misurata correttamente in modalità `Dimensione del sensore`, perché la dimensione del sensore viene ripartita sui pixel che la foto ha davvero.

## La riga della fotocamera {#line}

Dopo ogni importazione una riga del pannello dei messaggi dice da dove viene ciascun valore e la scala che ne risulta, per esempio:

*Fotocamera: lunghezza focale 5.56 mm e dimensione del pixel 5.23 µm (equivalente a 35 mm) dalla foto, distanza 1500 mm dalle impostazioni: 1.411 mm/px.*

Altre forme:

- *nessun dato della fotocamera nella foto, impostazioni usate* — la foto non ha dati EXIF utilizzabili (un PNG, uno screenshot, una copia ritoccata).
- *la lunghezza focale … della foto non è valida* — è stato trovato un valore fuori intervallo; è stata usata l'impostazione.
- *la foto potrebbe essere ritagliata* — i lati della foto non hanno lo stesso rapporto di quelli del sensore. Se è stata ritagliata, la scala ricavata dalla dimensione del sensore è sbagliata: usate la foto originale.

::: warning
L'equivalente 35 mm è arrotondato dal produttore della fotocamera, quindi una dimensione del pixel ricavata da esso è approssimata. Per la migliore precisione inserite la vera dimensione del sensore o del pixel in `Imposta` e usate foto con la risoluzione del piano focale nei loro dati, oppure verificate la scala con un oggetto noto.
:::

## Pixel non quadrati {#non-square}

Alcuni sensori hanno pixel non quadrati. Indicate la larghezza e l'altezza del pixel in modalità `Dimensione del pixel`, con la catena disattivata. La scala ha allora due valori, uno in larghezza e uno in altezza della foto, e la superficie viene contata con entrambi.
