# Arquitectura de PIXI

PIXI conserva cuatro componentes: firmware ESP32, aplicación Android, interfaz web y Worker Cloudflare. Android mantiene la conexión BLE, voz, alarmas y memoria SQLite. El Worker protege la clave de OpenRouter y entrega respuestas de IA. El ESP32 representa el estado mediante rostro, pantalla, NVS, SD, BLE y servidor web.

## Flujo principal

1. Android obtiene voz o texto con permiso del usuario.
2. `MemoryManager` consulta recuerdos locales autorizados.
3. `ConversationService` envía texto, historial corto y contexto de memoria al Worker.
4. El Worker consulta OpenRouter sin exponer la clave.
5. Android reproduce la voz y envía estado, emoción y texto al ESP32 por BLE.
6. El ESP32 anima ojos, cejas, boca y burbuja de texto.

## Límites actuales

Spotify requiere una aplicación registrada, Client ID, Redirect URI y OAuth PKCE. El respaldo online requiere elegir proveedor y consentimiento. Cámara, clima, calendario, lugares, notificaciones y sensores todavía no están implementados. No se presentan como terminados.
