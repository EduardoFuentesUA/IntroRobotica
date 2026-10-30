# Guía 1 · Introducción a la Robótica

Guía de iniciación: construyes un robot **seguidor de línea con control PID**,
que además puedes manejar a mano por Bluetooth, desde el diseño del chasis hasta
la app del celular.

**Libro:** [Introduccion-a-la-Robotica.pdf](Introduccion-a-la-Robotica.pdf)

**Hardware:** NodeMCU ESP8266 v1.0 (Amica) sobre shield HW-588A, módulo L298N
para los motores, módulo seguidor de línea de 2 canales con sensores TCRT5000,
módulo Bluetooth HC-05 y 4 pilas AA.

## Código (NodeMCU ESP8266)

| Carpeta | Qué hace | Capítulo |
|---|---|---|
| [`codigo/CodigoDefinitivo/`](codigo/CodigoDefinitivo/) | Seguidor de línea con control PID y modo manual por Bluetooth | 7 |

- Los demás programas del libro (Capítulos 5 y 6) no se descargan: están
  completos en sus capítulos, para que los escribas tú mismo.
- En el Arduino IDE, agrega el paquete del ESP8266 (URL de tarjetas
  `https://arduino.esp8266.com/stable/package_esp8266com_index.json`) y elige la
  placa **NodeMCU 1.0 (ESP-12E Module)**.
- Cada programa está dentro de una carpeta con su mismo nombre, porque el
  Arduino IDE así lo exige. Ábrelo desde esa carpeta, sin moverlo ni renombrarlo.
- El HC-05 va a los pines RX/TX del NodeMCU: **desconéctalo antes de subir un
  programa**.

## App de control (MIT App Inventor)

| Archivo | Para qué sirve |
|---|---|
| [`app/Guia_Robotica.apk`](app/Guia_Robotica.apk) | App lista para instalar en un celular Android |
| [`app/Guia_Robotica.aia`](app/Guia_Robotica.aia) | Proyecto para abrir y modificar en MIT App Inventor |

La app envía dos mensajes de texto, cada uno terminado en salto de línea:

| Mensaje | Ejemplo | Qué hace |
|---|---|---|
| `KP:x,KI:y,KD:z` | `KP:400,KI:0,KD:0` | Fija las ganancias del PID e inicia el modo automático |
| `M:izq,der` | `M:150,-150` | Modo manual: velocidad de cada motor, de −255 a 255 |

## Chasis del autor (STL)

| Archivo | Pieza |
|---|---|
| [`chasis/chasis.stl`](chasis/chasis.stl) | Placa del chasis, 250 × 130 × 9 mm |
| [`chasis/guardabarros.stl`](chasis/guardabarros.stl) | Guardabarros |
| [`chasis/union-motor-1.stl`](chasis/union-motor-1.stl) | Pieza de unión de los motores al chasis |
| [`chasis/union-motor-2.stl`](chasis/union-motor-2.stl) | Segunda pieza de unión de los motores |

> **Atención:** la placa mide 250 × 130 mm y no cabe en las camas de
> 220 × 220 mm de muchas impresoras. Compruébalo en Cura antes de laminar. Si no
> cabe, úsala como referencia y diseña tu propia placa, como propone el
> Capítulo 2.
