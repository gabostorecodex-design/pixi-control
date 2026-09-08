package com.pixilife.android;

import android.Manifest;
import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.webkit.JavascriptInterface;
import android.webkit.WebChromeClient;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;

public class MainActivity extends Activity {
    private static final int PERMISSIONS = 42;
    private static final String PAGE = "https://gabostorecodex-design.github.io/pixi-control/";
    private WebView web;
    private SharedPreferences prefs;
    private boolean continuous;
    private final BroadcastReceiver events = new BroadcastReceiver() {
        @Override public void onReceive(Context context, Intent intent) {
            String type = intent.getStringExtra("type"), value = intent.getStringExtra("value");
            if (web == null || value == null) return;
            String safe = js(value);
            if ("reply".equals(type)) web.evaluateJavascript("document.getElementById('reply').textContent='" + safe + "';", null);
            else if ("heard".equals(type)) web.evaluateJavascript("document.getElementById('heard').textContent='" + safe + "';", null);
            else if ("status".equals(type)) web.evaluateJavascript("document.getElementById('micHelp').textContent='" + safe + "';", null);
        }
    };

    @Override public void onCreate(Bundle state) {
        super.onCreate(state); prefs = getSharedPreferences("pixi", MODE_PRIVATE); requestPermissionsIfNeeded();
        web = new WebView(this); WebSettings settings = web.getSettings(); settings.setJavaScriptEnabled(true); settings.setDomStorageEnabled(true); settings.setMediaPlaybackRequiresUserGesture(false); settings.setAllowFileAccess(false); web.setWebChromeClient(new WebChromeClient());
        web.addJavascriptInterface(new PixiBridge(), "PixiNative"); web.setWebViewClient(new WebViewClient() { @Override public void onPageFinished(WebView view, String url) { injectBridge(); } }); setContentView(web); web.loadUrl(PAGE);
    }

    private void injectBridge() {
        String url = js(prefs.getString("worker_url", "https://pixi-openai.gabostorecodex.workers.dev")); String token = js(prefs.getString("access_token", "Gabo@22622"));
        String script = "javascript:(function(){localStorage.setItem('pixi-openai-url','" + url + "');localStorage.setItem('pixi-access-token','" + token + "');window.connectPixi=function(){PixiNative.connect()};window.toggleContinuous=function(){PixiNative.toggle()};window.sendText=function(){var e=document.getElementById('txt');if(e&&e.value.trim())PixiNative.send(e.value.trim())};window.saveApiConfig=function(){PixiNative.config(document.getElementById('apiUrl').value,document.getElementById('apiAccess').value);};var u=document.getElementById('apiUrl'),a=document.getElementById('apiAccess');if(u)u.value='" + url + "';if(a)a.value='" + token + "';})();";
        web.evaluateJavascript(script, null);
    }

    private String js(String value) { return value.replace("\\", "\\\\").replace("'", "\\'").replace("\n", "\\n").replace("\r", ""); }
    private void action(String action, String text) { Intent i = new Intent(this, ConversationService.class).setAction(action); if (text != null) i.putExtra("text", text); if (Build.VERSION.SDK_INT >= 26) startForegroundService(i); else startService(i); }
    private void requestPermissionsIfNeeded() { java.util.ArrayList<String> needed = new java.util.ArrayList<>(); if (Build.VERSION.SDK_INT >= 31) { if (checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN) != PackageManager.PERMISSION_GRANTED) needed.add(Manifest.permission.BLUETOOTH_SCAN); if (checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) needed.add(Manifest.permission.BLUETOOTH_CONNECT); } else if (checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED) needed.add(Manifest.permission.ACCESS_FINE_LOCATION); if (checkSelfPermission(Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) needed.add(Manifest.permission.RECORD_AUDIO); if (Build.VERSION.SDK_INT >= 33 && checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) needed.add(Manifest.permission.POST_NOTIFICATIONS); if (!needed.isEmpty()) requestPermissions(needed.toArray(new String[0]), PERMISSIONS); }
    @Override protected void onResume() { super.onResume(); registerReceiver(events, new IntentFilter(ConversationService.EVENT), Build.VERSION.SDK_INT >= 33 ? RECEIVER_NOT_EXPORTED : 0); }
    @Override protected void onPause() { super.onPause(); try { unregisterReceiver(events); } catch (Exception ignored) {} }
    @Override public void onBackPressed() { if (web != null && web.canGoBack()) web.goBack(); else super.onBackPressed(); }

    private final class PixiBridge {
        @JavascriptInterface public void connect() { action(ConversationService.ACTION_SCAN, null); }
        @JavascriptInterface public void toggle() { continuous = !continuous; action(continuous ? ConversationService.ACTION_CONTINUOUS : ConversationService.ACTION_STOP, null); }
        @JavascriptInterface public void send(String text) { action(ConversationService.ACTION_MESSAGE, text); }
        @JavascriptInterface public void config(String url, String token) { prefs.edit().putString("worker_url", url.trim().replaceAll("/+$", "")).putString("access_token", token.trim()).apply(); }
    }
}
