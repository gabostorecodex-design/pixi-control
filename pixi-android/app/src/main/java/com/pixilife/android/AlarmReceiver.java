package com.pixilife.android;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.os.Build;

public class AlarmReceiver extends BroadcastReceiver {
    @Override public void onReceive(Context context, Intent intent) {
        Intent alarm = new Intent(context, ConversationService.class).setAction(ConversationService.ACTION_ALARM);
        if (Build.VERSION.SDK_INT >= 26) context.startForegroundService(alarm); else context.startService(alarm);
    }
}
