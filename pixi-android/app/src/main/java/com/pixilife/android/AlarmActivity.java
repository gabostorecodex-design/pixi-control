package com.pixilife.android;

import android.app.Activity;
import android.graphics.Color;
import android.os.Bundle;
import android.view.View;
import android.widget.*;
import java.util.Locale;

public class AlarmActivity extends Activity {
    private TimePicker time; private EditText text; private final CheckBox[] days=new CheckBox[7];
    @Override public void onCreate(Bundle b){super.onCreate(b);LinearLayout root=new LinearLayout(this);root.setOrientation(LinearLayout.VERTICAL);root.setPadding(30,30,30,30);root.setBackgroundColor(Color.rgb(7,11,17));TextView title=new TextView(this);title.setText("Alarmas de Pixi");title.setTextSize(27);title.setTextColor(Color.WHITE);root.addView(title);time=new TimePicker(this);time.setIs24HourView(true);root.addView(time);text=new EditText(this);text.setHint("Mensaje de la alarma");text.setTextColor(Color.WHITE);text.setHintTextColor(Color.GRAY);root.addView(text);LinearLayout row=new LinearLayout(this);String[] names={"D","L","M","X","J","V","S"};for(int i=0;i<7;i++){days[i]=new CheckBox(this);days[i].setText(names[i]);days[i].setTextColor(Color.WHITE);row.addView(days[i]);}root.addView(row);Button save=new Button(this);save.setText("Guardar o actualizar");save.setOnClickListener(v->save());root.addView(save);Button delete=new Button(this);delete.setText("Eliminar alarma");delete.setOnClickListener(v->{AlarmReceiver.cancel(this,22622);getSharedPreferences("alarms",MODE_PRIVATE).edit().clear().apply();Toast.makeText(this,"Alarma eliminada",Toast.LENGTH_SHORT).show();});root.addView(delete);setContentView(root);load();}
    private void load(){android.content.SharedPreferences p=getSharedPreferences("alarms",MODE_PRIVATE);time.setHour(p.getInt("hour",8));time.setMinute(p.getInt("minute",0));text.setText(p.getString("text","Alarma. Despierta."));int mask=p.getInt("days",127);for(int i=0;i<7;i++)days[i].setChecked((mask&(1<<i))!=0);}
    private void save(){int mask=0;for(int i=0;i<7;i++)if(days[i].isChecked())mask|=1<<i;if(mask==0)mask=127;String message=text.getText().toString().trim();if(message.isEmpty())message="Alarma. Despierta.";getSharedPreferences("alarms",MODE_PRIVATE).edit().putInt("hour",time.getHour()).putInt("minute",time.getMinute()).putInt("days",mask).putString("text",message).apply();AlarmReceiver.schedule(this,22622,time.getHour(),time.getMinute(),mask,message);Toast.makeText(this,String.format(Locale.US,"Alarma %02d:%02d guardada",time.getHour(),time.getMinute()),Toast.LENGTH_SHORT).show();}
}
