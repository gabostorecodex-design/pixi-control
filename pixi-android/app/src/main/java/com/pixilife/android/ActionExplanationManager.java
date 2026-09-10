package com.pixilife.android;

import android.content.ContentValues;
import android.content.Context;

public final class ActionExplanationManager {
    private final MemoryDatabase database;
    public ActionExplanationManager(Context context){database=new MemoryDatabase(context.getApplicationContext());}
    public void record(String source,String request,String skill,String rule,String data,String provider,String result,String error){ContentValues v=new ContentValues();v.put("created_at",System.currentTimeMillis());v.put("source",source);v.put("request",request);v.put("skill",skill);v.put("rule_name",rule);v.put("data_used",data);v.put("provider",provider);v.put("result",result);v.put("error",error);database.getWritableDatabase().insert("actions",null,v);}
    public String explainLast(){try(android.database.Cursor c=database.getReadableDatabase().query("actions",null,null,null,null,null,"created_at DESC","1")){if(!c.moveToFirst())return "Todavía no tengo una acción registrada.";String request=c.getString(c.getColumnIndexOrThrow("request")),skill=c.getString(c.getColumnIndexOrThrow("skill")),result=c.getString(c.getColumnIndexOrThrow("result"));return "Recibí '"+request+"', usé "+skill+" y el resultado fue "+result+".";}}
}
