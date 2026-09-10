# Permisos Android de PIXI 2.0

La APK incluye un **Centro de permisos** con estado real y acceso a las pantallas oficiales.

| Permiso/acceso | Función | Tipo |
|---|---|---|
| CAMERA | Cámara frontal/trasera y visión | Runtime |
| RECORD_AUDIO | Voz y conversación | Runtime |
| ACCESS_FINE/COARSE_LOCATION | Clima y lugares según ubicación | Runtime |
| BLUETOOTH_SCAN/CONNECT | Descubrir y conectar el ESP32 | Runtime desde Android 12 |
| POST_NOTIFICATIONS | Alarmas y servicio persistente | Runtime desde Android 13 |
| READ/WRITE_CALENDAR | Agenda autorizada | Runtime |
| NEARBY_WIFI_DEVICES | Dispositivos Wi-Fi cercanos | Runtime desde Android 13 |
| SYSTEM_ALERT_WINDOW | PIXI flotante | Ajustes especiales |
| Notification Listener | Lectura filtrada de notificaciones | Ajustes especiales |
| SCHEDULE_EXACT_ALARM | Alarmas exactas | Ajustes especiales |
| RECEIVE_BOOT_COMPLETED | Restauración de estado después de reinicio | Normal |

Android no permite que una APK normal habilite silenciosamente superposición, accesibilidad o acceso a notificaciones. En instalaciones normales el usuario debe hacerlo desde el Centro de permisos. Si se deniega un permiso, PIXI continúa funcionando sin esa función.

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
