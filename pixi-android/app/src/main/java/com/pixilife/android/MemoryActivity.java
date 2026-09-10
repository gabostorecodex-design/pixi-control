package com.pixilife.android;

import android.app.Activity;
import android.graphics.Color;
import android.os.Bundle;
import android.widget.*;
import org.json.JSONArray;
import org.json.JSONObject;

public class MemoryActivity extends Activity {
    private MemoryManager manager; private LinearLayout list; private EditText search;
    @Override public void onCreate(Bundle state){super.onCreate(state);manager=new MemoryManager(this);LinearLayout root=new LinearLayout(this);root.setOrientation(LinearLayout.VERTICAL);root.setPadding(24,24,24,24);root.setBackgroundColor(Color.rgb(7,11,17));TextView title=new TextView(this);title.setText("Memoria de Pixi");title.setTextSize(26);title.setTextColor(Color.WHITE);root.addView(title);search=new EditText(this);search.setHint("Buscar recuerdos");search.setTextColor(Color.WHITE);search.setHintTextColor(Color.GRAY);root.addView(search);Button find=new Button(this);find.setText("Buscar");find.setOnClickListener(v->render(search.getText().toString()));root.addView(find);ScrollView scroll=new ScrollView(this);list=new LinearLayout(this);list.setOrientation(LinearLayout.VERTICAL);scroll.addView(list);root.addView(scroll,new LinearLayout.LayoutParams(-1,0,1));setContentView(root);render("");}
    private void render(String term){list.removeAllViews();JSONArray memories=term.trim().isEmpty()?manager.listMemories():manager.searchMemory(term.trim());for(int i=0;i<memories.length();i++){JSONObject memory=memories.optJSONObject(i);if(memory==null)continue;LinearLayout row=new LinearLayout(this);row.setOrientation(LinearLayout.VERTICAL);TextView text=new TextView(this);text.setText((memory.optBoolean("important")?"IMPORTANTE: ":"")+memory.optString("content"));text.setTextColor(Color.WHITE);row.addView(text);Button delete=new Button(this);delete.setText("Eliminar");long id=memory.optLong("id");delete.setOnClickListener(v->{manager.deleteMemory(id);render(search.getText().toString());});row.addView(delete);list.addView(row);}}
}
