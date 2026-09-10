package com.pixilife.android;

import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import org.json.JSONArray;
import org.json.JSONObject;

public final class MemoryManager {
    public static final String SESSION_MEMORY="session",SHORT_TERM_MEMORY="short",LONG_TERM_MEMORY="long",IMPORTANT_MEMORY="important",SYSTEM_MEMORY="system";
    private final MemoryDatabase database;private final Context context;
    public MemoryManager(Context context){this.context=context.getApplicationContext();database=new MemoryDatabase(this.context);}
    public long saveMemory(String type,String content,boolean important){long now=System.currentTimeMillis();ContentValues v=new ContentValues();v.put("type",type);v.put("content",content.trim());v.put("important",important?1:0);v.put("created_at",now);v.put("updated_at",now);return database.getWritableDatabase().insertOrThrow("memories",null,v);}
    public JSONObject getMemory(long id){try(Cursor c=database.getReadableDatabase().query("memories",null,"id=?",new String[]{String.valueOf(id)},null,null,null)){return c.moveToFirst()?row(c):null;}}
    public JSONArray searchMemory(String query){return query("content LIKE ?",new String[]{"%"+query+"%"});}
    public JSONArray listMemories(){return query(null,null);}
    public boolean updateMemory(long id,String content,boolean important){ContentValues v=new ContentValues();v.put("content",content.trim());v.put("important",important?1:0);v.put("updated_at",System.currentTimeMillis());return database.getWritableDatabase().update("memories",v,"id=?",new String[]{String.valueOf(id)})>0;}
    public boolean deleteMemory(long id){return database.getWritableDatabase().delete("memories","id=?",new String[]{String.valueOf(id)})>0;}
    public String exportMemory(){return listMemories().toString();}
    public int importMemory(String json)throws Exception{JSONArray a=new JSONArray(json);int count=0;SQLiteDatabase db=database.getWritableDatabase();db.beginTransaction();try{for(int i=0;i<a.length();i++){JSONObject o=a.getJSONObject(i);saveMemory(o.optString("type",LONG_TERM_MEMORY),o.getString("content"),o.optBoolean("important"));count++;}db.setTransactionSuccessful();}finally{db.endTransaction();}return count;}
    public String context(int limit){String last=context.getSharedPreferences("pixi",Context.MODE_PRIVATE).getString("last_user_text","").toLowerCase();java.util.LinkedHashMap<Long,JSONObject> selected=new java.util.LinkedHashMap<>();for(String word:last.split("[^\\p{L}\\p{N}]+")){if(word.length()<4)continue;JSONArray found=searchMemory(word);for(int i=0;i<found.length()&&selected.size()<limit;i++){JSONObject o=found.optJSONObject(i);if(o!=null)selected.put(o.optLong("id"),o);}}JSONArray recent=query(null,null,"important DESC,updated_at DESC",limit);for(int i=0;i<recent.length()&&selected.size()<limit;i++){JSONObject o=recent.optJSONObject(i);if(o!=null)selected.put(o.optLong("id"),o);}StringBuilder s=new StringBuilder(new PersonalityManager(context).profile()).append('\n');for(JSONObject o:selected.values())s.append("- ").append(o.optString("content")).append('\n');return s.toString();}
    private JSONArray query(String where,String[] args){return query(where,args,"important DESC,updated_at DESC",100);}
    private JSONArray query(String where,String[] args,String order,int limit){JSONArray out=new JSONArray();try(Cursor c=database.getReadableDatabase().query("memories",null,where,args,null,null,order,String.valueOf(limit))){while(c.moveToNext())out.put(row(c));}return out;}
    private JSONObject row(Cursor c){JSONObject o=new JSONObject();try{o.put("id",c.getLong(c.getColumnIndexOrThrow("id")));o.put("type",c.getString(c.getColumnIndexOrThrow("type")));o.put("content",c.getString(c.getColumnIndexOrThrow("content")));o.put("important",c.getInt(c.getColumnIndexOrThrow("important"))==1);o.put("createdAt",c.getLong(c.getColumnIndexOrThrow("created_at")));o.put("updatedAt",c.getLong(c.getColumnIndexOrThrow("updated_at")));}catch(Exception ignored){}return o;}
}
