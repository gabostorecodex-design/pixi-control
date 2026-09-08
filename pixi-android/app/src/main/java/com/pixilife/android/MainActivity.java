package com.pixilife.android;

import android.Manifest;
import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.os.Build;
import android.os.Bundle;
import android.text.InputType;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

public class MainActivity extends Activity {
    private static final int PERMISSIONS = 42;
    private final int bg = Color.rgb(8, 12, 22), card = Color.rgb(19, 27, 43), accent = Color.rgb(116, 94, 255), pink = Color.rgb(255, 113, 177);
    private TextView status, heard, reply, mode;
    private EditText workerUrl, accessToken, message;
    private SharedPreferences prefs;
    private Button continuous;
    private final BroadcastReceiver events = new BroadcastReceiver() {
        @Override public void onReceive(Context context, Intent intent) {
            String type = intent.getStringExtra("type"), value = intent.getStringExtra("value");
            if ("reply".equals(type)) reply.setText(value == null ? "" : value);
            else if ("heard".equals(type)) heard.setText(value == null ? "" : value);
            else if ("status".equals(type)) { status.setText(value == null ? "" : value); mode.setText(value != null && value.toLowerCase().contains("escuch") ? "MICROFONO ACTIVO" : "PIXI VOICE"); }
        }
    };

    @Override public void onCreate(Bundle state) { super.onCreate(state); prefs = getSharedPreferences("pixi", MODE_PRIVATE); buildUi(); requestPermissionsIfNeeded(); }

    private void buildUi() {
        ScrollView scroll = new ScrollView(this); scroll.setFillViewport(true); scroll.setBackgroundColor(bg);
        LinearLayout root = new LinearLayout(this); root.setOrientation(LinearLayout.VERTICAL); root.setPadding(24, 28, 24, 28);
        TextView title = label("PIXI  /  VOICE", 27, Color.WHITE); title.setTypeface(Typeface.DEFAULT, Typeface.BOLD); root.addView(title);
        TextView subtitle = label("Companion robotico con OpenAI y BLE", 15, Color.rgb(180, 188, 210)); root.addView(subtitle);
        mode = label("PIXI VOICE", 11, Color.rgb(130, 245, 190)); mode.setTypeface(Typeface.DEFAULT, Typeface.BOLD); mode.setPadding(0, 18, 0, 6); root.addView(mode);
        status = label("Listo para conectar", 14, Color.rgb(180, 188, 210)); root.addView(status);

        LinearLayout ble = card(); ble.addView(label("CONEXION BLE", 13, pink));
        ble.addView(label("Conecta PIXI una vez. El servicio mantiene la reconexion mientras conversas.", 13, Color.LTGRAY));
        Button connect = button("CONECTAR / RECONectar PIXI", accent); ble.addView(connect); connect.setOnClickListener(v -> startServiceAction(ConversationService.ACTION_SCAN)); root.addView(ble);

        LinearLayout config = card(); config.addView(label("CONEXION SEGURA", 13, pink));
        config.addView(label("La URL HTTPS es la direccion de tu Worker de Cloudflare que envia preguntas a OpenAI. Ejemplo: https://pixi-openai.tu-dominio.workers.dev\n\nLa clave de acceso Pixi es el token que configuraste como PIXI_ACCESS_TOKEN en ese Worker. No pegues aqui una clave sk-proj de OpenAI.", 13, Color.LTGRAY));
        workerUrl = edit("https://...workers.dev/chat", false); workerUrl.setText(prefs.getString("worker_url", "")); config.addView(workerUrl);
        accessToken = edit("Token PIXI_ACCESS_TOKEN", true); accessToken.setText(prefs.getString("access_token", "")); config.addView(accessToken);
        Button save = button("GUARDAR CONFIGURACION", Color.rgb(42, 155, 124)); config.addView(save); save.setOnClickListener(v -> saveConfig()); root.addView(config);

        LinearLayout talk = card(); talk.addView(label("CONVERSACION CONTINUA", 13, pink)); talk.addView(label("Pulsa iniciar, bloquea la pantalla si quieres y habla. Pixi escucha, responde con voz tierna y vuelve a escuchar.", 13, Color.LTGRAY));
        continuous = button("INICIAR CONVERSACION", pink); talk.addView(continuous); continuous.setOnClickListener(v -> { saveConfig(); startServiceAction(ConversationService.ACTION_CONTINUOUS); continuous.setText("DETENER CONVERSACION"); continuous.setOnClickListener(x -> { startServiceAction(ConversationService.ACTION_STOP); continuous.setText("INICIAR CONVERSACION"); }); }); root.addView(talk);

        LinearLayout chat = card(); chat.addView(label("CHAT RAPIDO", 13, pink)); message = edit("Escribe un mensaje para Pixi", false); chat.addView(message); Button send = button("ENVIAR", accent); chat.addView(send); send.setOnClickListener(v -> { String value = message.getText().toString().trim(); if (!value.isEmpty()) { heard.setText(value); startServiceAction(ConversationService.ACTION_MESSAGE, value); message.setText(""); } }); heard = label("Tu: ...", 16, Color.WHITE); heard.setPadding(0, 18, 0, 8); chat.addView(heard); reply = label("Pixi: ...", 16, Color.rgb(255, 220, 245)); chat.addView(reply); root.addView(chat);
        root.addView(label("La voz usa el motor TTS del telefono con tono dulce, velocidad pausada y voz espanola disponible. Para que siga en segundo plano debes iniciar la conversacion y permitir microfono/notificaciones. Android puede detenerla si fuerzas el cierre.", 12, Color.rgb(145, 155, 175)));
        scroll.addView(root); setContentView(scroll);
    }

    private LinearLayout card() { LinearLayout box = new LinearLayout(this); box.setOrientation(LinearLayout.VERTICAL); box.setPadding(18, 16, 18, 18); GradientDrawable shape = new GradientDrawable(); shape.setColor(card); shape.setCornerRadius(22); box.setBackground(shape); LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(-1, -2); lp.setMargins(0, 14, 0, 0); box.setLayoutParams(lp); return box; }
    private TextView label(String value, int size, int color) { TextView t = new TextView(this); t.setText(value); t.setTextSize(size); t.setTextColor(color); t.setPadding(0, 5, 0, 5); return t; }
    private EditText edit(String hint, boolean secret) { EditText e = new EditText(this); e.setHint(hint); e.setHintTextColor(Color.rgb(120, 130, 150)); e.setTextColor(Color.WHITE); e.setSingleLine(true); e.setPadding(15, 8, 15, 8); if (secret) e.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PASSWORD); GradientDrawable shape = new GradientDrawable(); shape.setColor(Color.rgb(29, 39, 59)); shape.setCornerRadius(14); e.setBackground(shape); LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(-1, -2); lp.setMargins(0, 9, 0, 0); e.setLayoutParams(lp); return e; }
    private Button button(String label, int color) { Button b = new Button(this); b.setText(label); b.setTextColor(Color.WHITE); b.setTextSize(13); b.setAllCaps(false); b.setGravity(Gravity.CENTER); GradientDrawable shape = new GradientDrawable(); shape.setColor(color); shape.setCornerRadius(15); b.setBackground(shape); LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(-1, 52); lp.setMargins(0, 11, 0, 0); b.setLayoutParams(lp); return b; }
    private void saveConfig() { String url = workerUrl.getText().toString().trim().replaceAll("/+$", ""); String token = accessToken.getText().toString().trim(); prefs.edit().putString("worker_url", url).putString("access_token", token).apply(); status.setText(url.startsWith("https://") && !token.isEmpty() ? "Configuracion guardada y lista" : "Completa URL HTTPS y token Pixi"); }
    private void startServiceAction(String action) { startServiceAction(action, null); }
    private void startServiceAction(String action, String text) { Intent intent = new Intent(this, ConversationService.class).setAction(action); if (text != null) intent.putExtra("text", text); if (Build.VERSION.SDK_INT >= 26) startForegroundService(intent); else startService(intent); }
    private void requestPermissionsIfNeeded() { java.util.ArrayList<String> needed = new java.util.ArrayList<>(); if (Build.VERSION.SDK_INT >= 31) { if (checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN) != PackageManager.PERMISSION_GRANTED) needed.add(Manifest.permission.BLUETOOTH_SCAN); if (checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) needed.add(Manifest.permission.BLUETOOTH_CONNECT); } else if (checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED) needed.add(Manifest.permission.ACCESS_FINE_LOCATION); if (checkSelfPermission(Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) needed.add(Manifest.permission.RECORD_AUDIO); if (Build.VERSION.SDK_INT >= 33 && checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) needed.add(Manifest.permission.POST_NOTIFICATIONS); if (!needed.isEmpty()) requestPermissions(needed.toArray(new String[0]), PERMISSIONS); }
    @Override protected void onResume() { super.onResume(); IntentFilter filter = new IntentFilter(ConversationService.EVENT); if (Build.VERSION.SDK_INT >= 33) registerReceiver(events, filter, RECEIVER_NOT_EXPORTED); else registerReceiver(events, filter); }
    @Override protected void onPause() { super.onPause(); try { unregisterReceiver(events); } catch (Exception ignored) {} }
}
