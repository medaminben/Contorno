---
title: Referencia
order: 8
summary: Las fórmulas, los rangos, la precisión, los archivos y los ajustes.
---

## Fórmulas {#formulas}

::: formula
escala a lo ancho (mm/px) = distancia × ancho del píxel ÷ distancia focal
escala a lo alto (mm/px) = distancia × alto del píxel ÷ distancia focal
tamaño del píxel (modo Tamaño del sensor) = lado largo del sensor ÷ lado largo de la foto (px)
superficie (mm²) = píxeles dentro del contorno × escala a lo ancho × escala a lo alto
:::

El contorno dibujado y guardado se simplifica dentro de 0.5 mm del borde de los píxeles; la superficie mostrada se cuenta sobre los propios píxeles, así que es igual al blanco de la máscara.

## Rangos {#ranges}

| Valor | Rango | Por defecto |
|---|---|---|
| Distancia a la mesa | 1–100000 mm | 1500 mm |
| Distancia focal | 0.1–2000 mm | 13.02 mm |
| Ancho × alto del sensor | 0.1–100 mm cada uno | 15.04 × 11.28 mm |
| Ancho × alto del píxel | 0.1–100 µm cada uno | 3.76 × 3.76 µm |
| Objeto o agujero más pequeño que se conserva | ajuste objects/min_area_mm2 | 100 mm² |

## Precisión {#accuracy}

La superficie crece con el cuadrado de la escala, así que un error relativo en la distancia, la distancia focal o el tamaño del píxel cuenta doble:

| Error en | Error en la superficie |
|---|---|
| distancia +1 % (15 mm a 1500 mm) | +2 % |
| distancia focal +1 % | −2 % |
| cámara inclinada 2°, piel centrada | menos del 0.1 % |
| cámara inclinada 2°, piel a 300 mm del centro | alrededor del 1.4 % |
| contorno 1 píxel demasiado afuera, piel de 2 m² a 1.4 mm/px | alrededor de +0.5 % |
| objetivo enfocado a la mesa (13 mm a 1500 mm) | alrededor de +1.7 % |

La cámara se trata como una cámara estenopeica. Un objetivo real enfocado a la mesa hace la escala demasiado grande en aproximadamente distancia focal ÷ distancia; la comprobación con un objeto conocido (vea [Medir una piel](measure#check)) elimina este error junto con cualquier error en la distancia. La distorsión del objetivo no se corrige, y la superficie es la proyección de la piel sobre la mesa. Mantenga la piel en el centro de la imagen, donde la distorsión de la mayoría de los objetivos es menor.

## El artículo {#article}

Cómo se encuentra el contorno, cómo la superficie se deduce de la cámara, cómo se probaron ambos y cuál es la incertidumbre: vea el artículo [Measuring the surface of animal hides from a single photograph](../../articles/contorno-area-measurement/contorno-area-measurement) (en inglés).

## Archivos {#files}

- Fotos: JPEG, PNG, BMP, TIFF. Los datos de cámara se leen de JPEG y TIFF.
- Inspecciones: zip; vea [Guardar y abrir inspecciones](inspections#save).
- Ajustes: indicator.ini, en la carpeta de configuración de la aplicación (en Linux ~/.config/Contorno/Indicator/).

## Ajustes que se conservan {#settings}

La cámara de `Ajustes`, el idioma, el tema, la última carpeta y el área mínima se conservan entre sesiones. El área mínima no tiene campo en la ventana; se puede cambiar en indicator.ini (objects/min_area_mm2, uno de 0, 10, 50, 100, 500, 1000, 5000, 10000, 50000 mm²).

## Fotos {#photos}

La piel de vaca de las capturas es «Stretched Cowhide Skin» de Enda012, Wikimedia Commons (commons.wikimedia.org/wiki/File:Stretched_Cowhide_Skin.jpg), con licencia CC BY 4.0 (creativecommons.org/licenses/by/4.0/). Se redujo de tamaño y se muestra en Contorno Indicator con su contorno.
