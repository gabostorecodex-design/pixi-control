# Permisos Android de PIXI

| Permiso | Uso | Si se deniega |
|---|---|---|
| RECORD_AUDIO | Conversación por voz | Se mantiene entrada por texto |
| BLUETOOTH_SCAN | Encontrar el ESP32 en Android 12+ | No se inicia el escaneo |
| BLUETOOTH_CONNECT | Conectar y mantener BLE | La app continúa sin robot |
| ACCESS_FINE_LOCATION | Descubrimiento BLE en Android antiguo | No se escanea en esas versiones |
| POST_NOTIFICATIONS | Servicio visible y alarmas | Android puede ocultar avisos |
| SCHEDULE_EXACT_ALARM | Alarmas precisas | Se usa programación aproximada |
| FOREGROUND_SERVICE_MICROPHONE | Voz en segundo plano | No se mantiene escucha continua |
| FOREGROUND_SERVICE_CONNECTED_DEVICE | BLE en segundo plano | No se mantiene la conexión |

PIXI no solicita cámara, calendario, lectura multimedia, superposición, accesibilidad ni acceso a notificaciones hasta que exista una función real asociada.
