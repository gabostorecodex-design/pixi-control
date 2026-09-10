# Privacidad de PIXI

- La clave de OpenRouter existe únicamente como secreto de Cloudflare.
- Las memorias permanecen en SQLite local.
- No se guardan imágenes ni audio.
- Cámara, calendario, acceso a notificaciones, ubicación y Spotify no se activan porque esas funciones aún no están implementadas.
- La aplicación solicita permisos de voz, BLE y notificaciones que sí utiliza.
- El usuario puede borrar recuerdos desde el panel de memoria.

Una futura sincronización deberá ser opcional, cifrada, versionada y activada con consentimiento explícito.
