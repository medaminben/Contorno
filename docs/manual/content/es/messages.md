---
title: Mensajes y problemas
order: 7
summary: Qué dice el panel de mensajes y qué hacer cuando el resultado parece incorrecto.
---

## Líneas habituales {#usual}

| Línea | Significado |
|---|---|
| Importado cowhide.jpg, 1600 x 1200 px. | La foto se muestra; su tamaño en píxeles. |
| Cámara: … | De dónde sale la escala. Vea [De dónde sale la escala](camera#line). |
| Analizando… | El análisis está en marcha; la barra de actividad avanza. |
| 1 objeto · 1.411 mm/px · 3.2 s | El resultado: los objetos, la escala y el tiempo empleado. |
| Parte de la piel se encontró a partir de la mesa que la rodea, … | La IA tomó solo una parte de la piel, por ejemplo las manchas oscuras de una piel moteada. El contorno se encontró en cambio a partir de la mesa que rodea la piel. Compruébelo antes de guardar. |
| No se encontró ningún objeto | Nada en la foto es lo bastante grande. El visor muestra 0.0 cm². |
| Guardado … / Abierto … | El archivo escrito o leído. |

## El contorno de IA no está disponible {#ai}

*Contorno de IA no disponible: modelo no encontrado* — falta el archivo del modelo IS-Net. El mensaje enumera los lugares donde se buscó. En una compilación desde las fuentes, compile el objetivo isnet_model; en una instalación, vuelva a instalar.

*Contorno de IA no disponible: Vision se compiló sin LibTorch* — esta copia del programa se compiló sin la IA. Muestra la foto pero no mide: nunca recurre en silencio a un método menos exacto.

## El contorno está mal {#outline}

- **Sigue una sombra.** Ilumine la mesa de forma más uniforme, o desde arriba.
- **Corta una parte clara de la piel.** Use una mesa cuyo color se distinga más del de la piel.
- **Deja fuera una gran parte lisa de una piel de dos colores.** La IA puede tomar una zona amplia y uniforme, como el blanco de una piel de vaca, por la mesa. Contorno encuentra entonces la piel a partir de la mesa que la rodea, y el panel de mensajes lo dice. Para ello debe verse mesa alrededor de toda la piel: donde la piel llega al borde de la foto, se mantiene el contorno de la IA. Compruebe siempre el contorno antes de guardar.
- **Abarca dos pieles.** Dos objetos que se tocan son un solo objeto. Deje un espacio entre ellos o mídalos uno a uno.
- **Incluye la mesa junto a una alfombrilla.** Una alfombrilla o una tela más pequeña que la foto muestra dos colores en el borde de la foto, y la mesa junto a la alfombrilla puede tomarse por piel. Use una alfombrilla o una tela que llene toda la foto, o ninguna.
- **Una marca o una mancha se recorta como agujero.** Solo deberían recortarse los agujeros a través de los que se ve la mesa. Envíe la foto al soporte.

## La superficie está mal {#surface}

- Revise primero la `Distancia a la mesa`. Un error del 1 % en la distancia es un error del 2 % en la superficie.
- Revise la `Distancia focal`: la real, no el equivalente de 35 mm.
- Lea la línea de cámara: ¿toma los valores de la foto o de los ajustes, y son correctos?
- Compruebe con un objeto de tamaño conocido (vea [Medir una piel](measure#check)).

## El manual no se abre {#manual}

El ? busca este manual junto al programa y lo abre en Chromium o Google Chrome (Linux) o Microsoft Edge (Windows), o si no en el navegador predeterminado. Si ninguno puede, un mensaje indica el archivo del manual: ábralo en cualquier navegador.
