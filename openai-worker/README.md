# Servidor seguro de OpenAI para Pixi

Este Cloudflare Worker mantiene la API key fuera de GitHub Pages y usa
`gpt-5-nano` con respuestas en tiempo real.

La API key compartida en el chat debe revocarse. Crea una nueva antes de seguir.

## Publicar

1. Instala Node.js.
2. Abre una terminal dentro de `openai-worker`.
3. Ejecuta `npx wrangler login`.
4. Guarda la nueva clave con `npx wrangler secret put OPENAI_API_KEY`.
5. Crea una contraseña larga diferente y guárdala con
   `npx wrangler secret put PIXI_ACCESS_TOKEN`.
6. Ejecuta `npx wrangler deploy`.
7. Copia la URL `https://pixi-openai....workers.dev` en la página de Pixi.
8. En **Clave de acceso Pixi**, escribe la contraseña creada en el paso 5.

No copies la API key de OpenAI en la página, el firmware, `.dev.vars`,
`wrangler.toml` ni ningún archivo que vaya a GitHub.

El Worker acepta solicitudes únicamente desde
`https://gabostorecodex-design.github.io`. Si cambia el dominio, actualiza
`ALLOWED_ORIGIN` antes de desplegar.
