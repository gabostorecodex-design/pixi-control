PIXI V8.4 LIFE + HTTPS BLE
=========================

V8.4 incluye:
- motor local 2M+
- memoria persistente
- nombre y gustos
- personalidades
- felicidad, energia, aburrimiento, curiosidad, confianza e irritacion
- rutina diaria y hora
- alarma
- preguntas espontaneas
- anti-repeticion
- mini juegos
- gestos: doble toque, triple toque secreto, swipe, mantener para dormir
- animaciones extra
- modo escritorio
- exportar/importar memoria
- OTA desde la web local
- microSD para logs
- logros
- BLE
- ESP-NOW para tus otras ESP32
- pagina HTTPS lista para GitHub Pages con Whisper local
- IA generativa Qwen2.5 local y gratuita en el navegador, sin API key
- microfono y TTS del telefono

LINK HTTPS / MICROFONO
----------------------
GitHub Pages sirve por HTTPS. Eso permite que getUserMedia pueda pedir permiso
del microfono en navegadores compatibles.

La pagina alojada se comunica con Pixi por BLE:
Telefono -> HTTPS -> BLE -> Pixi.

Esto evita depender de http://192.168.x.x para el microfono.

En Android, usa preferentemente Chrome.
Web Bluetooth no esta disponible en todos los navegadores.

GITHUB PAGES
------------
El ZIP trae:
github-pages/
.github/workflows/pages.yml

Sube todo a un repo, por ejemplo "pixi-control", activa GitHub Pages con
GitHub Actions y la URL sera normalmente:

https://gabostorecodex-design.github.io/pixi-control/

GitHub Pages usa HTTPS.

ARDUINO
-------
Instala:
1. esp32 by Espressif Systems
2. LovyanGFX
3. NimBLE-Arduino de h2zero

Placa:
ESP32 Dev Module

Ajustes:
CPU 240 MHz
Flash 4MB
PSRAM Disabled
Upload Speed 115200
Partition Scheme Minimal SPIFFS recomendado para conservar OTA

Archivo:
PIXI_CYD_V8_LIFE_HTTPS_BLE.ino

RED LOCAL:
PIXI-AI
clave: pixirobot
web: 192.168.4.1

MICROSD:
CS 5
SCK 18
MISO 19
MOSI 23

Si no insertas tarjeta, Pixi sigue funcionando.

NOTA:
No se puede crear por software un microfono o parlante fisico inexistente.
Esta V8.4 usa el telefono para audio e IA. Una version futura puede usar INMP441 + parlante.
