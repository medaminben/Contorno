---
title: Guardar y abrir inspecciones
order: 6
summary: Un archivo zip conserva la foto, la cámara, el contorno y la superficie.
---

## Guardar {#save}

Pulse `Guardar` en cuanto el visor muestre el resultado. Elija una carpeta y un nombre; se propone el nombre de la foto. Mientras se analiza una foto, `Guardar` espera al resultado: el panel de mensajes dice *Espere a que termine el análisis.*

El archivo es un zip corriente que cualquier programa puede leer:

| Archivo | Contenido |
|---|---|
| original.jpg (o .png, …) | la foto, byte a byte tal como se importó |
| mask.png | el objeto en blanco sobre negro, al tamaño de la foto |
| camera.json | la distancia, la distancia focal y el tamaño de píxel usados, y de dónde salió cada uno |
| parameters.json | el área mínima y la tolerancia del contorno |
| objects.dxf | el contorno y los agujeros en milímetros, para CAD y máquinas de corte |
| result.json | el tamaño de la foto, la escala y la superficie en mm² |

## Abrir {#open}

Pulse `Abrir` y elija un zip guardado. La foto, el contorno y la superficie vuelven exactamente como se guardaron, sin un análisis nuevo, con la cámara guardada. El panel de mensajes termina la línea con *resultado guardado*.

El siguiente `Importar` vuelve a la cámara de `Ajustes`.

::: note
Una inspección guardada por una versión anterior de Contorno (una inspección de piel) abre su foto y su cámara y se analiza de nuevo con el contorno de IA. El panel de mensajes lo indica.
:::

## Carpetas {#folders}

`Importar`, `Abrir` y `Guardar` empiezan todos en la carpeta del último archivo elegido en cualquiera de ellos.
