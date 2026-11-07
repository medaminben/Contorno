---
title: Salvare e aprire ispezioni
order: 6
summary: Un file zip conserva la foto, la fotocamera, il contorno e la superficie.
---

## Salvare {#save}

Premete `Salva` non appena il display mostra il risultato. Scegliete una cartella e un nome; viene proposto il nome della foto. Mentre una foto è in analisi, `Salva` attende il risultato: il pannello dei messaggi dice *Attendi la fine dell'analisi.*

Il file è un normale zip che qualsiasi programma può leggere:

| File | Contenuto |
|---|---|
| original.jpg (o .png, …) | la foto, byte per byte come importata |
| mask.png | l'oggetto bianco su nero, alla dimensione della foto |
| camera.json | la distanza, la lunghezza focale e la dimensione del pixel usate, e da dove viene ciascuna |
| parameters.json | l'area minima e la tolleranza del contorno |
| objects.dxf | il contorno e i fori in millimetri, per CAD e macchine da taglio |
| result.json | la dimensione della foto, la scala e la superficie in mm² |

## Aprire {#open}

Premete `Apri` e scegliete uno zip salvato. Foto, contorno e superficie tornano esattamente come sono stati salvati, senza una nuova analisi, con la fotocamera salvata. Il pannello dei messaggi chiude la riga con *risultato salvato*.

Il successivo `Importa` torna alla fotocamera di `Imposta`.

::: note
Un'ispezione salvata da una versione precedente di Contorno (un'ispezione di pelle) apre la sua foto e la sua fotocamera e viene analizzata di nuovo con il contorno IA. Il pannello dei messaggi lo dice.
:::

## Cartelle {#folders}

`Importa`, `Apri` e `Salva` partono tutti dalla cartella dell'ultimo file scelto in uno di essi.
