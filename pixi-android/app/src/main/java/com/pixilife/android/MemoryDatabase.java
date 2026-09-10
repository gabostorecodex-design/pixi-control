package com.pixilife.android;

import android.content.Context;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;

public final class MemoryDatabase extends SQLiteOpenHelper {
    public static final int VERSION=1;
    public MemoryDatabase(Context context){super(context,"pixi_memory.db",null,VERSION);}
    @Override public void onConfigure(SQLiteDatabase db){db.setForeignKeyConstraintsEnabled(true);db.enableWriteAheadLogging();}
    @Override public void onCreate(SQLiteDatabase db){
        db.execSQL("CREATE TABLE memories(id INTEGER PRIMARY KEY AUTOINCREMENT,type TEXT NOT NULL,content TEXT NOT NULL,important INTEGER NOT NULL DEFAULT 0,created_at INTEGER NOT NULL,updated_at INTEGER NOT NULL)");
        db.execSQL("CREATE INDEX idx_memories_type ON memories(type)");
        db.execSQL("CREATE TABLE actions(id INTEGER PRIMARY KEY AUTOINCREMENT,created_at INTEGER NOT NULL,source TEXT,request TEXT,skill TEXT,rule_name TEXT,data_used TEXT,provider TEXT,result TEXT,error TEXT)");
        db.execSQL("CREATE TABLE settings(key TEXT PRIMARY KEY,value TEXT NOT NULL,updated_at INTEGER NOT NULL)");
        db.execSQL("CREATE TABLE reminders(id INTEGER PRIMARY KEY AUTOINCREMENT,text TEXT NOT NULL,due_at INTEGER NOT NULL,repeat_rule TEXT,status TEXT NOT NULL,created_at INTEGER NOT NULL)");
        db.execSQL("CREATE TABLE tasks(id INTEGER PRIMARY KEY AUTOINCREMENT,text TEXT NOT NULL,done INTEGER NOT NULL DEFAULT 0,created_at INTEGER NOT NULL,updated_at INTEGER NOT NULL)");
    }
    @Override public void onUpgrade(SQLiteDatabase db,int oldVersion,int newVersion){}
}
