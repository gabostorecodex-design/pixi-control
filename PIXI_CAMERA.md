# Cámara y visión de PIXI

La APK 2.0 usa Camera2 con vista previa real. El botón **Abrir ojos de PIXI** inicia la cámara frontal, permite cambiar a la cámara trasera y analiza un fotograma actual mediante `/vision` del Worker.

- La cámara solo se abre por una acción del usuario.
- La pantalla muestra `CÁMARA ACTIVA` mientras el recurso está abierto.
- Las capturas de consulta se mantienen en memoria y no se guardan como archivos.
- Al cerrar la pantalla se cierran `CameraCaptureSession`, `CameraDevice` e `ImageReader`.
- La respuesta visual evita inferencias sensibles y diagnósticos.

La visión requiere internet, el Worker desplegado, `OPENROUTER_API_KEY` y `PIXI_ACCESS_TOKEN` configurados en Cloudflare.
