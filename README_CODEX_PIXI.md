# PIXI V8.5 — Guía para Codex

## Objetivo

Este proyecto convierte una **ESP32-2432S028 / CYD (Cheap Yellow Display)** en un mini robot de escritorio llamado **Pixi**.

Pixi debe funcionar principalmente **en local**, sin Gemini ni APIs externas para responder. El teléfono se usa como:

- micrófono
- altavoz / TTS
- panel de control
- interfaz HTTPS para permisos del navegador

La web HTTPS ofrece IA generativa local en WebGPU. Usa `Gemma 3 270M Instruct` como modo rápido predeterminado y conserva `Qwen2.5-0.5B-Instruct` como opción de mayor calidad. Ambos son gratuitos, no requieren API key y mantienen el motor del ESP32 como respaldo cuando el modelo no está disponible.

La IA no se ejecuta dentro de la ESP32. Chrome o Edge en el teléfono descarga el modelo cuantizado la primera vez, genera una respuesta y la envía por BLE. Se necesita WebGPU y suficiente memoria en el teléfono; si falla, la web reenvía la pregunta al motor local del firmware.

Para reducir el consumo de memoria en celulares, la web usa el reconocimiento de voz del navegador mientras la IA está activa y libera Whisper antes de cargar el modelo generativo. No mantener ambos modelos cargados simultáneamente.

Prioridades:

1. estabilidad
2. personalidad
3. velocidad
4. funciones

---

## Hardware objetivo

```text
ESP32-2432S028 / CYD
ESP32 integrado
Pantalla táctil 320x240
Flash 4 MB
Sin PSRAM
```

Arduino IDE:

```text
Board: ESP32 Dev Module
CPU Frequency: 240 MHz
Flash Frequency: 80 MHz
Flash Mode: QIO
Flash Size: 4MB
PSRAM: Disabled
Upload Speed: 115200
Partition Scheme: Minimal SPIFFS (1.9MB APP con OTA) recomendado
```

El firmware asume inicialmente:

```cpp
#define DISPLAY_CYD_2USB 1
```

Si la pantalla sale negra o incorrecta, revisar `LGFX_CYD.hpp`.

---

## Librerías necesarias

Instalar:

```text
esp32 by Espressif Systems
LovyanGFX
NimBLE-Arduino by h2zero
```

No volver a depender de:

```text
Gemini
Google AI
OpenAI API
Claude API
ArduinoJson para IA
HTTPClient para IA
WiFiClientSecure para IA
API keys
```

La lógica conversacional debe seguir siendo local.

---

## Archivos principales

```text
PIXI_CYD_V8_LIFE_HTTPS_BLE/
│
├── PIXI_CYD_V8_LIFE_HTTPS_BLE.ino
├── LGFX_CYD.hpp
├── README.txt
├── INICIO_RAPIDO.txt
├── platformio.ini
│
├── github-pages/
│   ├── index.html
│   ├── manifest.webmanifest
│   ├── sw.js
│   └── .nojekyll
│
├── .github/
│   └── workflows/
│       └── pages.yml
│
└── extras/
    └── PIXI_ESPNOW_NODE_EXAMPLE.ino
```

---

# Funciones que deben conservarse

## Motor local

Pixi usa un motor de respuestas local con más de:

```text
2.097.152 combinaciones
```

No guardar 2 millones de frases literales. Mantener el enfoque combinatorio para ahorrar flash.

También dispone de miles de preguntas espontáneas.

---

## Conversación

Debe reconocer y responder localmente a entradas como:

```text
hola
holi
oli
holaaaa
como estas
quien eres
que puedes hacer
gracias
te quiero
adios
duerme
despierta
minecraft
arduino
esp32
wifi
bluetooth
pantalla
bateria
cuentame un chiste
que te dije
te acuerdas
```

También debe manejar preguntas abiertas que comiencen con:

```text
que
como
por que
donde
cuando
quien
```

---

## Memoria corta

Guardar los últimos intercambios en RAM para contexto inmediato.

Ejemplo:

```text
Usuario: Me gusta Minecraft
Pixi: Yay, eso suena divertido

Usuario: ¿Qué te dije?
Pixi: Lo último que recuerdo...
```

---

## Memoria persistente

Usar `Preferences/NVS` para guardar:

```text
nombre del usuario
personalidad
juego favorito
color favorito
recuerdos
felicidad
energia
aburrimiento
curiosidad
confianza
irritacion
contador de conversaciones
contador de juegos
contador de toques
logros
alarma
zona horaria
```

No borrar estos datos al reiniciar.

---

## Personalidades

Mantener:

```text
tierna
juguetona
timida
traviesa
curiosa
dormilona
```

La personalidad debe cambiar el estilo de las respuestas.

---

## Estados internos

Mantener entre 0 y 100:

```text
happiness
energy
boredom
curiosity
trustLevel
irritation
```

Comportamiento esperado:

```text
si hablan demasiado seguido:
    sube irritacion

si nadie habla durante bastante tiempo:
    sube boredom

si la tocan:
    sube happiness y trust

si pasa mucho tiempo:
    baja energy

si irritation es alta:
    responde más seca / molesta

si boredom es alto:
    puede iniciar conversación sola
```

No describir estos estados como conciencia real.

---

## Comportamiento espontáneo

Pixi puede hablar sola.

Ejemplos:

```text
Oye... ¿sigues ahí?
Pi... estoy aburridita.
¿Qué estás haciendo?
¿Hacemos algo?
Cuéntame algo.
```

Mantener un sistema anti-repetición.

---

## Gestos táctiles

Objetivo:

```text
toque corto:
    reacción normal

doble toque:
    risa / felicidad

triple toque:
    activar/desactivar modo secreto

deslizar:
    mover mirada hacia dirección del gesto

mantener pulsado:
    dormir
```

---

## Caras

Mantener al menos:

```text
neutral
happy
veryhappy
sad
angry
surprised
scared
sleepy
thinking
love
wink
confused
bored
excited
sick
smug
crying
robot
annoyed
furious
sarcasm
unimpressed
embarrassed
proud
playful
dizzy
shocked
suspicious
nervous
relieved
grumpy
mischievous
deadpan
starry
happycry
pout
tongue
glitch
middlefinger
rebel
```

Las expresiones fuertes o groseras solo deben mostrarse cuando el usuario las
selecciona o envía el comando correspondiente; no deben aparecer al azar.

---

## Animaciones

Mantener/mejorar:

```text
parpadeo
mirada automática
mirada al tocar
risa
rubor
temblor
destellos
sueño
modo robot
```

Evitar animaciones que bloqueen el `loop()`.

---

## Mini juegos

Actualmente:

```text
piedra / papel / tijera
adivina el número
trivia
```

Se pueden agregar más sin comprometer memoria ni estabilidad.

---

## Hora y rutinas

Pixi puede recibir hora desde:

```text
navegador
NTP cuando hay Wi-Fi
```

Rutinas:

```text
mañana:
    saludo

noche:
    cansancio

hora de alarma:
    mostrar mensaje y reaccionar
```

---

## microSD

Si hay tarjeta:

```text
guardar logs
guardar conversaciones
guardar eventos
```

Si no hay tarjeta, Pixi debe seguir funcionando normalmente.

---

## OTA

Ruta local:

```text
/update
```

Debe permitir cargar firmware `.bin`.

---

## ESP-NOW

Pixi transmite estado a otras ESP32:

```text
nombre
felicidad
energia
aburrimiento
irritacion
cara
numero de conversaciones
```

Ejemplo receptor:

```text
extras/PIXI_ESPNOW_NODE_EXAMPLE.ino
```

---

# Bluetooth BLE

Pixi anuncia:

```text
PIXI
```

UUIDs:

```text
SERVICE:
6E400001-B5A3-F393-E0A9-E50E24DCCA9E

RX:
6E400002-B5A3-F393-E0A9-E50E24DCCA9E

TX:
6E400003-B5A3-F393-E0A9-E50E24DCCA9E
```

La web HTTPS usa BLE como canal principal entre el teléfono y Pixi.
Después de que el usuario autoriza el dispositivo una vez, la web intenta
recuperarlo con `navigator.bluetooth.getDevices()` y reconectarlo si se corta
la conexión. La primera autorización siempre requiere interacción del usuario.

---

# Arquitectura de voz

```text
USUARIO
   ↓
MICRÓFONO DEL TELÉFONO
   ↓
PÁGINA HTTPS
   ↓
reconocimiento de voz del navegador o Whisper local
   ↓
Gemma/Qwen local con salida progresiva
   ↓
Bluetooth BLE
   ↓
PIXI
   ↓
motor local
   ↓
respuesta
   ↓
Bluetooth BLE
   ↓
TELÉFONO
   ↓
speechSynthesis / TTS
```

---

## Problema importante del micrófono

Una página local como:

```text
http://192.168.4.1
```

puede devolver:

```text
not-allowed
```

por las restricciones de seguridad del navegador.

No intentar "forzar" permisos desde la ESP32.

La solución correcta es:

```text
HTTPS + getUserMedia + BLE
```

---

# Web HTTPS

La carpeta:

```text
github-pages/
```

contiene la interfaz pública.

Debe estar pensada para:

```text
Chrome Android
```

Funciones:

```text
conectar por BLE
reconectar automáticamente un dispositivo ya autorizado
pedir permiso de micrófono
Whisper local en el navegador (WebAssembly)
IA generativa local con texto visible mientras se genera
errores descriptivos al cargar modelos
PWA offline después de almacenar la app y dependencias usadas
TTS
enviar texto
mostrar respuesta
seleccionar voz
ajustar pitch
ajustar velocidad
cambiar personalidad
cambiar cara
configurar alarma
sincronizar hora
detectar emoción y enviarla a Pixi
activar las 22 expresiones nuevas
```

Los modelos grandes y sus librerías se guardan en caché durante la primera
descarga correcta. Por eso la instalación inicial requiere Internet; luego la
PWA puede abrirse sin conexión con los recursos que ya quedaron almacenados.

---

# Reconocimiento de emociones y modo seguro

La página y el firmware clasifican texto como `happy`, `sad`, `angry`,
`scared`, `surprised`, `love`, `bored` o `neutral`. La web envía `@emotion:`
antes de la pregunta y Pixi conserva esa cara mientras llega la respuesta.

El firmware registra reinicios causados por pánico o watchdog. Al detectar tres
arranques inestables entra en modo seguro: omite microSD, ESP-NOW, conexión Wi-Fi
de estación y conversación espontánea, pero conserva pantalla, panel local y
BLE para poder diagnosticarlo. También entra en modo seguro si la memoria libre
cae por debajo del umbral crítico. Tras 30 segundos de arranque estable limpia
el contador guardado en NVS.

---

# GitHub Pages

Workflow:

```text
.github/workflows/pages.yml
```

Repositorio recomendado:

```text
pixi-control
```

URL esperada:

```text
https://gabostorecodex-design.github.io/pixi-control/
```

La web debe publicarse desde:

```text
github-pages/
```

No mover esa carpeta sin actualizar el workflow.

---

# Web Bluetooth

No usar desde la página HTTPS:

```javascript
fetch("http://192.168.4.1/...")
```

Preferir:

```text
HTTPS -> BLE -> ESP32
```

para evitar problemas de mixed content.

---

# Panel local

Pixi sigue creando:

```text
SSID: PIXI-AI
Password: pixirobot
IP: 192.168.4.1
```

La web local sirve para:

```text
Wi-Fi
perfil
alarma
memoria
caras
brillo
OTA
diagnóstico
```

La voz principal debe preferir la web HTTPS.

---

# Reglas para Codex

## 1. No eliminar funciones existentes sin motivo

Antes de quitar algo:

```text
buscar dónde se usa
revisar dependencias
reemplazarlo si hace falta
```

## 2. No volver a meter IA remota

No agregar APIs externas salvo que el usuario lo pida explícitamente.

## 3. Priorizar velocidad

Evitar:

```text
delay() largos
peticiones bloqueantes
loops infinitos
esperas de red
```

## 4. Priorizar memoria

La placa tiene:

```text
4 MB Flash
sin PSRAM
```

Preferir:

```text
tablas pequeñas
generación combinatoria
PROGMEM cuando convenga
memoria dinámica limitada
```

## 5. Mantener Arduino IDE

Debe seguir compilando como:

```text
ESP32 Dev Module
```

No convertir el proyecto exclusivamente a ESP-IDF.

## 6. Mantener PlatformIO

Conservar:

```text
platformio.ini
```

## 7. No romper pantalla/touch

`LGFX_CYD.hpp` contiene la configuración de display/touch.

Modificar solo si es necesario.

## 8. SD opcional

No bloquear el arranque si falta microSD.

## 9. Wi-Fi opcional

El motor local debe seguir funcionando sin Internet.

## 10. BLE local

BLE debe seguir funcionando aunque no haya Internet.

---

# Compilación

Arduino IDE:

```text
1. Abrir PIXI_CYD_V8_LIFE_HTTPS_BLE.ino
2. Seleccionar ESP32 Dev Module
3. Seleccionar puerto COM correcto
4. Flash Size 4MB
5. PSRAM Disabled
6. Partition Scheme: Minimal SPIFFS
7. Upload Speed 115200
8. Verificar
9. Subir
```

---

# Si falla la compilación

Revisar primero:

```text
versión de esp32 by Espressif Systems
LovyanGFX instalada
NimBLE-Arduino instalada
conflictos entre librerías BLE
tamaño final del sketch
errores de API de NimBLE
errores de ESP-NOW por versión del core ESP32
```

No reemplazar librerías al azar.

---

# Diagnóstico esperado

Si Codex modifica el firmware:

1. compilar
2. corregir errores
3. revisar warnings importantes
4. confirmar tamaño del sketch
5. confirmar que cabe en la partición
6. no declarar éxito si no compiló

---

# Mejoras futuras permitidas

```text
más mini juegos
más animaciones
mejor anti-repetición
mejor memoria semántica local
sistema de prioridades
rutinas más inteligentes
estado de ánimo más natural
más comandos BLE
mejor panel web
PWA más completa
notificaciones locales
sonidos robot
más logros
más gestos
más estadísticas
mejor guardado en SD
más tipos de alarmas
temporizadores
modo noche
modo silencioso
modo demostración
```

Siempre respetando:

```text
ESP32 clásica
4 MB Flash
sin PSRAM
respuesta rápida
motor local
```

---

# Meta final

Pixi debe sentirse como una pequeña mascota/robot de escritorio:

```text
expresiva
curiosa
tierna
a veces traviesa
a veces cansada
a veces molesta
capaz de recordar
capaz de iniciar conversación
rápida
sin depender de una IA online
```

Prioridad final:

```text
estabilidad > personalidad > velocidad > cantidad de funciones
```

Si una nueva función hace que Pixi deje de compilar o vuelva lenta la interfaz,
debe simplificarse antes de integrarla.
