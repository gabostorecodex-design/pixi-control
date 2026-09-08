# Pixi Voice para Android

Aplicacion nativa para mantener conversacion continua con Pixi mientras la
aplicacion esta abierta o instalada. Usa microfono del sistema, TTS, Bluetooth
BLE con reconexion, notificaciones y un servicio en primer plano.

La aplicacion no contiene la API key de OpenAI. En la pantalla inicial guarda
la URL de `openai-worker` y la clave de acceso Pixi; la API key real permanece
como secreto del Worker. Usa Mistral Small 3.2 mediante OpenRouter con salida progresiva.

## Compilar

Abre esta carpeta en Android Studio y ejecuta **Build > Make Project**. El
proyecto usa Android Gradle Plugin 8.1.4, Gradle 8.1.1, compileSdk 35 y Java 17.
La compilacion de prueba se verifico con `:app:assembleDebug`; el APK queda en
`app/build/outputs/apk/debug/app-debug.apk`.

## Permisos

La aplicacion solicita microfono, Bluetooth cercano, ubicacion en Android
antiguo, notificaciones, Internet y servicio en primer plano. Android puede
suspender el microfono si el usuario fuerza el cierre; ningun permiso permite
mantenerlo activo despues de un cierre forzado.
