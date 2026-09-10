# Memoria de PIXI

La memoria principal de Android usa SQLite (`pixi_memory.db`) con WAL y versión de esquema. Incluye tablas para recuerdos, acciones, ajustes, recordatorios y tareas.

`MemoryManager` implementa guardar, obtener, buscar, actualizar, borrar, listar, exportar e importar. Los tipos disponibles son sesión, corto plazo, largo plazo, importante y sistema. Las frases que contienen “recuerda que” se guardan; “olvida …” elimina la primera coincidencia encontrada.

Los recuerdos se envían al Worker como contexto únicamente cuando el usuario conversa. No se sincronizan online todavía. Sobreviven a cierres y reinicios, pero se pierden al borrar datos o desinstalar la APK mientras no exista un respaldo externo.
