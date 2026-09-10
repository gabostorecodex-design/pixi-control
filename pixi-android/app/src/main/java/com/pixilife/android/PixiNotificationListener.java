package com.pixilife.android;
import android.service.notification.*;import android.content.*;
public class PixiNotificationListener extends NotificationListenerService {
 @Override public void onNotificationPosted(StatusBarNotification n){if(!getSharedPreferences("pixi",MODE_PRIVATE).getBoolean("notification_reading",false))return;Intent i=new Intent(ConversationService.EVENT).setPackage(getPackageName()).putExtra("type","notification").putExtra("value",n.getPackageName());sendBroadcast(i);}
}
