# Live Vision

La APK mantiene `LiveVisionService` como servicio de cámara en primer plano después de abrir PIXI. Captura un fotograma frontal local cada cinco segundos, reemplaza el anterior y muestra la notificación obligatoria **PIXI está viendo**.

Los fotogramas no se envían continuamente. Cuando se formula una pregunta visual, `VisionClient` envía automáticamente el último fotograma al Worker, sin exigir pulsar **Analizar imagen**. Esto mantiene visión reciente y evita consumir la API cada cinco segundos.

Android puede impedir que una aplicación inicie la cámara directamente desde el arranque del teléfono. PIXI inicia el servicio cuando el usuario abre la aplicación y lo mantiene en segundo plano mientras el sistema lo permita.
