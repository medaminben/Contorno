---
title: De dónde sale la escala
order: 5
summary: Los datos de cámara de la foto, los ajustes y la línea que dice qué se usó.
---

La superficie es el número de píxeles dentro del contorno por la superficie de un píxel sobre la mesa. El tamaño de ese píxel, la **escala** en mm/px, sale de tres valores:

::: formula
escala = distancia a la mesa × tamaño del píxel ÷ distancia focal
:::

## Primero la foto, luego los ajustes {#order}

Al importar una foto, Contorno Indicator lee los datos de cámara que una cámara escribe en sus archivos JPEG o TIFF (EXIF):

- **Distancia focal** — la de la foto, si está entre 0.1 y 2000 mm.
- **Tamaño del píxel** — a partir de la resolución del plano focal de la foto, o si no calculado con su distancia focal equivalente de 35 mm, si el resultado está entre 0.1 y 100 µm.
- **Distancia** — siempre la de `Ajustes`. La foto no puede saber a qué altura estaba la cámara.

Un valor que la foto no tiene, o que tiene fuera de estos rangos, se toma en su lugar de `Ajustes`, cada uno por separado.

El número de píxeles sale siempre de la propia foto, una vez enderezada. Una foto reducida después de tomarla se mide bien en el modo `Tamaño del sensor`, porque el tamaño del sensor se reparte entre los píxeles que la foto tiene de verdad.

## La línea de cámara {#line}

Tras cada importación, una línea del panel de mensajes dice de dónde salió cada valor y la escala resultante, por ejemplo:

*Cámara: distancia focal 5.56 mm y tamaño de píxel 5.23 µm (equivalente a 35 mm) de la foto, distancia 1500 mm de los ajustes: 1.411 mm/px.*

Otras formas:

- *sin datos de cámara en la foto, se usan los ajustes* — la foto no tiene datos EXIF utilizables (un PNG, una captura de pantalla, una copia editada).
- *la distancia focal … de la foto no es válida* — se encontró un valor pero está fuera de rango; se usó el ajuste.
- *puede que la foto esté recortada* — los lados de la foto no guardan la misma proporción que los del sensor. Si se recortó, la escala obtenida del tamaño del sensor es errónea: use la foto original.

::: warning
El equivalente de 35 mm lo redondea el fabricante de la cámara, así que un tamaño de píxel calculado con él es aproximado. Para la mejor precisión, introduzca el tamaño real del sensor o del píxel en `Ajustes` y use fotos con la resolución del plano focal en sus datos, o compruebe la escala con un objeto conocido.
:::

## Píxeles no cuadrados {#non-square}

Algunos sensores tienen píxeles que no son cuadrados. Indique el ancho y el alto del píxel en el modo `Tamaño del píxel`, con la cadena desactivada. La escala tiene entonces dos valores, uno a lo ancho y otro a lo alto de la foto, y la superficie se cuenta con ambos.
