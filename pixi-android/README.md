# Pixi Voice para Android

Aplicación nativa para mantener conversación continua con Pixi mientras la
aplicación está abierta o instalada. Usa el micrófono del sistema, TTS,
Bluetooth BLE con reconexión, notificaciones y un servicio en primer plano.

La aplicación no contiene la API key de OpenAI. En la pantalla inicial guarda
la URL de `openai-worker` y la clave de acceso Pixi; la API key real permanece
como secreto del Worker. Usa `gpt-5-nano` con salida progresiva.

## Compilar

Abre esta carpeta en Android Studio y ejecuta **Build > Make Project**. El
proyecto usa Android Gradle Plugin 8.5.2, compileSdk 35 y Java 8. En este equipo
no se encontró Android Studio, Gradle ni Android SDK, por lo que la compilación
queda para Android Studio o para una máquina con esas herramientas instaladas.

## Permisos

La aplicación solicita micrófono, Bluetooth cercano, ubicación en Android
antiguo, notificaciones, Internet y servicio en primer plano. Android puede
suspender el micrófono si el usuario fuerza el cierre de la aplicación; ningún
permiso permite mantenerlo activo después de un cierre forzado.
