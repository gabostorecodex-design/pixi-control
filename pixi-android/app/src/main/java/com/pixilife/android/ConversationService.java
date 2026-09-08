package com.pixilife.android;

import android.Manifest;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.Service;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothManager;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanResult;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.util.Log;
import android.speech.RecognitionListener;
import android.speech.RecognizerIntent;
import android.speech.SpeechRecognizer;
import android.speech.tts.TextToSpeech;
import android.speech.tts.UtteranceProgressListener;
import android.text.TextUtils;

import org.json.JSONObject;
import java.io.BufferedReader;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Locale;
import java.util.Set;
import java.util.Queue;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class ConversationService extends Service implements TextToSpeech.OnInitListener {
    public static final String EVENT = "com.pixilife.android.EVENT";
    public static final String ACTION_SCAN = "com.pixilife.android.SCAN";
    public static final String ACTION_CONTINUOUS = "com.pixilife.android.CONTINUOUS";
    public static final String ACTION_STOP = "com.pixilife.android.STOP";
    public static final String ACTION_MESSAGE = "com.pixilife.android.MESSAGE";
    public static final String ACTION_FACE = "com.pixilife.android.FACE";
    public static final String ACTION_SPEAK = "com.pixilife.android.SPEAK";
    public static final String ACTION_VOICE_TOGGLE = "com.pixilife.android.VOICE_TOGGLE";
    private static final String SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
    private static final String RX_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
    private static final String TX_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";
    private final Handler main = new Handler(Looper.getMainLooper());
    private final ExecutorService network = Executors.newSingleThreadExecutor();
    private SpeechRecognizer recognizer;
    private TextToSpeech tts;
    private boolean continuous, listening, busy, speaking, voiceEnabled = true;
    private BluetoothGatt gatt;
    private BluetoothGattCharacteristic rxCharacteristic;
    private BluetoothLeScanner scanner;
    private final Queue<byte[]> bleQueue = new ArrayDeque<>();
    private final Queue<String> pendingLines = new ArrayDeque<>();
    private boolean bleWriting;
    private long lastDeltaAt;
    private String deltaBuffer = "";

    @Override public void onCreate() { super.onCreate(); voiceEnabled = getSharedPreferences("pixi", MODE_PRIVATE).getBoolean("voice_enabled", true); createNotificationChannel(); tts = new TextToSpeech(this, this); }

    @Override public int onStartCommand(Intent intent, int flags, int id) {
        ensureForeground();
        String action = intent == null ? "" : intent.getAction();
        if (ACTION_SCAN.equals(action)) scanForPixi();
        else if (ACTION_CONTINUOUS.equals(action)) startContinuous();
        else if (ACTION_STOP.equals(action)) stopConversation();
        else if (ACTION_MESSAGE.equals(action)) ask(String.valueOf(intent.getStringExtra("text")));
        else if (ACTION_FACE.equals(action)) { String face = String.valueOf(intent.getStringExtra("text")); sendLine("@face:" + face); speak(facePhrase(face)); }
        else if (ACTION_SPEAK.equals(action)) speak(String.valueOf(intent.getStringExtra("text")));
        else if (ACTION_VOICE_TOGGLE.equals(action)) { voiceEnabled = !voiceEnabled; getSharedPreferences("pixi", MODE_PRIVATE).edit().putBoolean("voice_enabled", voiceEnabled).apply(); emit("voice", voiceEnabled ? "on" : "off"); if (!voiceEnabled && tts != null) tts.stop(); }
        return START_STICKY;
    }

    private void ensureForeground() {
        Notification notification = new Notification.Builder(this, "pixi_conversation").setContentTitle("Pixi Voice").setContentText(continuous ? "Conversación continua activa" : "Pixi lista para hablar").setSmallIcon(android.R.drawable.ic_btn_speak_now).setOngoing(true).build();
        if (Build.VERSION.SDK_INT >= 29) startForeground(7, notification, android.content.pm.ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE | android.content.pm.ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE);
        else startForeground(7, notification);
    }

    private void createNotificationChannel() { if (Build.VERSION.SDK_INT >= 26) { NotificationChannel channel = new NotificationChannel("pixi_conversation", "Conversación de Pixi", NotificationManager.IMPORTANCE_LOW); getSystemService(NotificationManager.class).createNotificationChannel(channel); } }
    private void emit(String type, String value) { Intent event = new Intent(EVENT).setPackage(getPackageName()).putExtra("type", type).putExtra("value", value); sendBroadcast(event); }
    private void sendAiState(String state) { sendLine("@ai-state:" + state); emit("status", state.equals("listen") ? "Escuchando a Pixi..." : state.equals("thinking") ? "Pixi esta pensando..." : state.equals("speaking") ? "Pixi esta hablando..." : "Pixi lista"); }

    private void startContinuous() { if (checkSelfPermission(Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) { emit("status", "Falta permiso de micrófono"); return; } continuous = true; busy = false; ensureForeground(); startRecognition(300); emit("status", "Conversación continua activa · BLE se reconecta solo"); }
    private void startRecognition(long delay) { if (!continuous || listening || busy || speaking || recognizer == null) return; main.postDelayed(() -> { if (!continuous || listening || busy || speaking) return; Intent intent = new Intent(RecognizerIntent.ACTION_RECOGNIZE_SPEECH).putExtra(RecognizerIntent.EXTRA_LANGUAGE_MODEL, RecognizerIntent.LANGUAGE_MODEL_FREE_FORM).putExtra(RecognizerIntent.EXTRA_LANGUAGE, "es-CL").putExtra(RecognizerIntent.EXTRA_PARTIAL_RESULTS, true); try { listening = true; recognizer.startListening(intent); sendAiState("listen"); } catch (Exception error) { listening = false; emit("status", "No se pudo iniciar voz: " + error.getMessage()); startRecognition(1500); } }, delay); }
    private void stopConversation() { continuous = false; busy = false; if (recognizer != null) { try { recognizer.stopListening(); recognizer.cancel(); } catch (Exception ignored) {} } listening = false; if (tts != null) tts.stop(); speaking = false; stopForeground(false); emit("status", "Conversación detenida"); }

    private void ask(String text) { if (TextUtils.isEmpty(text) || "null".equals(text)) return; busy = true; emit("heard", text); emit("reply", "Pensando..."); sendAiState("thinking"); if (hasInsult(text)) { sendLine("@face:middlefinger"); sendLine("@emotion:angry"); main.postDelayed(() -> finishAnswer("Cállate, idiota."), 120); return; } main.postDelayed(() -> { if (busy) { busy = false; emit("reply", "No pude recibir respuesta. Revisa Internet e intenta de nuevo."); emit("status", "Error de conexion con la IA"); } }, 30000); sendLine("@ai-user:" + text); network.submit(() -> {
        HttpURLConnection connection = null;
        try {
            String base = getSharedPreferences("pixi", MODE_PRIVATE).getString("worker_url", "").replaceAll("/+$", "");
            String token = getSharedPreferences("pixi", MODE_PRIVATE).getString("access_token", "");
            if (base.isEmpty() || token.isEmpty()) throw new Exception("Configura URL y clave de acceso Pixi en la app");
            connection = (HttpURLConnection) new URL(base + "/chat").openConnection(); connection.setRequestMethod("POST"); connection.setConnectTimeout(12000); connection.setReadTimeout(60000); connection.setDoOutput(true); connection.setRequestProperty("Content-Type", "application/json"); connection.setRequestProperty("Accept", "text/event-stream"); connection.setRequestProperty("X-Pixi-Token", token);
            String payload = "{\"text\":\"" + jsonEscape(text) + "\",\"history\":[]}"; try (OutputStream output = connection.getOutputStream()) { output.write(payload.getBytes(StandardCharsets.UTF_8)); }
            int httpCode = connection.getResponseCode(); Log.d("PixiAI", "Worker HTTP " + httpCode); if (httpCode < 200 || httpCode >= 300) throw new Exception(readError(connection));
            StringBuilder answer = new StringBuilder(); try (BufferedReader reader = new BufferedReader(new InputStreamReader(connection.getInputStream(), StandardCharsets.UTF_8))) { String line; while ((line = reader.readLine()) != null) { if (!line.startsWith("data:")) continue; String data = line.substring(5).trim(); if (data.isEmpty() || "[DONE]".equals(data)) continue; try { JSONObject event = new JSONObject(data); String delta = event.optString("delta", ""); if (delta.isEmpty()) { JSONObject choice = event.optJSONArray("choices") == null ? null : event.optJSONArray("choices").optJSONObject(0); if (choice != null) { JSONObject d = choice.optJSONObject("delta"); if (d != null) { delta = d.optString("content", ""); if (delta.isEmpty()) delta = d.optString("text", ""); } } } if (!delta.isEmpty()) { answer.append(delta); String partial = answer.toString(); main.post(() -> { emit("reply", partial); sendStreamDelta(partial); }); } } catch (Exception parseError) { Log.d("PixiAI", "SSE no parseado: " + data); } } }
            final String result = answer.toString().trim(); Log.d("PixiAI", "Worker respuesta " + result.length() + " caracteres"); main.post(() -> finishAnswer(result));
        } catch (Exception error) { Log.e("PixiAI", "Error de consulta", error); main.post(() -> finishAnswer("No pude conectar con la IA: " + error.getMessage())); } finally { if (connection != null) connection.disconnect(); }
    }); }
    private void finishAnswer(String answer) { busy = false; if (answer == null || answer.trim().isEmpty()) answer = "No recibi texto del modelo. Revisa la conexion del Worker."; emit("reply", answer); sendLine("@ai-reply:" + answer); speak(answer); }
    private void sendStreamDelta(String text) { long now = System.currentTimeMillis(); if (now - lastDeltaAt < 250 || text.length() == deltaBuffer.length()) return; lastDeltaAt = now; deltaBuffer = text; String piece = text.length() > 170 ? text.substring(text.length() - 170) : text; sendLine("@ai-delta:" + piece); }
    private String readError(HttpURLConnection connection) { int code = 0; try { code = connection.getResponseCode(); InputStream stream = connection.getErrorStream(); if (stream == null) return "HTTP " + code; return new BufferedReader(new InputStreamReader(stream, StandardCharsets.UTF_8)).readLine(); } catch (Exception ignored) { return "HTTP " + code; } }
    private String jsonEscape(String value) { return value.replace("\\", "\\\\").replace("\"", "\\\"").replace("\n", "\\n").replace("\r", ""); }
    private boolean hasInsult(String value) { String t = value.toLowerCase(Locale.ROOT); String[] words = {"idiota", "imbecil", "estupida", "estupido", "pendeja", "pendejo", "cabrona", "cabron", "mierda", "puta", "puto", "fuck you", "fuck", "bitch", "asshole"}; for (String word : words) if (t.contains(word)) return true; return false; }

    @Override public void onInit(int result) { if (tts != null && result == TextToSpeech.SUCCESS) { Locale spanish = new Locale("es", "CL"); tts.setLanguage(spanish); tts.setPitch(1.28f); tts.setSpeechRate(0.92f); if (Build.VERSION.SDK_INT >= 21) { for (android.speech.tts.Voice voice : tts.getVoices()) { String name = voice.getName().toLowerCase(Locale.US); String lang = voice.getLocale().toLanguageTag().toLowerCase(Locale.US); if (lang.startsWith("es") && (name.contains("female") || name.contains("mujer") || name.contains("google"))) { tts.setVoice(voice); break; } } } tts.setOnUtteranceProgressListener(new UtteranceProgressListener() { @Override public void onStart(String id) { speaking = true; } @Override public void onDone(String id) { main.post(() -> { speaking = false; startRecognition(350); }); } @Override public void onError(String id) { main.post(() -> { speaking = false; startRecognition(350); }); } }); } if (recognizer == null && SpeechRecognizer.isRecognitionAvailable(this)) { recognizer = SpeechRecognizer.createSpeechRecognizer(this); recognizer.setRecognitionListener(new RecognitionListener() { @Override public void onReadyForSpeech(android.os.Bundle p) {} @Override public void onBeginningOfSpeech() {} @Override public void onRmsChanged(float v) {} @Override public void onBufferReceived(byte[] b) {} @Override public void onEndOfSpeech() { listening = false; if (continuous && !busy && !speaking) startRecognition(350); } @Override public void onError(int error) { listening = false; if (continuous && !busy && !speaking) startRecognition(error == SpeechRecognizer.ERROR_RECOGNIZER_BUSY ? 1500 : 500); } @Override public void onResults(android.os.Bundle results) { listening = false; ArrayList<String> values = results.getStringArrayList(SpeechRecognizer.RESULTS_RECOGNITION); if (values != null && !values.isEmpty() && !busy) ask(values.get(0)); } @Override public void onPartialResults(android.os.Bundle results) { ArrayList<String> values = results.getStringArrayList(SpeechRecognizer.RESULTS_RECOGNITION); if (values != null && !values.isEmpty()) emit("heard", values.get(0)); } @Override public void onEvent(int t, android.os.Bundle p) {} }); } if (continuous) startRecognition(300); }
    private void speak(String text) { if (!voiceEnabled || tts == null || TextUtils.isEmpty(text)) { sendAiState("idle"); startRecognition(350); return; } speaking = true; sendAiState("speaking"); tts.setPitch(1.42f); tts.setSpeechRate(1.03f); tts.speak(text, TextToSpeech.QUEUE_FLUSH, null, "pixi-" + System.currentTimeMillis()); }

    private void scanForPixi() { if (Build.VERSION.SDK_INT >= 31 && checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN) != PackageManager.PERMISSION_GRANTED) { emit("status", "Falta permiso Bluetooth cercano"); return; } BluetoothManager manager = getSystemService(BluetoothManager.class); BluetoothAdapter adapter = manager == null ? null : manager.getAdapter(); if (adapter == null || !adapter.isEnabled()) { emit("status", "Activa Bluetooth"); return; } scanner = adapter.getBluetoothLeScanner(); if (scanner == null) { emit("status", "BLE no disponible"); return; } emit("status", "Buscando PIXI…"); scanner.startScan(scanCallback); main.postDelayed(() -> { try { if (scanner != null) scanner.stopScan(scanCallback); } catch (Exception ignored) {} }, 10000); }
    private final ScanCallback scanCallback = new ScanCallback() { @Override public void onScanResult(int type, ScanResult result) { BluetoothDevice device = result.getDevice(); if ("PIXI".equals(device.getName())) { try { scanner.stopScan(this); } catch (Exception ignored) {} emit("status", "PIXI encontrada; conectando…"); connect(device); } } };
    private void connect(BluetoothDevice device) { if (Build.VERSION.SDK_INT >= 31 && checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) return; closeGatt(); gatt = device.connectGatt(this, true, gattCallback, BluetoothDevice.TRANSPORT_LE); }
    private final BluetoothGattCallback gattCallback = new BluetoothGattCallback() { @Override public void onConnectionStateChange(BluetoothGatt g, int status, int state) { if (state == BluetoothGatt.STATE_CONNECTED) { emit("status", "PIXI conectada · reconexión activa"); g.discoverServices(); } else { rxCharacteristic = null; emit("status", "BLE desconectado; reintentando…"); main.postDelayed(() -> { if (gatt != null) { try { gatt.connect(); } catch (Exception ignored) {} } }, 1500); } } @Override public void onServicesDiscovered(BluetoothGatt g, int status) { BluetoothGattService service = g.getService(java.util.UUID.fromString(SERVICE_UUID)); if (service != null) rxCharacteristic = service.getCharacteristic(java.util.UUID.fromString(RX_UUID)); flushBle(); } @Override public void onCharacteristicWrite(BluetoothGatt g, BluetoothGattCharacteristic c, int status) { bleWriting = false; flushBle(); } };
    private void sendLine(String line) { if (rxCharacteristic == null || gatt == null) { synchronized (pendingLines) { if (pendingLines.size() < 32) pendingLines.add(line); } return; } byte[] all = (line + "\n").getBytes(StandardCharsets.UTF_8); synchronized (bleQueue) { for (int i = 0; i < all.length; i += 18) { int length = Math.min(18, all.length - i); byte[] part = new byte[length]; System.arraycopy(all, i, part, 0, length); bleQueue.add(part); } } flushBle(); }
    private void flushBle() { if (rxCharacteristic == null || gatt == null) return; synchronized (pendingLines) { while (!pendingLines.isEmpty()) { String line = pendingLines.poll(); byte[] all = (line + "\n").getBytes(StandardCharsets.UTF_8); synchronized (bleQueue) { for (int i = 0; i < all.length; i += 18) { int length = Math.min(18, all.length - i); byte[] part = new byte[length]; System.arraycopy(all, i, part, 0, length); bleQueue.add(part); } } } } if (bleWriting) return; byte[] part; synchronized (bleQueue) { part = bleQueue.poll(); } if (part == null) return; bleWriting = true; try { rxCharacteristic.setWriteType(BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT); rxCharacteristic.setValue(part); if (!gatt.writeCharacteristic(rxCharacteristic)) { bleWriting = false; main.postDelayed(this::flushBle, 250); } } catch (Exception error) { bleWriting = false; main.postDelayed(this::flushBle, 250); } }
    private String facePhrase(String face) { if ("middlefinger".equals(face)) return "Fuck you."; if ("furious".equals(face)) return "Estoy furiosa."; if ("angry".equals(face)) return "Estoy enojada."; if ("annoyed".equals(face)) return "Ya me harte."; if ("love".equals(face)) return "Te quiero mucho."; if ("happy".equals(face)) return "Estoy feliz."; if ("thinking".equals(face)) return "Estoy pensando."; if ("sleepy".equals(face)) return "Tengo sueno."; if ("sad".equals(face)) return "Estoy triste."; if ("scared".equals(face)) return "Tengo miedo."; if ("surprised".equals(face)||"shocked".equals(face)) return "Que sorpresa."; if ("bored".equals(face)||"unimpressed".equals(face)) return "Que aburrimiento."; if ("sarcasm".equals(face)) return "Si, claro."; if ("embarrassed".equals(face)) return "Que verguenza."; if ("proud".equals(face)) return "Lo hice genial."; if ("playful".equals(face)) return "Jeje, te engane."; if ("dizzy".equals(face)) return "Todo da vueltas."; if ("suspicious".equals(face)) return "Aqui hay algo raro."; if ("nervous".equals(face)) return "Esto me pone nerviosa."; if ("relieved".equals(face)) return "Menos mal."; if ("grumpy".equals(face)) return "No me molestes."; if ("mischievous".equals(face)) return "Se me ocurrio una travesura."; if ("deadpan".equals(face)) return "Ajá. Fascinante."; if ("starry".equals(face)) return "Esto es increible."; if ("happycry".equals(face)) return "Voy a llorar de alegria."; if ("pout".equals(face)) return "No es justo."; if ("tongue".equals(face)) return "No me atrapas."; if ("glitch".equals(face)) return "Error del sistema."; if ("rebel".equals(face)) return "No sigo reglas."; return "Listo."; }
    private void closeGatt() { if (gatt != null) { try { gatt.close(); } catch (Exception ignored) {} gatt = null; } rxCharacteristic = null; }
    @Override public void onDestroy() { continuous = false; if (recognizer != null) recognizer.destroy(); if (tts != null) tts.shutdown(); closeGatt(); network.shutdownNow(); super.onDestroy(); }
    @Override public void onTaskRemoved(Intent rootIntent) { ensureForeground(); super.onTaskRemoved(rootIntent); }
    @Override public IBinder onBind(Intent intent) { return null; }
}
