package com.pixilife.android;

import android.Manifest;
import android.app.*;
import android.content.*;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.provider.Settings;
import java.util.*;

public final class AndroidPermissionManager {
 public static final int REQUEST_ALL=22622;
 private final Activity activity;
 public AndroidPermissionManager(Activity activity){this.activity=activity;}
 public String[] missingRuntimePermissions(){
  ArrayList<String> p=new ArrayList<>();
  add(p,Manifest.permission.CAMERA); add(p,Manifest.permission.RECORD_AUDIO);
  add(p,Manifest.permission.ACCESS_FINE_LOCATION); add(p,Manifest.permission.ACCESS_COARSE_LOCATION);
  add(p,Manifest.permission.READ_CALENDAR); add(p,Manifest.permission.WRITE_CALENDAR);
  if(Build.VERSION.SDK_INT>=31){add(p,Manifest.permission.BLUETOOTH_SCAN);add(p,Manifest.permission.BLUETOOTH_CONNECT);}
  if(Build.VERSION.SDK_INT>=33){add(p,Manifest.permission.POST_NOTIFICATIONS);add(p,Manifest.permission.NEARBY_WIFI_DEVICES);}
  return p.toArray(new String[0]);
 }
 private void add(List<String> p,String permission){if(activity.checkSelfPermission(permission)!=PackageManager.PERMISSION_GRANTED)p.add(permission);}
 public void requestAll(){String[] p=missingRuntimePermissions();if(p.length>0)activity.requestPermissions(p,REQUEST_ALL);}
 public boolean camera(){return activity.checkSelfPermission(Manifest.permission.CAMERA)==PackageManager.PERMISSION_GRANTED;}
 public boolean overlay(){return Settings.canDrawOverlays(activity);}
 public void openOverlaySettings(){activity.startActivity(new Intent(Settings.ACTION_MANAGE_OVERLAY_PERMISSION, Uri.parse("package:"+activity.getPackageName())));}
 public void openNotificationAccess(){activity.startActivity(new Intent("android.settings.ACTION_NOTIFICATION_LISTENER_SETTINGS"));}
 public void openExactAlarm(){if(Build.VERSION.SDK_INT>=31)activity.startActivity(new Intent(Settings.ACTION_REQUEST_SCHEDULE_EXACT_ALARM,Uri.parse("package:"+activity.getPackageName())));}
 public void openAppSettings(){activity.startActivity(new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS,Uri.parse("package:"+activity.getPackageName())));}
 public boolean notificationAccess(){String s=Settings.Secure.getString(activity.getContentResolver(),"enabled_notification_listeners");return s!=null&&s.contains(activity.getPackageName());}
}
