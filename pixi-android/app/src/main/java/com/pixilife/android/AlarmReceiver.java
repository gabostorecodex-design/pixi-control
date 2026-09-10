package com.pixilife.android;

import android.app.AlarmManager;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import java.util.Calendar;

public class AlarmReceiver extends BroadcastReceiver {
    @Override public void onReceive(Context context, Intent intent) {
        Intent alarm = new Intent(context, ConversationService.class).setAction(ConversationService.ACTION_ALARM).putExtra("text",intent.getStringExtra("text"));
        if (Build.VERSION.SDK_INT >= 26) context.startForegroundService(alarm); else context.startService(alarm);
        int days=intent.getIntExtra("days",127);
        if(days!=0)schedule(context,intent.getIntExtra("id",1),intent.getIntExtra("hour",8),intent.getIntExtra("minute",0),days,intent.getStringExtra("text"));
    }

    public static void schedule(Context c,int id,int hour,int minute,int days,String text){Calendar now=Calendar.getInstance(),next=(Calendar)now.clone();next.set(Calendar.HOUR_OF_DAY,hour);next.set(Calendar.MINUTE,minute);next.set(Calendar.SECOND,0);next.set(Calendar.MILLISECOND,0);for(int add=0;add<8;add++){if(add>0)next.add(Calendar.DAY_OF_YEAR,1);int bit=1<<(next.get(Calendar.DAY_OF_WEEK)-1);if((days==0||days==127||(days&bit)!=0)&&next.after(now))break;}Intent i=new Intent(c,AlarmReceiver.class).putExtra("id",id).putExtra("hour",hour).putExtra("minute",minute).putExtra("days",days).putExtra("text",text);PendingIntent pi=PendingIntent.getBroadcast(c,id,i,PendingIntent.FLAG_UPDATE_CURRENT|PendingIntent.FLAG_IMMUTABLE);AlarmManager am=(AlarmManager)c.getSystemService(Context.ALARM_SERVICE);try{if(Build.VERSION.SDK_INT>=23)am.setExactAndAllowWhileIdle(AlarmManager.RTC_WAKEUP,next.getTimeInMillis(),pi);else am.setExact(AlarmManager.RTC_WAKEUP,next.getTimeInMillis(),pi);}catch(SecurityException denied){if(Build.VERSION.SDK_INT>=23)am.setAndAllowWhileIdle(AlarmManager.RTC_WAKEUP,next.getTimeInMillis(),pi);else am.set(AlarmManager.RTC_WAKEUP,next.getTimeInMillis(),pi);}}
    public static void cancel(Context c,int id){AlarmManager am=(AlarmManager)c.getSystemService(Context.ALARM_SERVICE);PendingIntent pi=PendingIntent.getBroadcast(c,id,new Intent(c,AlarmReceiver.class),PendingIntent.FLAG_NO_CREATE|PendingIntent.FLAG_IMMUTABLE);if(pi!=null)am.cancel(pi);}
    public static void scheduleAt(Context c,int id,long when,String text){Intent i=new Intent(c,AlarmReceiver.class).putExtra("id",id).putExtra("days",0).putExtra("text",text);PendingIntent pi=PendingIntent.getBroadcast(c,id,i,PendingIntent.FLAG_UPDATE_CURRENT|PendingIntent.FLAG_IMMUTABLE);AlarmManager am=(AlarmManager)c.getSystemService(Context.ALARM_SERVICE);try{if(Build.VERSION.SDK_INT>=23)am.setExactAndAllowWhileIdle(AlarmManager.RTC_WAKEUP,when,pi);else am.setExact(AlarmManager.RTC_WAKEUP,when,pi);}catch(SecurityException e){am.set(AlarmManager.RTC_WAKEUP,when,pi);}}
}
