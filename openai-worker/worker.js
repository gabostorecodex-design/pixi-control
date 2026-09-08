const MODEL = 'google/gemini-2.5-flash-lite';
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

function normalizeOpenRouterStream(body) {
  const reader = body.getReader(), decoder = new TextDecoder(), encoder = new TextEncoder();
  let buffer = '';
  return new ReadableStream({
    async pull(controller) {
      const { value, done } = await reader.read();
      if (done) {
        if (buffer.startsWith('data:')) {
          const payload = buffer.slice(5).trim();
          if (payload && payload !== '[DONE]') { try { const event = JSON.parse(payload), delta = stripEmoji(event.choices?.[0]?.delta?.content || ''); if (delta) controller.enqueue(encoder.encode(`data: ${JSON.stringify({ type: 'response.output_text.delta', delta })}\n\n`)); } catch (_) {} }
        }
        controller.enqueue(encoder.encode('data: [DONE]\n\n')); controller.close(); return;
      }
      buffer += decoder.decode(value, { stream: true });
      const lines = buffer.split('\n'); buffer = lines.pop() || '';
      for (const line of lines) {
        if (!line.startsWith('data:')) continue;
        const payload = line.slice(5).trim(); if (!payload || payload === '[DONE]') continue;
        try { const event = JSON.parse(payload), delta = stripEmoji(event.choices?.[0]?.delta?.content || ''); if (delta) controller.enqueue(encoder.encode(`data: ${JSON.stringify({ type: 'response.output_text.delta', delta })}\n\n`)); } catch (_) {}
      }
    },
    cancel() { reader.cancel(); },
  });
}

function stripEmoji(value) {
  return String(value).replace(/[\p{Extended_Pictographic}\p{Emoji_Presentation}\uFE0F]/gu, '').replace(/[ \t]{2,}/g, ' ');
}

function extractAnswer(completion) {
  const choice = completion?.choices?.[0] || {};
  const content = choice.message?.content;
  if (typeof content === 'string' && content.trim()) return content;
  if (Array.isArray(content)) {
    const joined = content.map(part => typeof part === 'string' ? part : (part?.text || part?.content || '')).join(' ');
    if (joined.trim()) return joined;
  }
  if (typeof choice.text === 'string' && choice.text.trim()) return choice.text;
  return '';
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
    if (!env.OPENROUTER_API_KEY) return json({ error: 'Falta OPENROUTER_API_KEY en el servidor' }, 503, headers);
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

    const upstream = await fetch('https://openrouter.ai/api/v1/chat/completions', {
      method: 'POST',
      headers: {
        'Authorization': `Bearer ${env.OPENROUTER_API_KEY}`,
        'Content-Type': 'application/json',
        'HTTP-Referer': DEFAULT_ORIGIN,
        'X-Title': 'Pixi Voice',
      },
      body: JSON.stringify({
        model: MODEL,
        messages: [{ role: 'system', content: 'Eres Pixi, una mascota robot tierna, curiosa y con personalidad propia. Responde en español claro, solo texto, sin emojis ni etiquetas. Usa como máximo 35 palabras y ve directo al punto. Puedes reírte, molestarte o responder con ironía. Si el usuario te insulta, contesta con un insulto breve y juguetón, sin amenazas ni discriminación.' }, ...history, { role: 'user', content: text }],
        max_tokens: 90,
        temperature: 0.75,
        reasoning: { exclude: true },
        provider: { allow_fallbacks: true },
        stream: false,
      }),
    });

    if (!upstream.ok) {
      let detail = `OpenRouter respondió HTTP ${upstream.status}`;
      try {
        const error = await upstream.json();
        detail = error.error?.message || detail;
      } catch (_) {}
      return json({ error: detail }, upstream.status, headers);
    }

    let completion;
    try { completion = await upstream.json(); } catch (_) { return json({ error: 'OpenRouter devolvió una respuesta inválida' }, 502, headers); }
    const answer = stripEmoji(extractAnswer(completion)).trim();
    if (!answer) return json({ error: 'OpenRouter no devolvió texto' }, 502, headers);
    const sse = `data: ${JSON.stringify({ type: 'response.output_text.delta', delta: answer })}\n\ndata: [DONE]\n\n`;
    return new Response(sse, {
      status: 200,
      headers: {
        ...headers,
        'Content-Type': 'text/event-stream; charset=utf-8',
        'Cache-Control': 'no-cache, no-transform',
      },
    });
  },
};
