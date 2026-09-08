const MODEL = 'gpt-5-nano';
const DEFAULT_ORIGIN = 'https://gabostorecodex-design.github.io';

function cors(origin, env) {
  const allowed = env.ALLOWED_ORIGIN || DEFAULT_ORIGIN;
  return {
    'Access-Control-Allow-Origin': origin === allowed ? origin : allowed,
    'Access-Control-Allow-Headers': 'Content-Type, X-Pixi-Token',
    'Access-Control-Allow-Methods': 'POST, OPTIONS',
    'Vary': 'Origin',
  };
}

function json(data, status, headers) {
  return new Response(JSON.stringify(data), {
    status,
    headers: { ...headers, 'Content-Type': 'application/json; charset=utf-8' },
  });
}

export default {
  async fetch(request, env) {
    const url = new URL(request.url);
    const origin = request.headers.get('Origin') || '';
    const headers = cors(origin, env);

    if (request.method === 'OPTIONS') return new Response(null, { status: 204, headers });
    if (url.pathname === '/health') return json({ ok: true, model: MODEL }, 200, headers);
    if (url.pathname !== '/chat' || request.method !== 'POST') return json({ error: 'Ruta no encontrada' }, 404, headers);
    // WebView Android puede enviar Origin vacio; el token Pixi sigue siendo obligatorio.
    if (origin && origin !== (env.ALLOWED_ORIGIN || DEFAULT_ORIGIN)) return json({ error: 'Origen no autorizado' }, 403, headers);
    if (!env.OPENAI_API_KEY) return json({ error: 'Falta OPENAI_API_KEY en el servidor' }, 503, headers);
    if (!env.PIXI_ACCESS_TOKEN || request.headers.get('X-Pixi-Token') !== env.PIXI_ACCESS_TOKEN) return json({ error: 'Clave de acceso Pixi incorrecta' }, 401, headers);

    let body;
    try {
      body = await request.json();
    } catch (_) {
      return json({ error: 'Solicitud JSON inválida' }, 400, headers);
    }

    const text = String(body.text || '').trim().slice(0, 600);
    if (!text) return json({ error: 'Falta el mensaje' }, 400, headers);
    const history = Array.isArray(body.history) ? body.history.slice(-6).map(item => ({
      role: item.role === 'assistant' ? 'assistant' : 'user',
      content: String(item.content || '').slice(0, 600),
    })) : [];

    const upstream = await fetch('https://api.openai.com/v1/responses', {
      method: 'POST',
      headers: {
        'Authorization': `Bearer ${env.OPENAI_API_KEY}`,
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({
        model: MODEL,
        instructions: 'Eres Pixi, una pequeña mascota robot tierna, curiosa y útil. Responde en español claro. Usa máximo 60 palabras. Mantén una personalidad juguetona. Si no sabes algo, dilo. No afirmes tener conciencia ni sentimientos reales.',
        input: [...history, { role: 'user', content: text }],
        reasoning: { effort: 'minimal' },
        max_output_tokens: 140,
        stream: true,
      }),
    });

    if (!upstream.ok) {
      let detail = `OpenAI respondió HTTP ${upstream.status}`;
      try {
        const error = await upstream.json();
        detail = error.error?.message || detail;
      } catch (_) {}
      return json({ error: detail }, upstream.status, headers);
    }

    return new Response(upstream.body, {
      status: 200,
      headers: {
        ...headers,
        'Content-Type': 'text/event-stream; charset=utf-8',
        'Cache-Control': 'no-cache, no-transform',
      },
    });
  },
};
