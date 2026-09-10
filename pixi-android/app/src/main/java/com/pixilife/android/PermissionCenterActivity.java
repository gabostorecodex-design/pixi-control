package com.pixilife.android;

import android.app.*;import android.os.*;import android.graphics.Color;import android.view.*;import android.widget.*;import android.content.*;import android.content.pm.PackageManager;

public class PermissionCenterActivity extends Activity {
 private LinearLayout root; private AndroidPermissionManager pm;
 @Override public void onCreate(Bundle b){super.onCreate(b);pm=new AndroidPermissionManager(this);render();}
 @Override protected void onResume(){super.onResume();if(root!=null)render();}
 private void render(){ScrollView s=new ScrollView(this);s.setBackgroundColor(Color.rgb(7,11,17));root=new LinearLayout(this);root.setOrientation(LinearLayout.VERTICAL);root.setPadding(24,24,24,40);
  title("CENTRO DE PERMISOS PIXI");info("PIXI solo usa cada permiso cuando activas su función. Android exige configurar manualmente los accesos especiales.");
  Button all=button("Solicitar permisos normales");all.setOnClickListener(v->pm.requestAll());root.addView(all);
  row("Cámara",android.Manifest.permission.CAMERA,"Visión frontal y trasera");row("Micrófono",android.Manifest.permission.RECORD_AUDIO,"Conversación y sincronización de boca");row("Ubicación precisa",android.Manifest.permission.ACCESS_FINE_LOCATION,"Clima y lugares reales");row("Calendario",android.Manifest.permission.READ_CALENDAR,"Consultar eventos autorizados");
  if(Build.VERSION.SDK_INT>=31)row("Bluetooth",android.Manifest.permission.BLUETOOTH_CONNECT,"Conectar con el ESP32 PIXI");if(Build.VERSION.SDK_INT>=33)row("Notificaciones",android.Manifest.permission.POST_NOTIFICATIONS,"Alarmas y servicio activo");
  special("Superposición flotante",pm.overlay(),"Mostrar la cara de PIXI sobre otras apps",v->pm.openOverlaySettings());special("Acceso a notificaciones",pm.notificationAccess(),"Leer solo notificaciones autorizadas",v->pm.openNotificationAccess());special("Alarmas exactas",canExact(),"Activar alarmas aunque la app esté cerrada",v->pm.openExactAlarm());
  Button settings=button("Abrir ajustes completos de PIXI");settings.setOnClickListener(v->pm.openAppSettings());root.addView(settings);s.addView(root);setContentView(s);}
 private boolean canExact(){AlarmManager a=(AlarmManager)getSystemService(ALARM_SERVICE);return Build.VERSION.SDK_INT<31||a.canScheduleExactAlarms();}
 private void row(String n,String p,String why){boolean ok=checkSelfPermission(p)==PackageManager.PERMISSION_GRANTED;TextView v=info(n+" — "+(ok?"CONCEDIDO":"DENEGADO")+"\n"+why);v.setTextColor(ok?Color.rgb(94,230,160):Color.rgb(255,155,90));}
 private void special(String n,boolean ok,String why,View.OnClickListener l){TextView v=info(n+" — "+(ok?"CONCEDIDO":"REQUIERE AJUSTES")+"\n"+why);v.setTextColor(ok?Color.rgb(94,230,160):Color.rgb(255,155,90));Button b=button("Configurar "+n);b.setOnClickListener(l);root.addView(b);}
 private void title(String x){TextView t=info(x);t.setTextSize(24);t.setTextColor(Color.WHITE);root.addView(t);}private TextView info(String x){TextView t=new TextView(this);t.setText(x);t.setTextColor(Color.LTGRAY);t.setTextSize(15);t.setPadding(8,12,8,12);root.addView(t);return t;}private Button button(String x){Button b=new Button(this);b.setText(x);b.setAllCaps(false);b.setTextSize(16);return b;}
}
