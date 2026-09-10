package com.pixilife.android;
import android.content.*;
public class BootReceiver extends BroadcastReceiver {public void onReceive(Context c,Intent i){if(Intent.ACTION_BOOT_COMPLETED.equals(i.getAction()))c.getSharedPreferences("pixi",Context.MODE_PRIVATE).edit().putLong("last_boot_seen",System.currentTimeMillis()).apply();}}
