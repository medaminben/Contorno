---
title: Messaggi e problemi
order: 7
summary: Che cosa dice il pannello dei messaggi e che cosa fare quando il risultato sembra sbagliato.
---

## Righe abituali {#usual}

| Riga | Significato |
|---|---|
| Importato cowhide.jpg, 1600 x 1200 px. | La foto è mostrata; la sua dimensione in pixel. |
| Fotocamera: … | Da dove viene la scala. Vedete [Da dove viene la scala](camera#line). |
| Analisi in corso… | L'analisi è in corso; la barra di attività avanza. |
| 1 oggetto · 1.411 mm/px · 3.2 s | Il risultato: gli oggetti, la scala e il tempo impiegato. |
| Parte della pelle è stata trovata dal tavolo intorno, … | L'IA ha preso solo una parte della pelle, per esempio le macchie scure di una pelle pezzata. Il contorno è stato trovato invece dal tavolo intorno alla pelle. Controllatelo prima di salvare. |
| Nessun oggetto trovato | Niente sulla foto è abbastanza grande. Il display mostra 0.0 cm². |
| Salvato … / Aperto … | Il file scritto o letto. |

## Il contorno IA non è disponibile {#ai}

*Contorno IA non disponibile: modello non trovato* — manca il file del modello IS-Net. Il messaggio elenca i luoghi in cui è stato cercato. In una compilazione dai sorgenti, compilate il target isnet_model; in un'installazione, reinstallate.

*Contorno IA non disponibile: Vision è stato compilato senza LibTorch* — questa copia del programma è stata compilata senza l'IA. Mostra la foto ma non misura: non ripiega mai in silenzio su un metodo meno esatto.

## Il contorno è sbagliato {#outline}

- **Segue un'ombra.** Illuminate il tavolo in modo più uniforme, o dall'alto.
- **Taglia una parte chiara della pelle.** Usate un tavolo il cui colore si distingua di più da quello della pelle.
- **Lascia fuori una grande parte uniforme di una pelle bicolore.** L'IA può scambiare per il tavolo un'area ampia e uniforme, come il bianco di una pelle bovina. Contorno trova allora la pelle dal tavolo intorno, e il pannello dei messaggi lo dice. Per questo il tavolo deve vedersi tutto intorno alla pelle: dove la pelle raggiunge il bordo della foto, resta il contorno dell'IA. Controllate sempre il contorno prima di salvare.
- **Racchiude due pelli.** Due oggetti che si toccano sono un unico oggetto. Lasciate uno spazio fra loro o misurateli uno alla volta.
- **Include il tavolo accanto a un tappetino.** Un tappetino o un telo più piccolo della foto mostra due colori sul bordo della foto, e il tavolo accanto al tappetino può essere preso per pelle. Usate un tappetino o un telo che riempia tutta la foto, oppure nessuno.
- **Un segno o una macchia viene ritagliato come foro.** Solo i fori attraverso cui si vede il tavolo dovrebbero essere ritagliati. Segnalate la foto all'assistenza.

## La superficie è sbagliata {#surface}

- Controllate prima la `Distanza dal tavolo`. Un errore dell'1 % sulla distanza è un errore del 2 % sulla superficie.
- Controllate la `Lunghezza focale`: quella reale, non l'equivalente 35 mm.
- Leggete la riga della fotocamera: prende i valori dalla foto o dalle impostazioni, e sono giusti?
- Verificate con un oggetto di dimensione nota (vedete [Misurare una pelle](measure#check)).

## Il manuale non si apre {#manual}

Il ? cerca questo manuale accanto al programma e lo apre in Chromium o Google Chrome (Linux) o Microsoft Edge (Windows), altrimenti nel browser predefinito. Se nessuno ci riesce, un messaggio indica il file del manuale: apritelo in un browser qualsiasi.
