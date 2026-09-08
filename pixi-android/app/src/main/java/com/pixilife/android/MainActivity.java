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
import android.os.Build;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

public class MainActivity extends Activity {
    private static final int PERMISSIONS = 42;
    private TextView status, heard, reply;
    private EditText workerUrl, accessToken, message;
    private SharedPreferences prefs;
    private final BroadcastReceiver events = new BroadcastReceiver() {
        @Override public void onReceive(Context context, Intent intent) {
            String type = intent.getStringExtra("type");
            String value = intent.getStringExtra("value");
            if ("reply".equals(type)) reply.setText(value == null ? "" : value);
            else if ("heard".equals(type)) heard.setText(value == null ? "" : value);
            else if ("status".equals(type)) status.setText(value == null ? "" : value);
        }
    };

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        prefs = getSharedPreferences("pixi", MODE_PRIVATE);
        buildUi();
        requestPermissionsIfNeeded();
    }

    private void buildUi() {
        ScrollView scroll = new ScrollView(this);
        LinearLayout root = new LinearLayout(this); root.setOrientation(LinearLayout.VERTICAL); root.setPadding(28, 28, 28, 28); root.setBackgroundColor(Color.rgb(7, 11, 17));
        TextView title = text("🎀 Pixi Voice Android", 26, Color.WHITE); root.addView(title);
        TextView subtitle = text("OpenAI GPT-5 nano · BLE · conversación continua", 15, Color.LTGRAY); root.addView(subtitle);
        status = text("Desconectada", 15, Color.rgb(114,255,192)); root.addView(status);
        Button connect = button("🔵 Conectar / reconectar PIXI"); root.addView(connect); connect.setOnClickListener(v -> startServiceAction(ConversationService.ACTION_SCAN));
        workerUrl = edit("URL HTTPS de tu openai-worker"); workerUrl.setText(prefs.getString("worker_url", "")); root.addView(workerUrl);
        accessToken = edit("Clave de acceso Pixi (no API key)"); accessToken.setText(prefs.getString("access_token", "")); accessToken.setInputType(129); root.addView(accessToken);
        Button save = button("Guardar conexión OpenAI"); root.addView(save); save.setOnClickListener(v -> { prefs.edit().putString("worker_url", workerUrl.getText().toString().trim()).putString("access_token", accessToken.getText().toString().trim()).apply(); status.setText("Conexión guardada"); });
        Button continuous = button("🔁 Iniciar conversación continua"); root.addView(continuous); continuous.setOnClickListener(v -> { save.performClick(); startServiceAction(ConversationService.ACTION_CONTINUOUS); continuous.setText("⏹ Detener conversación continua"); continuous.setOnClickListener(x -> startServiceAction(ConversationService.ACTION_STOP)); });
        message = edit("Escribe un mensaje"); root.addView(message);
        Button send = button("Enviar a Pixi"); root.addView(send); send.setOnClickListener(v -> { String value = message.getText().toString().trim(); if (!value.isEmpty()) { heard.setText(value); startServiceAction(ConversationService.ACTION_MESSAGE, value); message.setText(""); } });
        heard = text("Tú: ...", 17, Color.WHITE); root.addView(heard);
        reply = text("Pixi: ...", 17, Color.WHITE); root.addView(reply);
        TextView permissions = text("Permisos usados: micrófono, Bluetooth cercano, ubicación en Android antiguo, notificaciones y servicio en primer plano. Android puede limitar micrófono si fuerzas el cierre de la app.", 13, Color.GRAY); root.addView(permissions);
        scroll.addView(root); setContentView(scroll);
    }

    private TextView text(String value, int size, int color) { TextView t = new TextView(this); t.setText(value); t.setTextSize(size); t.setTextColor(color); t.setPadding(0, 12, 0, 12); return t; }
    private EditText edit(String hint) { EditText e = new EditText(this); e.setHint(hint); e.setHintTextColor(Color.GRAY); e.setTextColor(Color.WHITE); e.setSingleLine(true); e.setPadding(16, 12, 16, 12); return e; }
    private Button button(String label) { Button b = new Button(this); b.setText(label); b.setTextColor(Color.WHITE); return b; }
    private void startServiceAction(String action) { startServiceAction(action, null); }
    private void startServiceAction(String action, String text) { Intent intent = new Intent(this, ConversationService.class).setAction(action); if (text != null) intent.putExtra("text", text); if (Build.VERSION.SDK_INT >= 26) startForegroundService(intent); else startService(intent); }

    private void requestPermissionsIfNeeded() {
        java.util.ArrayList<String> needed = new java.util.ArrayList<>();
        if (Build.VERSION.SDK_INT >= 31) { if (checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN) != PackageManager.PERMISSION_GRANTED) needed.add(Manifest.permission.BLUETOOTH_SCAN); if (checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) needed.add(Manifest.permission.BLUETOOTH_CONNECT); }
        else if (checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED) needed.add(Manifest.permission.ACCESS_FINE_LOCATION);
        if (checkSelfPermission(Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) needed.add(Manifest.permission.RECORD_AUDIO);
        if (Build.VERSION.SDK_INT >= 33 && checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) needed.add(Manifest.permission.POST_NOTIFICATIONS);
        if (!needed.isEmpty()) requestPermissions(needed.toArray(new String[0]), PERMISSIONS);
    }

    @Override protected void onResume() { super.onResume(); IntentFilter filter = new IntentFilter(ConversationService.EVENT); if (Build.VERSION.SDK_INT >= 33) registerReceiver(events, filter, RECEIVER_NOT_EXPORTED); else registerReceiver(events, filter); }
    @Override protected void onPause() { super.onPause(); try { unregisterReceiver(events); } catch (Exception ignored) {} }
}
