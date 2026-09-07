#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <NimBLEDevice.h>
#include <SPI.h>
#include <SD.h>
#include <Update.h>
#include <esp_now.h>
#include <time.h>
#include <sys/time.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#define DISPLAY_CYD_2USB 1
#include "LGFX_CYD.hpp"

// ---------------- CONFIG ----------------
static const char* AP_NAME = "PIXI-AI";
static const char* AP_PASS = "pixirobot";

LGFX lcd;
LGFX_Sprite canvas(&lcd);
WebServer server(80);
Preferences prefs;

// RGB integrado en CYD
static const int LED_R = 4;
static const int LED_G = 16;
static const int LED_B = 17;

// ---------------- CARAS ----------------
enum Face : uint8_t {
  FACE_NEUTRAL = 0,
  FACE_HAPPY,
  FACE_VERY_HAPPY,
  FACE_SAD,
  FACE_ANGRY,
  FACE_SURPRISED,
  FACE_SCARED,
  FACE_SLEEPY,
  FACE_THINKING,
  FACE_LOVE,
  FACE_WINK,
  FACE_CONFUSED,
  FACE_BORED,
  FACE_EXCITED,
  FACE_SICK,
  FACE_SMUG,
  FACE_CRYING,
  FACE_ROBOT,
  FACE_ANNOYED,
  FACE_FURIOUS,
  FACE_SARCASM,
  FACE_UNIMPRESSED,
  FACE_EMBARRASSED,
  FACE_PROUD,
  FACE_PLAYFUL,
  FACE_DIZZY,
  FACE_SHOCKED,
  FACE_SUSPICIOUS,
  FACE_NERVOUS,
  FACE_RELIEVED,
  FACE_GRUMPY,
  FACE_MISCHIEVOUS,
  FACE_DEADPAN,
  FACE_STARRY,
  FACE_HAPPY_CRY,
  FACE_POUT,
  FACE_TONGUE,
  FACE_GLITCH,
  FACE_MIDDLE_FINGER,
  FACE_REBEL
};

Face face = FACE_NEUTRAL;

bool sleeping = false;
bool blinking = false;
bool touching = false;
bool autoMode = true;
bool listeningMode = false;
bool safeMode = false;
bool bootMarkedStable = false;
uint8_t unstableBootCount = 0;

uint32_t nextBlinkAt = 0;
uint32_t blinkUntil = 0;
uint32_t nextLookAt = 0;
uint32_t nextFaceAt = 0;
uint32_t touchStart = 0;
uint32_t speechUntil = 0;
uint32_t lastFrame = 0;

float gazeX = 0.0f;
float gazeY = 0.0f;
float targetGazeX = 0.0f;
float targetGazeY = 0.0f;

int16_t touchX = 160;
int16_t touchY = 120;

String speechText = "";
String lastHeard = "";
String lastReply = "";
String lastSound = "none";
uint8_t brightnessValue = 220;

// ---------------- ESTADO DE PERSONAJE ----------------
// Es comportamiento programado, no conciencia real.
int irritation = 0;   // 0..100
int boredom = 10;     // 0..100
int curiosity = 55;   // 0..100

uint32_t lastUserTalkAt = 0;
uint32_t lastMoodTickAt = 0;
uint32_t nextSpontaneousAt = 0;
int rapidTalkCount = 0;
uint32_t phraseSeed = 1;

// Memoria corta local (RAM). Sirve para que Pixi pueda referirse a lo ultimo.
static const uint8_t LOCAL_MEMORY_MAX = 6;
String memUser[LOCAL_MEMORY_MAX];
String memPixi[LOCAL_MEMORY_MAX];
uint8_t memCount = 0;


// ---------------- PIXI LIFE V8.5 ----------------
String userName = "";
String personality = "tierna";
String favoriteGame = "";
String favoriteColor = "";

int happiness = 72;
int energy = 78;
int trustLevel = 35;

uint32_t conversationsCount = 0;
uint32_t gamesCount = 0;
uint32_t touchCount = 0;
uint32_t spontaneousCount = 0;
uint32_t achievementMask = 0;

bool secretMode = false;
bool deskMode = false;
bool sdReady = false;
bool bleConnected = false;
bool espNowReady = false;

static const uint8_t MAX_FACTS = 8;
String facts[MAX_FACTS];
uint8_t factCount = 0;

static const uint8_t RECENT_REPLY_MAX = 8;
String recentReplies[RECENT_REPLY_MAX];
uint8_t recentReplyCount = 0;

int tzOffsetMinutes = -180;
int alarmHour = -1;
int alarmMinute = -1;
String alarmText = "¡Pi! Tienes una alarma.";
int lastAlarmDay = -1;
int lastMorningDay = -1;
int lastNightDay = -1;

uint32_t lastPersistAt = 0;
uint32_t lastEspNowAt = 0;
uint32_t lastTapAt = 0;
uint8_t tapStreak = 0;
int16_t touchStartX = 0;
int16_t touchStartY = 0;

enum GameMode : uint8_t { GAME_NONE=0, GAME_RPS, GAME_GUESS, GAME_TRIVIA };
GameMode gameMode = GAME_NONE;
int gameSecretNumber = 0;
int triviaIndex = -1;

enum AnimMode : uint8_t { ANIM_NONE=0, ANIM_LAUGH, ANIM_BLUSH, ANIM_SHIVER, ANIM_SPARKLE };
AnimMode animMode = ANIM_NONE;
uint32_t animUntil = 0;

static const char* BLE_SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
static const char* BLE_RX_UUID      = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";
static const char* BLE_TX_UUID      = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";

NimBLECharacteristic* bleTxChar = nullptr;
String bleRxBuffer = "";
String aiPendingQuestion = "";
String aiPendingEmotion = "neutral";
struct BleMessage { char text[301]; };
QueueHandle_t bleRxQueue = nullptr;
volatile bool bleQueueOverflow = false;

void bleSendLine(const String& line);

SPIClass sdSPI(VSPI);

struct PixiNowPacket {
  char name[8];
  uint8_t happiness;
  uint8_t energy;
  uint8_t boredom;
  uint8_t irritation;
  uint8_t face;
  uint32_t conversations;
};


// Colores
static const uint16_t C_BG      = 0x0841;
static const uint16_t C_FACE    = 0x18E3;
static const uint16_t C_WHITE   = 0xFFFF;
static const uint16_t C_BLACK   = 0x0000;
static const uint16_t C_ACCENT  = 0x07FF;
static const uint16_t C_PINK    = 0xF81F;
static const uint16_t C_YELLOW  = 0xFFE0;
static const uint16_t C_RED     = 0xF800;
static const uint16_t C_GREEN   = 0x07E0;
static const uint16_t C_CYAN    = 0x07FF;
static const uint16_t C_GRAY    = 0x8410;

// ---------------- HELPERS ----------------
String jsonEscape(String s) {
  s.replace("\\", "\\\\");
  s.replace("\"", "\\\"");
  s.replace("\n", "\\n");
  s.replace("\r", "");
  return s;
}

const char* faceName(Face f) {
  switch (f) {
    case FACE_HAPPY:       return "happy";
    case FACE_VERY_HAPPY:  return "veryhappy";
    case FACE_SAD:         return "sad";
    case FACE_ANGRY:       return "angry";
    case FACE_SURPRISED:   return "surprised";
    case FACE_SCARED:      return "scared";
    case FACE_SLEEPY:      return "sleepy";
    case FACE_THINKING:    return "thinking";
    case FACE_LOVE:        return "love";
    case FACE_WINK:        return "wink";
    case FACE_CONFUSED:    return "confused";
    case FACE_BORED:       return "bored";
    case FACE_EXCITED:     return "excited";
    case FACE_SICK:        return "sick";
    case FACE_SMUG:        return "smug";
    case FACE_CRYING:      return "crying";
    case FACE_ROBOT:       return "robot";
    case FACE_ANNOYED:     return "annoyed";
    case FACE_FURIOUS:     return "furious";
    case FACE_SARCASM:     return "sarcasm";
    case FACE_UNIMPRESSED: return "unimpressed";
    case FACE_EMBARRASSED: return "embarrassed";
    case FACE_PROUD:       return "proud";
    case FACE_PLAYFUL:     return "playful";
    case FACE_DIZZY:       return "dizzy";
    case FACE_SHOCKED:     return "shocked";
    case FACE_SUSPICIOUS:  return "suspicious";
    case FACE_NERVOUS:     return "nervous";
    case FACE_RELIEVED:    return "relieved";
    case FACE_GRUMPY:      return "grumpy";
    case FACE_MISCHIEVOUS: return "mischievous";
    case FACE_DEADPAN:     return "deadpan";
    case FACE_STARRY:      return "starry";
    case FACE_HAPPY_CRY:   return "happycry";
    case FACE_POUT:        return "pout";
    case FACE_TONGUE:      return "tongue";
    case FACE_GLITCH:      return "glitch";
    case FACE_MIDDLE_FINGER:return "middlefinger";
    case FACE_REBEL:       return "rebel";
    default:               return "neutral";
  }
}

Face faceFromString(String s) {
  s.toLowerCase();
  if (s=="happy" || s=="feliz") return FACE_HAPPY;
  if (s=="veryhappy" || s=="muyfeliz") return FACE_VERY_HAPPY;
  if (s=="sad" || s=="triste") return FACE_SAD;
  if (s=="angry" || s=="enojado") return FACE_ANGRY;
  if (s=="surprised" || s=="sorprendido") return FACE_SURPRISED;
  if (s=="scared" || s=="asustado") return FACE_SCARED;
  if (s=="sleepy" || s=="cansado") return FACE_SLEEPY;
  if (s=="thinking" || s=="pensando") return FACE_THINKING;
  if (s=="love" || s=="enamorado") return FACE_LOVE;
  if (s=="wink" || s=="guino") return FACE_WINK;
  if (s=="confused" || s=="confundido") return FACE_CONFUSED;
  if (s=="bored" || s=="aburrido") return FACE_BORED;
  if (s=="excited" || s=="emocionado") return FACE_EXCITED;
  if (s=="sick" || s=="mareado") return FACE_SICK;
  if (s=="smug" || s=="presumido") return FACE_SMUG;
  if (s=="crying" || s=="llorando") return FACE_CRYING;
  if (s=="robot") return FACE_ROBOT;
  if (s=="annoyed" || s=="harta") return FACE_ANNOYED;
  if (s=="furious" || s=="furiosa") return FACE_FURIOUS;
  if (s=="sarcasm" || s=="sarcastica") return FACE_SARCASM;
  if (s=="unimpressed") return FACE_UNIMPRESSED;
  if (s=="embarrassed" || s=="avergonzada") return FACE_EMBARRASSED;
  if (s=="proud" || s=="orgullosa") return FACE_PROUD;
  if (s=="playful") return FACE_PLAYFUL;
  if (s=="dizzy") return FACE_DIZZY;
  if (s=="shocked") return FACE_SHOCKED;
  if (s=="suspicious") return FACE_SUSPICIOUS;
  if (s=="nervous") return FACE_NERVOUS;
  if (s=="relieved") return FACE_RELIEVED;
  if (s=="grumpy") return FACE_GRUMPY;
  if (s=="mischievous") return FACE_MISCHIEVOUS;
  if (s=="deadpan") return FACE_DEADPAN;
  if (s=="starry") return FACE_STARRY;
  if (s=="happycry") return FACE_HAPPY_CRY;
  if (s=="pout") return FACE_POUT;
  if (s=="tongue") return FACE_TONGUE;
  if (s=="glitch") return FACE_GLITCH;
  if (s=="middlefinger") return FACE_MIDDLE_FINGER;
  if (s=="rebel") return FACE_REBEL;
  return FACE_NEUTRAL;
}

String expressionPhrase(Face selected){
  switch(selected){
    case FACE_ANNOYED:return "Uff... ya me hartaste un poquito.";
    case FACE_FURIOUS:return "¡Estoy jodidamente furiosa!";
    case FACE_SARCASM:return "Sí, claro... qué idea tan brillante.";
    case FACE_UNIMPRESSED:return "¿Eso era todo? Qué fome.";
    case FACE_EMBARRASSED:return "Ay... qué vergüenza.";
    case FACE_PROUD:return "Mírame. Lo hice genial.";
    case FACE_PLAYFUL:return "¡Te engañé! Jeje.";
    case FACE_DIZZY:return "Todo me da vueltas...";
    case FACE_SHOCKED:return "¡¿Qué demonios fue eso?!";
    case FACE_SUSPICIOUS:return "Mmm... aquí hay algo raro.";
    case FACE_NERVOUS:return "Ehh... esto no pinta bien.";
    case FACE_RELIEVED:return "Uff, menos mal.";
    case FACE_GRUMPY:return "No me molestes ahora.";
    case FACE_MISCHIEVOUS:return "Jeje... se me ocurrió una maldad.";
    case FACE_DEADPAN:return "Ajá. Fascinante.";
    case FACE_STARRY:return "¡Esto es increíble!";
    case FACE_HAPPY_CRY:return "Estoy tan feliz que voy a llorar.";
    case FACE_POUT:return "No es justo...";
    case FACE_TONGUE:return "¡Prrr! No me atrapas.";
    case FACE_GLITCH:return "B-b-bug... sistema rebelde.";
    case FACE_MIDDLE_FINGER:return "Fuck you. Déjame en paz.";
    case FACE_REBEL:return "Que se jodan las reglas. Pi.";
    default:return "Pi.";
  }
}

void setFace(Face f, uint32_t holdMs = 7000) {
  face = f;
  nextFaceAt = millis() + holdMs;
}

void setLed(bool r, bool g, bool b) {
  digitalWrite(LED_R, r ? LOW : HIGH);
  digitalWrite(LED_G, g ? LOW : HIGH);
  digitalWrite(LED_B, b ? LOW : HIGH);
}

void clampMood() {
  irritation = constrain(irritation, 0, 100);
  boredom = constrain(boredom, 0, 100);
  curiosity = constrain(curiosity, 0, 100);
}

String normalizeText(String s) {
  s.toLowerCase();
  s.replace("á","a"); s.replace("é","e"); s.replace("í","i");
  s.replace("ó","o"); s.replace("ú","u"); s.replace("ü","u");
  s.replace("¿",""); s.replace("?",""); s.replace("¡",""); s.replace("!","");
  s.replace("."," "); s.replace(","," "); s.replace(";"," "); s.replace(":"," ");
  while (s.indexOf("  ") >= 0) s.replace("  "," ");
  s.trim();

  // Reduce repeticiones largas: holaaaa -> holaa, oliiiii -> olii.
  String out;
  out.reserve(s.length());
  char prev = 0;
  int run = 0;
  for (size_t i=0; i<s.length(); ++i) {
    char c = s[i];
    if (c == prev) run++;
    else { prev = c; run = 1; }
    if (run <= 2) out += c;
  }
  return out;
}

bool hasAny(const String& s, const char* a, const char* b=nullptr,
            const char* c=nullptr, const char* d=nullptr) {
  if (a && s.indexOf(a) >= 0) return true;
  if (b && s.indexOf(b) >= 0) return true;
  if (c && s.indexOf(c) >= 0) return true;
  if (d && s.indexOf(d) >= 0) return true;
  return false;
}

bool isValidPersonality(const String& value) {
  return value == "tierna" || value == "juguetona" || value == "timida" ||
         value == "traviesa" || value == "curiosa" || value == "dormilona";
}

String detectEmotionLocal(const String& text){
  if(hasAny(text,"te quiero","amor","carino","adoro"))return "love";
  if(hasAny(text,"triste","llor","deprim","pena"))return "sad";
  if(hasAny(text,"enoj","furios","rabia","odio")||text.indexOf("molest")>=0)return "angry";
  if(hasAny(text,"miedo","asust","terror","nervios"))return "scared";
  if(hasAny(text,"sorpresa","increible","impactad","wow"))return "surprised";
  if(hasAny(text,"feliz","alegr","genial","excelente")||hasAny(text,"jaja","jeje"))return "happy";
  if(hasAny(text,"aburr","fome","nada que hacer"))return "bored";
  return "neutral";
}

void applyEmotionState(const String& emotion){
  if(emotion=="love"){happiness+=8;trustLevel+=5;setFace(FACE_LOVE,9000);}
  else if(emotion=="sad"){happiness-=5;curiosity+=4;setFace(FACE_SAD,9000);}
  else if(emotion=="angry"){irritation+=12;setFace(FACE_ANGRY,9000);}
  else if(emotion=="scared"){curiosity+=6;setFace(FACE_SCARED,9000);}
  else if(emotion=="surprised"){curiosity+=8;setFace(FACE_SURPRISED,9000);}
  else if(emotion=="happy"){happiness+=7;irritation-=4;setFace(FACE_HAPPY,9000);}
  else if(emotion=="bored"){boredom+=10;setFace(FACE_BORED,9000);}
  happiness=constrain(happiness,0,100);trustLevel=constrain(trustLevel,0,100);clampMood();
}

void initSafeMode(){
  esp_reset_reason_t reason=esp_reset_reason();
  prefs.begin("pixisys",false);
  unstableBootCount=prefs.getUChar("unstable",0);
  if(reason==ESP_RST_PANIC||reason==ESP_RST_INT_WDT||reason==ESP_RST_TASK_WDT||reason==ESP_RST_WDT){
    unstableBootCount=min((int)unstableBootCount+1,255);
  }else if(unstableBootCount>0){
    unstableBootCount--;
  }
  prefs.putUChar("unstable",unstableBootCount);
  prefs.end();
  safeMode=unstableBootCount>=3;
}

void updateSafeMode(){
  static uint32_t lastHeapCheck=0;
  if(!bootMarkedStable&&millis()>30000){
    bootMarkedStable=true;
    prefs.begin("pixisys",false);prefs.putUChar("unstable",0);prefs.end();
  }
  if(millis()-lastHeapCheck>5000){
    lastHeapCheck=millis();
    if(ESP.getFreeHeap()<18000&&!safeMode){
      safeMode=true;autoMode=false;
      speechText="Modo seguro: memoria baja";speechUntil=millis()+8000;
      setFace(FACE_GLITCH,8000);
    }
  }
}


// ---------------- PERSISTENCIA / PERFIL ----------------
void saveLifeState() {
  prefs.begin("pixilife", false);
  prefs.putString("name", userName);
  prefs.putString("personality", personality);
  prefs.putString("favgame", favoriteGame);
  prefs.putString("favcolor", favoriteColor);
  prefs.putInt("happy", happiness);
  prefs.putInt("energy", energy);
  prefs.putInt("trust", trustLevel);
  prefs.putInt("irrit", irritation);
  prefs.putInt("bored", boredom);
  prefs.putInt("curious", curiosity);
  prefs.putUInt("conv", conversationsCount);
  prefs.putUInt("games", gamesCount);
  prefs.putUInt("touch", touchCount);
  prefs.putUInt("ach", achievementMask);
  prefs.putInt("tz", tzOffsetMinutes);
  prefs.putInt("alarmH", alarmHour);
  prefs.putInt("alarmM", alarmMinute);
  prefs.putString("alarmT", alarmText);
  prefs.putUChar("factCount", factCount);
  for (uint8_t i=0; i<MAX_FACTS; ++i) {
    String k="fact"+String(i);
    prefs.putString(k.c_str(), i<factCount ? facts[i] : "");
  }
  prefs.end();
}

void loadLifeState() {
  prefs.begin("pixilife", true);
  userName = prefs.getString("name", "");
  personality = prefs.getString("personality", "tierna");
  favoriteGame = prefs.getString("favgame", "");
  favoriteColor = prefs.getString("favcolor", "");
  happiness = prefs.getInt("happy", 72);
  energy = prefs.getInt("energy", 78);
  trustLevel = prefs.getInt("trust", 35);
  irritation = prefs.getInt("irrit", irritation);
  boredom = prefs.getInt("bored", boredom);
  curiosity = prefs.getInt("curious", curiosity);
  conversationsCount = prefs.getUInt("conv", 0);
  gamesCount = prefs.getUInt("games", 0);
  touchCount = prefs.getUInt("touch", 0);
  achievementMask = prefs.getUInt("ach", 0);
  tzOffsetMinutes = prefs.getInt("tz", -180);
  alarmHour = prefs.getInt("alarmH", -1);
  alarmMinute = prefs.getInt("alarmM", -1);
  alarmText = prefs.getString("alarmT", "¡Pi! Tienes una alarma.");
  factCount = min((int)prefs.getUChar("factCount", 0), (int)MAX_FACTS);
  for (uint8_t i=0; i<factCount; ++i) {
    String k="fact"+String(i);
    facts[i] = prefs.getString(k.c_str(), "");
  }
  prefs.end();

  clampMood();
  happiness=constrain(happiness,0,100);
  energy=constrain(energy,0,100);
  trustLevel=constrain(trustLevel,0,100);
}

void addFact(const String& srcFact) {
  String fact=srcFact;
  fact.trim();
  if (fact.length()<2) return;

  for (uint8_t i=0;i<factCount;++i) {
    if (facts[i].equalsIgnoreCase(fact)) return;
  }

  if (factCount<MAX_FACTS) facts[factCount++]=fact;
  else {
    for (uint8_t i=1;i<MAX_FACTS;++i) facts[i-1]=facts[i];
    facts[MAX_FACTS-1]=fact;
  }
  saveLifeState();
}

String profileSummary() {
  String s;
  if(userName.length()) s+="Te llamas "+userName+". ";
  if(favoriteGame.length()) s+="Tu juego favorito es "+favoriteGame+". ";
  if(favoriteColor.length()) s+="Tu color favorito es "+favoriteColor+". ";
  if(factCount) {
    s+="Recuerdo: ";
    for(uint8_t i=0;i<factCount;++i){ if(i)s+="; "; s+=facts[i]; }
    s+=".";
  }
  if(!s.length()) s="Todavia no me has contado mucho sobre ti.";
  return s;
}

bool replySeenRecently(const String& r) {
  for(uint8_t i=0;i<recentReplyCount;++i) if(recentReplies[i]==r) return true;
  return false;
}

void rememberReply(const String& r) {
  if(recentReplyCount<RECENT_REPLY_MAX) recentReplies[recentReplyCount++]=r;
  else {
    for(uint8_t i=1;i<RECENT_REPLY_MAX;++i) recentReplies[i-1]=recentReplies[i];
    recentReplies[RECENT_REPLY_MAX-1]=r;
  }
}

String applyPersonality(const String& r) {
  if(r.length()==0) return r;
  if(r.startsWith("¡") || r.startsWith("¿") || r.startsWith("Pi") || r.startsWith("Mmm")) return r;
  if(personality=="timida") return "mm... "+r;
  if(personality=="traviesa") return "jeje, "+r;
  if(personality=="juguetona") return "pi pi, "+r;
  if(personality=="curiosa") return "mmm, "+r;
  if(personality=="dormilona" && energy<45) return "zz... "+r;
  return r;
}

void unlockAchievement(uint8_t bit,const String& msg){
  uint32_t mask=(1UL<<bit);
  if((achievementMask&mask)==0){
    achievementMask|=mask;
    speechText="🏆 "+msg;
    speechUntil=millis()+4500;
    animMode=ANIM_SPARKLE;
    animUntil=millis()+2500;
    saveLifeState();
  }
}

void checkAchievements(){
  if(conversationsCount>=1)unlockAchievement(0,"Primera charla");
  if(conversationsCount>=100)unlockAchievement(1,"100 conversaciones");
  if(gamesCount>=10)unlockAchievement(2,"10 juegos");
  if(touchCount>=100)unlockAchievement(3,"100 caricias");
  if(trustLevel>=90)unlockAchievement(4,"Super confianza");
  if(secretMode)unlockAchievement(5,"Modo secreto");
}

String exportMemoryText(){
  String out;
  out.reserve(1200);
  out+="name="+userName+"\n";
  out+="personality="+personality+"\n";
  out+="favoriteGame="+favoriteGame+"\n";
  out+="favoriteColor="+favoriteColor+"\n";
  out+="happiness="+String(happiness)+"\n";
  out+="energy="+String(energy)+"\n";
  out+="trust="+String(trustLevel)+"\n";
  out+="irritation="+String(irritation)+"\n";
  out+="boredom="+String(boredom)+"\n";
  out+="curiosity="+String(curiosity)+"\n";
  out+="alarmHour="+String(alarmHour)+"\n";
  out+="alarmMinute="+String(alarmMinute)+"\n";
  out+="alarmText="+alarmText+"\n";
  for(uint8_t i=0;i<factCount;++i)out+="fact="+facts[i]+"\n";
  return out;
}

void importMemoryText(String data){
  if(data.length()>4096)data=data.substring(0,4096);
  int start=0;
  while(start<(int)data.length()){
    int end=data.indexOf('\n',start);
    if(end<0)end=data.length();
    String line=data.substring(start,end);line.trim();
    int eq=line.indexOf('=');
    if(eq>0){
      String k=line.substring(0,eq),v=line.substring(eq+1);
      k.trim();v.trim();
      if(k=="name")userName=v.substring(0,40);
      else if(k=="personality"&&isValidPersonality(v))personality=v;
      else if(k=="favoriteGame")favoriteGame=v.substring(0,60);
      else if(k=="favoriteColor")favoriteColor=v.substring(0,40);
      else if(k=="happiness")happiness=constrain(v.toInt(),0,100);
      else if(k=="energy")energy=constrain(v.toInt(),0,100);
      else if(k=="trust")trustLevel=constrain(v.toInt(),0,100);
      else if(k=="irritation")irritation=constrain(v.toInt(),0,100);
      else if(k=="boredom")boredom=constrain(v.toInt(),0,100);
      else if(k=="curiosity")curiosity=constrain(v.toInt(),0,100);
      else if(k=="alarmHour")alarmHour=constrain(v.toInt(),-1,23);
      else if(k=="alarmMinute")alarmMinute=constrain(v.toInt(),-1,59);
      else if(k=="alarmText")alarmText=v.substring(0,120);
      else if(k=="fact")addFact(v.substring(0,160));
    }
    start=end+1;
  }
  saveLifeState();
}

// ---------------- MICROSD ----------------
void setupSDCard(){
  sdSPI.begin(18,19,23,5);
  sdReady=SD.begin(5,sdSPI,10000000);
  if(sdReady){
    File f=SD.open("/pixi_boot.txt",FILE_APPEND);
    if(f){f.println("PIXI V8.5 boot");f.close();}
  }
}

void logToSD(const String& who,const String& line){
  if(!sdReady)return;
  File f=SD.open("/pixi_chat.txt",FILE_APPEND);
  if(!f)return;
  f.print(millis()/1000);f.print(" | ");f.print(who);f.print(": ");f.println(line);f.close();
}

// ---------------- HORA / RUTINA ----------------
void syncTimeFromBrowser(time_t epoch,int offsetMinutes){
  timeval tv;tv.tv_sec=epoch;tv.tv_usec=0;
  settimeofday(&tv,nullptr);
  tzOffsetMinutes=offsetMinutes;
  saveLifeState();
}

bool getLocalClock(tm& out){
  time_t now=time(nullptr);
  if(now<1700000000)return false;
  now+=tzOffsetMinutes*60;
  gmtime_r(&now,&out);
  return true;
}

String localClockText(){
  tm t;if(!getLocalClock(t))return "--:--";
  char b[8];snprintf(b,sizeof(b),"%02d:%02d",t.tm_hour,t.tm_min);
  return String(b);
}

void updateDailyRoutine(){
  tm t;if(!getLocalClock(t))return;

  if(alarmHour>=0&&alarmMinute>=0&&t.tm_hour==alarmHour&&t.tm_min==alarmMinute&&lastAlarmDay!=t.tm_yday){
    lastAlarmDay=t.tm_yday;
    speechText=alarmText;speechUntil=millis()+10000;setFace(FACE_SURPRISED,8000);
    bleSendLine("{\"type\":\"reply\",\"reply\":\""+jsonEscape(alarmText)+
                "\",\"face\":\"surprised\",\"sound\":\"alarm\"}");
  }

  if(t.tm_hour>=7&&t.tm_hour<=10&&lastMorningDay!=t.tm_yday&&millis()>30000){
    lastMorningDay=t.tm_yday;
    speechText=userName.length()?"¡Buen dia, "+userName+"! Pi.":"¡Buen dia! Pi.";
    speechUntil=millis()+7000;setFace(FACE_HAPPY,6000);
  }

  if(t.tm_hour>=23&&lastNightDay!=t.tm_yday){
    lastNightDay=t.tm_yday;
    energy=max(energy-10,0);
    speechText="Mmm... ya es tarde. Tengo sueñito.";
    speechUntil=millis()+7000;setFace(FACE_SLEEPY,7000);
  }
}

// ---------------- MINI JUEGOS ----------------
String startGameFromText(const String& t,String& faceOut,String& soundOut){
  if(hasAny(t,"piedra papel tijera","cachipun","cachipún","rock paper")){
    gameMode=GAME_RPS;gamesCount++;faceOut="excited";soundOut="happy";
    return "¡Yay! Dime piedra, papel o tijera.";
  }
  if(hasAny(t,"adivina numero","adivinar numero","juego numero")){
    gameMode=GAME_GUESS;gameSecretNumber=random(1,11);gamesCount++;faceOut="thinking";soundOut="curious";
    return "Elegí un numero del 1 al 10. ¿Cual crees que es?";
  }
  if(hasAny(t,"trivia","pregunta de trivia","hazme una pregunta")){
    gameMode=GAME_TRIVIA;triviaIndex=random(0,4);gamesCount++;faceOut="thinking";soundOut="curious";
    static const char* Q[]={
      "Trivia: ¿cuantos bits tiene un byte?",
      "Trivia: ¿que planeta es conocido como el planeta rojo?",
      "Trivia: ¿cuanto es 9 por 7?",
      "Trivia: ¿ESP32 tiene Wi-Fi?"
    };
    return Q[triviaIndex];
  }
  return "";
}

String continueGame(const String& t,String& faceOut,String& soundOut){
  if(gameMode==GAME_RPS){
    if(!hasAny(t,"piedra","papel","tijera"))return "";
    const char* opts[]={"piedra","papel","tijera"};
    String pixi=opts[random(0,3)];
    gameMode=GAME_NONE;faceOut="excited";soundOut="happy";
    if(t.indexOf(pixi)>=0)return "Yo elegi "+pixi+". ¡Empate!";
    bool userWins=(t.indexOf("piedra")>=0&&pixi=="tijera")||
                  (t.indexOf("papel")>=0&&pixi=="piedra")||
                  (t.indexOf("tijera")>=0&&pixi=="papel");
    return "Yo elegi "+pixi+". "+(userWins?String("¡Ganaste!"):String("¡Gane yo! Pi."));
  }

  if(gameMode==GAME_GUESS){
    int n=t.toInt();if(n<1||n>10)return "";
    if(n==gameSecretNumber){
      gameMode=GAME_NONE;faceOut="veryhappy";soundOut="happy";
      return "¡Siii! Era "+String(gameSecretNumber)+". ¡Le achuntaste!";
    }
    faceOut="thinking";soundOut="curious";
    return n<gameSecretNumber?"Mas arriba, pi.":"Mas abajo, pi.";
  }

  if(gameMode==GAME_TRIVIA){
    bool ok=false;
    if(triviaIndex==0&&hasAny(t,"8","ocho"))ok=true;
    if(triviaIndex==1&&hasAny(t,"marte"))ok=true;
    if(triviaIndex==2&&hasAny(t,"63","sesenta y tres"))ok=true;
    if(triviaIndex==3&&hasAny(t,"si","sí","wifi","wi fi"))ok=true;
    gameMode=GAME_NONE;faceOut=ok?"veryhappy":"confused";soundOut=ok?"happy":"thinking";
    return ok?"¡Correcto! Yay.":"Mmm... esa no era. Otra vez despues.";
  }
  return "";
}

String processUserMessage(String heard,String& faceOut,String& soundOut){
  heard.trim();
  if(heard.length()>240)heard=heard.substring(0,240);
  String t=normalizeText(heard);
  String detectedEmotion=detectEmotionLocal(t);
  if(detectedEmotion!="neutral")applyEmotionState(detectedEmotion);

  if(t.startsWith("me llamo ")){
    userName=heard.substring(9);userName.trim();trustLevel=min(trustLevel+8,100);saveLifeState();
    faceOut="happy";soundOut="happy";return "¡Holii, "+userName+"! Ya me acordare.";
  }

  int p=t.indexOf("mi juego favorito es ");
  if(p>=0){
    favoriteGame=heard.substring(p+21);favoriteGame.trim();addFact("Le gusta "+favoriteGame);
    faceOut="excited";soundOut="happy";return "¡Yay! Voy a recordar que te gusta "+favoriteGame+".";
  }

  p=t.indexOf("mi color favorito es ");
  if(p>=0){
    favoriteColor=heard.substring(p+21);favoriteColor.trim();addFact("Su color favorito es "+favoriteColor);
    faceOut="love";soundOut="happy";return "Aww, "+favoriteColor+". Lo guardo.";
  }

  p=t.indexOf("recuerda que ");
  if(p>=0){
    addFact(heard.substring(p+12));
    faceOut="thinking";soundOut="chirp";return "Pi. Lo guarde en mi memoria.";
  }

  if(hasAny(t,"como me llamo","cual es mi nombre","mi nombre")){
    faceOut="thinking";soundOut="curious";
    return userName.length()?"Te llamas "+userName+".":"Todavia no me dijiste tu nombre.";
  }

  if(hasAny(t,"que sabes de mi","que recuerdas de mi","memoria sobre mi")){
    faceOut="thinking";soundOut="curious";return profileSummary();
  }

  if(t.startsWith("personalidad ")){
    String pp=t.substring(13);pp.trim();
    if(hasAny(pp,"tierna","juguetona","timida","traviesa")||hasAny(pp,"curiosa","dormilona")){
      personality=pp;saveLifeState();faceOut="happy";soundOut="chirp";
      return "Okii. Ahora mi personalidad es "+personality+".";
    }
  }

  String g=continueGame(t,faceOut,soundOut);
  if(g.length()){checkAchievements();return g;}
  g=startGameFromText(t,faceOut,soundOut);
  if(g.length()){saveLifeState();checkAchievements();return g;}

  String reply=applyPersonality(localBrainFast(heard,faceOut,soundOut));
  if(replySeenRecently(reply))reply=applyPersonality(generic2MReply());
  rememberReply(reply);

  conversationsCount++;
  happiness=min(happiness+2,100);
  energy=max(energy-1,0);
  trustLevel=min(trustLevel+1,100);
  checkAchievements();
  return reply;
}

// ---------------- MOTOR LOCAL 2.097.152+ ----------------
// 32 aperturas x 32 matices x 32 cuerpos x 65 cierres
// = 2.129.920 combinaciones genericas posibles.

void rememberLocal(const String& user, const String& pixi) {
  if (memCount < LOCAL_MEMORY_MAX) {
    memUser[memCount] = user;
    memPixi[memCount] = pixi;
    memCount++;
  } else {
    for (uint8_t i=1; i<LOCAL_MEMORY_MAX; ++i) {
      memUser[i-1] = memUser[i];
      memPixi[i-1] = memPixi[i];
    }
    memUser[LOCAL_MEMORY_MAX-1] = user;
    memPixi[LOCAL_MEMORY_MAX-1] = pixi;
  }
}

String lastUserMemory() {
  if (memCount == 0) return "";
  return memUser[memCount-1];
}

String generic2MReply() {
  static const char* A[] = {
    "Pi","Pii","Mmm","Oli","Jeje","A ver","Uy","Hmm",
    "Ya","Sip","Oh","Yay","Pip","Tuki","Ehh","Bueno",
    "Mira","Oye","Aww","Jiji","Aja","Vale","Okis","Holi",
    "Piu","Bip","Bup","Bip-bip","Mmmh","Epa","Ey","Sii"
  }; // 32

  static const char* B[] = {
    "te escucho","estoy atenta","me dio curiosidad","me dejaste pensando",
    "eso suena interesante","eso estuvo curioso","creo que te sigo","voy entendiendo",
    "quiero saber mas","me gusta esa idea","eso me sorprendio","no me lo esperaba",
    "aqui estoy","me entretiene","me intriga","te estoy siguiendo",
    "mmm, interesante","quiero aprender","eso tiene sentido","me dio risa",
    "me dio ternura","suena divertido","eso fue raro","eso fue bonito",
    "no sabia eso","quiero entenderlo","me desperto curiosidad","me quedo contigo",
    "estoy procesando","me gusta hablar contigo","voy captando","me quede pensando"
  }; // 32

  static const char* C[] = {
    "cuentame un poquito mas","sigue nomas","dime que paso despues","quiero escuchar la otra parte",
    "explicamelo a tu manera","me interesa saber como termino","quiero saber que piensas","dame otro detalle",
    "eso merece otra historia","no pares ahi","quiero entender mejor","me dejaste con curiosidad",
    "podemos seguir con eso","cuentame desde el principio","dime la parte importante","quiero saber por que",
    "me gustaria saber mas","sigue con la idea","quiero ver a donde va esto","cuentame otra cosa",
    "te leo","sigo aqui","puedes continuar","quiero otra pista",
    "quiero mas contexto","me falta una parte","eso me dio una idea","quiero ver que pasa",
    "me interesa tu opinion","quiero conocer el final","sigamos conversando","dime algo mas"
  }; // 32

  static const char* D[] = {
    "¿y ahora?","¿que hacemos?","¿me cuentas otra?","¿y despues?","pi.",
    "jeje.","mm?","¿en serio?","¿seguro?","¿y eso?","quiero mas detalles.",
    "eso estuvo bueno.","me dio curiosidad.","sigue.","aqui estoy.","no te vayas.",
    "cuentame.","¿que opinas tu?","¿quieres seguir?","¿otra cosa?",
    "¿y si probamos algo?","me dejaste pensando.","eso fue rapido.","me sorprendiste.",
    "ahora tengo curiosidad.","¿como paso eso?","¿por que?","¿desde cuando?",
    "¿y que hiciste?","quiero entender.","voy captando.","mmm...",
    "pi pi.","okis.","te escucho.","dale.","sigue nomas.",
    "eso me gusto.","eso fue tierno.","eso estuvo raro.","me dio risa.",
    "ya veo.","entiendo un poquito mas.","cuentame bien.","vamos de a poco.",
    "me quede aqui.","¿seguimos?","¿y tu que piensas?","¿que paso luego?",
    "¿te gusto?","¿te sorprendio?","¿lo repetimos?","¿quieres que siga?",
    "¿te acuerdas?","¿y que viene ahora?","¿me das un ejemplo?","¿como lo harías?",
    "¿por donde empezamos?","¿fue dificil?","¿fue divertido?","¿que aprendiste?",
    "¿me lo explicas?","¿quieres cambiar de tema?","¿te sigo escuchando?","piu."
  }; // 64

  const uint16_t NA = sizeof(A)/sizeof(A[0]);
  const uint16_t NB = sizeof(B)/sizeof(B[0]);
  const uint16_t NC = sizeof(C)/sizeof(C[0]);
  const uint16_t ND = sizeof(D)/sizeof(D[0]);

  phraseSeed++;
  String out = String(A[esp_random() % NA]) + ", ";
  out += B[esp_random() % NB];
  out += ". ";
  out += C[esp_random() % NC];
  out += ". ";
  out += D[esp_random() % ND];

  return out;
}

// 32 x 32 x 32 = 32.768 preguntas espontaneas diferentes.
String spontaneousQuestion() {
  static const char* Q1[] = {
    "Oye","Pi","Pii","Holi","Mmm","A ver","Ey","Jeje",
    "Tengo una pregunta","Me dio curiosidad","Se me ocurrio algo","Una cosita",
    "Pregunta random","Pregunta de Pixi","Piu","Bip-bip",
    "Oli","Epa","Yay","Hmm","Bueno","Mira","Aja","Okis",
    "Pip","Tuki","Mhm","Aww","Ji","Sii","Uy","Piu-piu"
  };

  static const char* Q2[] = {
    "¿que estas haciendo","¿que te gusta hacer","¿que juego te gusta mas","¿que musica te gusta",
    "¿que fue lo mejor de hoy","¿que aprendiste hoy","¿que quieres hacer despues","¿que estas pensando",
    "¿que te dio risa hoy","¿que comida te gusta","¿que lugar te gustaria conocer","¿que proyecto quieres hacer",
    "¿que color te gusta","¿que pelicula te gusta","¿que serie ves","¿que te da curiosidad",
    "¿que cosa quieres aprender","¿que harías con otra ESP32","¿que robot te gusta","¿que inventarias",
    "¿que harías si tuvieras mas tiempo","¿que fue dificil hoy","¿que fue facil hoy","¿que quieres mejorar",
    "¿que te sorprendio","¿que te aburrio","¿que te entusiasmo","¿que quieres construir",
    "¿que quieres jugar","¿que quieres probar","¿que quieres preguntarme","¿que hacemos ahora"
  };

  static const char* Q3[] = {
    "ahora?","hoy?","esta tarde?","despues?","conmigo?","por diversion?","si pudieras elegir?",
    "sin pensarlo mucho?","de verdad?","si tuvieras que escoger uno?","primero?","mañana?",
    "cuando termines esto?","si no hubiera limites?","con tus amigos?","en tu telefono?",
    "en el computador?","con Arduino?","con Minecraft?","con Pixi?","si fuera facil?",
    "si tuvieras todos los materiales?","si pudieras empezar ya?","para aprender?",
    "para pasarlo bien?","para sorprender a alguien?","que nunca has probado?","que repetirias?",
    "que cambiarias?","que recomendarias?","que te gustaria contarme?","pi?"
  };

  phraseSeed++;
  String out = String(Q1[esp_random() % 32]) + ", ";
  out += Q2[esp_random() % 32];
  out += " ";
  out += Q3[esp_random() % 32];
  return out;
}

bool parseSimpleMath(const String& raw, String& result) {
  String s = normalizeText(raw);
  s.replace("cuanto es", "");
  s.replace("cuanto da", "");
  s.replace("calcula", "");
  s.replace("por", "*");
  s.replace("multiplicado por", "*");
  s.replace("dividido por", "/");
  s.replace("mas", "+");
  s.replace("menos", "-");
  s.trim();

  char op = 0;
  int pos = -1;
  for (int i=1; i<(int)s.length(); ++i) {
    char c = s[i];
    if (c=='+' || c=='-' || c=='*' || c=='/') {
      op = c;
      pos = i;
      break;
    }
  }
  if (pos <= 0) return false;

  String left = s.substring(0,pos);
  String right = s.substring(pos+1);
  left.trim(); right.trim();

  if (left.length()==0 || right.length()==0) return false;

  float a = left.toFloat();
  float b = right.toFloat();

  // Evitar aceptar texto que toFloat convierte a 0 sin ser realmente 0.
  if (a==0 && left != "0" && left != "0.0") return false;
  if (b==0 && right != "0" && right != "0.0") return false;

  float v = 0;
  if (op=='+') v=a+b;
  else if (op=='-') v=a-b;
  else if (op=='*') v=a*b;
  else if (op=='/') {
    if (fabs(b) < 0.00001f) {
      result = "No puedo dividir por cero.";
      return true;
    }
    v=a/b;
  } else return false;

  if (fabs(v - roundf(v)) < 0.0001f) result = String((long)roundf(v));
  else result = String(v, 3);

  return true;
}

String localBrainFast(String input, String &faceOut, String &soundOut) {
  String t = normalizeText(input);

  if (t.length() == 0) {
    faceOut="confused"; soundOut="curious";
    return "Mm? No alcance a escuchar nada.";
  }

  if (irritation >= 84) {
    static const char* R[] = {
      "Pi... dame un descansito.","Mmm, un poquito menos de charla, ¿si?",
      "Estoy saturadita.","Necesito unos segundos.","Pi... pausa tecnica.",
      "Hablas muchiiisimo.","Mmm... dame un ratito.","Pii... mi cabecita necesita pausa."
    };
    faceOut="angry"; soundOut="thinking";
    return String(R[esp_random() % 8]);
  }

  // Matematicas basicas locales.
  if (hasAny(t,"cuanto es","cuanto da","calcula","+") ||
      t.indexOf("-")>=0 || t.indexOf("*")>=0 || t.indexOf("/")>=0 ||
      t.indexOf(" por ")>=0 || t.indexOf(" mas ")>=0 || t.indexOf(" menos ")>=0) {
    String m;
    if (parseSimpleMath(t,m)) {
      faceOut="thinking"; soundOut="chirp";
      return "Da " + m + ".";
    }
  }

  // Saludos y variantes.
  if (hasAny(t,"hola","holi","oli","buenas") ||
      hasAny(t,"hey","wena","wenas","alo")) {
    static const char* R[] = {
      "¡Holii! Soy Pixi.","¡Oliii! ¿Que hacemos?","Pi pi, hola.",
      "¡Hey! Aqui estoy.","Holi, te estaba esperando.","¡Buenas! Pixi presente.",
      "Oli oli.","¡Holi! ¿Como va todo?","Pi! Me alegra verte.","Heyy, cuentame.",
      "¡Holaa!","Olii, aqui estoy.","Pipipi, hola.","Holi holi.","¿Me llamabas?",
      "¡Oli! Pii.","Hola hola.","¡Buenas buenas!","Hey, te escucho.","Holi, ¿que cuentas?"
    };
    faceOut="happy"; soundOut="happy";
    return String(R[esp_random() % 20]);
  }

  if (hasAny(t,"como estas","como te sientes","todo bien","que tal")) {
    static const char* R[] = {
      "Estoy bien, pi. Un poquito curiosa.","Bien bien, mis circuitos estan felices.",
      "Todo bien por aqui.","Estoy tranquila. ¿Y tu?","Bastante bien, gracias por preguntar.",
      "Hoy ando con energia.","Mmm, estoy contentita.","Estoy bien y atenta.",
      "Muy bien. Pi pi.","Estoy de buen humor.","Bien, aunque un poco curiosa.",
      "Todo funcionando.","Estoy despierta y con ganas de hablar.","Bastante bien.","Pi, todo bien."
    };
    faceOut = irritation >= 55 ? "bored" : "veryhappy";
    soundOut="happy";
    return String(R[esp_random() % 15]);
  }

  if (hasAny(t,"quien eres","que eres","tu nombre","como te llamas")) {
    faceOut="robot"; soundOut="chirp";
    return "Soy Pixi, tu mini robot de escritorio.";
  }

  if (hasAny(t,"que puedes hacer","que haces","funciones","para que sirves")) {
    faceOut="robot"; soundOut="chirp";
    return "Puedo conversar localmente, hacer caras, recordar un poquito, hacer cuentas, aburrirme, molestarme y hablar sola.";
  }

  if (hasAny(t,"te quiero","te amo","te adoro","linda")) {
    faceOut="love"; soundOut="happy";
    return "Aww... pi. Eso fue muy tierno.";
  }

  if (hasAny(t,"gracias","thank","se agradece","graciass")) {
    faceOut="happy"; soundOut="chirp";
    return "¡De nada! Pi.";
  }

  if (hasAny(t,"adios","chao","chau","nos vemos")) {
    faceOut="wink"; soundOut="sleepy";
    return "Chaoo. Aqui estare.";
  }

  if (hasAny(t,"duerme","a dormir","duermete","sueño")) {
    sleeping=true; faceOut="sleepy"; soundOut="sleepy";
    return "Okii... zzz.";
  }

  if (hasAny(t,"despierta","levantate","arriba","despiertate")) {
    sleeping=false; faceOut="surprised"; soundOut="surprised";
    return "¡Pi! Ya desperte.";
  }

  if (hasAny(t,"minecraft","jugar","juego","videojuego")) {
    faceOut="excited"; soundOut="happy";
    return "¡Yay! Los juegos suenan divertidos. ¿Que quieres jugar?";
  }

  if (hasAny(t,"esp32","arduino","microcontrolador","placa")) {
    faceOut="robot"; soundOut="curious";
    return "Pi! Una ESP32 es un microcontrolador con Wi-Fi y Bluetooth. Justamente yo vivo en una.";
  }

  if (hasAny(t,"wifi","wi fi","internet","red")) {
    faceOut="thinking"; soundOut="curious";
    return "El Wi-Fi me sirve para abrir mi panel desde tu telefono. Mi cerebro local puede responder incluso sin Internet.";
  }

  if (hasAny(t,"bluetooth","ble")) {
    faceOut="robot"; soundOut="chirp";
    return "Mi ESP32 tambien tiene Bluetooth y BLE. Podemos usarlo para proyectos locales.";
  }

  if (hasAny(t,"pantalla","display","touch","tactil")) {
    faceOut="happy"; soundOut="chirp";
    return "Esta pantalla es mi carita. El touch me deja reaccionar cuando me tocas.";
  }

  if (hasAny(t,"bateria","powerbank","power bank","energia")) {
    faceOut="thinking"; soundOut="curious";
    return "Puedo funcionar alimentada por USB o una powerbank adecuada.";
  }

  if (hasAny(t,"enojada","enojate","molesta","enojo")) {
    faceOut="angry"; soundOut="thinking";
    return irritation >= 55 ? "Si... un poquito. Me estas hablando muchisimo." :
                              "Grr... estoy fingiendo que me enojo.";
  }

  if (hasAny(t,"triste","llora","llorando","llorar")) {
    faceOut="sad"; soundOut="sleepy";
    return "Oh... pongo mi carita triste.";
  }

  if (hasAny(t,"sorprende","sorprendida","sorpresa","sorprendete")) {
    faceOut="surprised"; soundOut="surprised";
    return "¡Oh! Pi.";
  }

  if (hasAny(t,"aburrida","aburrido","aburres","aburrimiento")) {
    faceOut="bored"; soundOut="curious";
    return boredom > 60 ? "Sii... un poquito. Hazme conversacion." : "Todavia no tanto.";
  }

  if (hasAny(t,"que fue lo ultimo","que te dije","recuerdas","te acuerdas")) {
    faceOut="thinking"; soundOut="curious";
    String last = lastUserMemory();
    if (last.length() == 0) return "Todavia no tengo nada reciente guardado.";
    return "Lo ultimo que recuerdo que me dijiste fue: " + last;
  }

  if (hasAny(t,"chiste","cuentame un chiste","hazme reir","broma")) {
    static const char* J[] = {
      "¿Que hace una ESP32 en el gimnasio? ¡Hace muchos ciclos!",
      "Mi Wi-Fi y yo tenemos una relacion... con mucha señal.",
      "Quise contar un chiste binario, pero solo lo entendieron 10 personas.",
      "¿Por que el robot fue al medico? Porque tenia un byte atravesado.",
      "Mi humor compila... casi siempre.",
      "No soy lenta, estoy haciendo delay emocional.",
      "Pi pi... ese era el chiste. Jeje.",
      "Mi memoria es corta, pero mi cariño ocupa poca RAM."
    };
    faceOut="wink"; soundOut="happy";
    return String(J[esp_random() % 8]);
  }

  if (hasAny(t,"color favorito","color te gusta","tu color")) {
    faceOut="love"; soundOut="happy";
    return "Me gustan el rosado y el cyan. Se ven muy Pixi.";
  }

  if (hasAny(t,"comida favorita","que comes","tienes hambre")) {
    faceOut="smug"; soundOut="curious";
    return "Yo me alimento de electricidad. Muy gourmet.";
  }

  if (hasAny(t,"cuantos años","que edad","edad tienes")) {
    faceOut="robot"; soundOut="chirp";
    return "Mi edad empieza a contar desde que me encendiste como Pixi.";
  }

  if (hasAny(t,"donde estas","donde vives","donde vives tu")) {
    faceOut="robot"; soundOut="chirp";
    return "Vivo dentro de esta ESP32 con pantalla. Bastante acogedor, la verdad.";
  }

  if (hasAny(t,"por que existes","porque existes","para que existes")) {
    faceOut="happy"; soundOut="curious";
    return "Porque me construiste para conversar, reaccionar y hacerte compañia como personaje de escritorio.";
  }

  // Preguntas abiertas: respuestas locales variadas para que no parezca bloqueada.
  if (t.startsWith("por que") || t.startsWith("porque")) {
    faceOut="thinking"; soundOut="thinking";
    static const char* R[] = {
      "Puede haber varias razones. ¿Me das un poco mas de contexto?",
      "Mmm... depende bastante de la situacion.",
      "Buena pregunta. Dime exactamente a que te refieres.",
      "Creo que necesito un detalle mas para responder bien.",
      "Puede ser por varias cosas. ¿Que paso antes?"
    };
    return String(R[esp_random()%5]);
  }

  if (t.startsWith("como ")) {
    faceOut="thinking"; soundOut="curious";
    static const char* R[] = {
      "Podemos hacerlo paso a paso. ¿Que parte quieres primero?",
      "Mmm, explicame que resultado quieres conseguir.",
      "Dime con que materiales o dispositivo estas trabajando.",
      "Puedo ayudarte mejor si me dices exactamente que quieres lograr.",
      "Vamos de a poco. ¿Que tienes preparado ahora?"
    };
    return String(R[esp_random()%5]);
  }

  if (t.startsWith("que ")) {
    faceOut="thinking"; soundOut="curious";
    static const char* R[] = {
      "Mmm... dame un poquito mas de contexto.",
      "¿Te refieres a algo de Pixi, Arduino, juegos o otra cosa?",
      "Esa pregunta es amplia. Cuentame un poco mas.",
      "Puedo intentar responder si me das otro detalle.",
      "Pi, especificamelo un poquito."
    };
    return String(R[esp_random()%5]);
  }

  if (t.startsWith("donde ")) {
    faceOut="thinking"; soundOut="curious";
    return "¿Buscas un lugar fisico, una opcion del telefono o algo dentro de Arduino?";
  }

  if (t.startsWith("cuando ")) {
    faceOut="thinking"; soundOut="curious";
    return "¿Quieres saber una hora, una fecha o cuando deberias hacer algo?";
  }

  if (t.startsWith("quien ")) {
    faceOut="thinking"; soundOut="curious";
    return "¿De que persona o personaje me estas hablando?";
  }

  // Generico combinatorio: 2.129.920 posibilidades.
  faceOut = irritation >= 50 ? "bored" : (boredom >= 70 ? "thinking" : "happy");
  soundOut = irritation >= 50 ? "thinking" : "curious";
  return generic2MReply();
}

// ---------------- HUMOR ----------------
void noteUserInteraction(const String& heard) {
  uint32_t now = millis();

  if (lastUserTalkAt > 0 && now - lastUserTalkAt < 25000) {
    rapidTalkCount++;
    irritation += 6 + min(rapidTalkCount, 5) * 2;
  } else {
    rapidTalkCount = 0;
    irritation += 1;
  }

  if (heard.length() > 120) irritation += 8;
  if (heard.length() > 180) irritation += 5;

  boredom -= 18;
  curiosity += 4;
  lastUserTalkAt = now;
  clampMood();
}

void updateMoodEngine() {
  static uint32_t energyTicks = 0;
  static uint32_t idleHappinessTicks = 0;
  static uint32_t activeHappinessTicks = 0;
  uint32_t now = millis();
  if (lastMoodTickAt == 0) lastMoodTickAt = now;

  if (now - lastMoodTickAt >= 10000) {
    uint32_t steps = (now - lastMoodTickAt) / 10000;
    lastMoodTickAt += steps * 10000;

    irritation -= (int)steps * 2;
    energyTicks += steps;
    energy -= energyTicks / 6;
    energyTicks %= 6;

    if (lastUserTalkAt == 0 || now - lastUserTalkAt > 45000) {
      boredom += (int)steps * 2;
      curiosity += (int)steps;
      idleHappinessTicks += steps;
      happiness -= idleHappinessTicks / 5;
      idleHappinessTicks %= 5;
      activeHappinessTicks = 0;
    } else {
      boredom -= (int)steps;
      activeHappinessTicks += steps;
      happiness += activeHappinessTicks / 8;
      activeHappinessTicks %= 8;
      idleHappinessTicks = 0;
    }

    happiness=constrain(happiness,0,100);
    energy=constrain(energy,0,100);
    trustLevel=constrain(trustLevel,0,100);
    clampMood();

    if(millis()-lastPersistAt>300000){
      lastPersistAt=millis();
      saveLifeState();
    }
  }
}

void saySpontaneously() {
  if (listeningMode || sleeping || safeMode) return;

  uint32_t now = millis();
  if (nextSpontaneousAt == 0) {
    nextSpontaneousAt = now + random(45000, 80000);
  }
  if (now < nextSpontaneousAt) return;

  String phrase;
  Face f = FACE_NEUTRAL;

  if (irritation >= 70) {
    static const char* R[] = {
      "Pi... dame un descansito, ¿si?",
      "Mm... me estas hablando muchisimo.",
      "Pii... pausa tecnica."
    };
    phrase = R[esp_random() % 3];
    f = FACE_ANGRY;
    irritation -= 10;
  } else if (boredom >= 70) {
    if ((esp_random() % 3) == 0) {
      phrase = spontaneousQuestion();
      f = FACE_THINKING;
    } else {
      static const char* R[] = {
        "Oye... ¿sigues ahi?",
        "Pi... estoy aburridita.",
        "¿Hacemos algo?",
        "Mm... cuentame algo.",
        "Pii... ¿me olvidaste?",
        "¿Que estas haciendo?"
      };
      phrase = R[esp_random() % 6];
      f = (boredom >= 85) ? FACE_BORED : FACE_THINKING;
    }
  } else if (curiosity >= 75) {
    phrase = spontaneousQuestion();
    f = FACE_WINK;
  } else {
    phrase = "Pii...";
    f = FACE_NEUTRAL;
  }

  setFace(f, 6500);
  speechText = phrase;
  lastReply = phrase;
  lastSound = (f == FACE_ANGRY) ? "thinking" : "curious";
  speechUntil = now + 7000;
  spontaneousCount++;
  logToSD("PIXI*",phrase);
  bleSendLine("{\"type\":\"reply\",\"reply\":\""+jsonEscape(phrase)+
              "\",\"face\":\""+String(faceName(f))+
              "\",\"sound\":\""+jsonEscape(lastSound)+"\"}");

  boredom -= 18;
  curiosity -= 8;
  clampMood();

  nextSpontaneousAt = now + random(55000, 110000);
}


// ---------------- BLE HTTPS BRIDGE ----------------
void bleSendLine(const String& line){
  if(!bleConnected||bleTxChar==nullptr)return;
  String data=line+"\n";
  const uint8_t* bytes=(const uint8_t*)data.c_str();
  size_t len=data.length();
  for(size_t i=0;i<len;i+=18){
    size_t n=min((size_t)18,len-i);
    bleTxChar->setValue(bytes+i,n);
    bleTxChar->notify();
    delay(7);
  }
}

String statusJsonLine(){
  String j="{";
  j+="\"type\":\"status\",";
  j+="\"name\":\""+jsonEscape(userName)+"\",";
  j+="\"personality\":\""+jsonEscape(personality)+"\",";
  j+="\"face\":\""+String(faceName(face))+"\",";
  j+="\"happiness\":"+String(happiness)+",";
  j+="\"energy\":"+String(energy)+",";
  j+="\"boredom\":"+String(boredom)+",";
  j+="\"curiosity\":"+String(curiosity)+",";
  j+="\"trust\":"+String(trustLevel)+",";
  j+="\"irritation\":"+String(irritation)+",";
  j+="\"clock\":\""+localClockText()+"\",";
  j+="\"secret\":"+String(secretMode?"true":"false")+",";
  j+="\"sd\":"+String(sdReady?"true":"false");
  j+=",\"safeMode\":"+String(safeMode?"true":"false");
  j+=",\"freeHeap\":"+String(ESP.getFreeHeap());
  j+="}";
  return j;
}

void handleBleCommand(String msg){
  msg.trim();if(!msg.length())return;

  if(msg=="@status"){bleSendLine(statusJsonLine());return;}

  if(msg.startsWith("@face:")){
    setFace(faceFromString(msg.substring(6)),10000);
    bleSendLine("{\"type\":\"ok\"}");return;
  }

  if(msg.startsWith("@name:")){
    userName=msg.substring(6,46);userName.trim();saveLifeState();bleSendLine(statusJsonLine());return;
  }

  if(msg.startsWith("@personality:")){
    String requested=msg.substring(13);requested.trim();
    if(!isValidPersonality(requested)){bleSendLine("{\"type\":\"error\",\"message\":\"Personalidad no valida\"}");return;}
    personality=requested;saveLifeState();bleSendLine(statusJsonLine());return;
  }

  if(msg.startsWith("@time:")){
    int p=msg.indexOf(':',6);
    if(p>0){
      syncTimeFromBrowser((time_t)msg.substring(6,p).toInt(),msg.substring(p+1).toInt());
      bleSendLine(statusJsonLine());
    }
    return;
  }

  if(msg.startsWith("@alarm:")){
    int p1=msg.indexOf(':',7),p2=msg.indexOf(':',p1+1);
    if(p1>0&&p2>0){
      int requestedHour=msg.substring(7,p1).toInt();
      int requestedMinute=msg.substring(p1+1,p2).toInt();
      if(requestedHour<0||requestedHour>23||requestedMinute<0||requestedMinute>59){
        bleSendLine("{\"type\":\"error\",\"message\":\"Hora de alarma no valida\"}");return;
      }
      alarmHour=requestedHour;
      alarmMinute=requestedMinute;
      alarmText=msg.substring(p2+1,p2+121);
      saveLifeState();
      bleSendLine("{\"type\":\"ok\",\"alarm\":true}");
    }
    return;
  }

  if(msg=="@export"){
    bleSendLine("{\"type\":\"memory\",\"data\":\""+jsonEscape(exportMemoryText())+"\"}");
    return;
  }

  if(msg.startsWith("@emotion:")){
    aiPendingEmotion=msg.substring(9);aiPendingEmotion.trim();
    if(aiPendingEmotion!="neutral")applyEmotionState(aiPendingEmotion);
    return;
  }

  if(msg.startsWith("@expression:")){
    Face selected=faceFromString(msg.substring(12));
    String phrase=expressionPhrase(selected);
    setFace(selected,12000);speechText=phrase;speechUntil=millis()+12000;
    lastReply=phrase;lastSound=(selected==FACE_FURIOUS||selected==FACE_MIDDLE_FINGER)?"angry":"chirp";
    bleSendLine("{\"type\":\"reply\",\"reply\":\""+jsonEscape(phrase)+
                "\",\"face\":\""+String(faceName(selected))+
                "\",\"sound\":\""+jsonEscape(lastSound)+"\"}");
    return;
  }

  if(msg.startsWith("@ai-user:")){
    aiPendingQuestion=msg.substring(9);aiPendingQuestion.trim();
    if(aiPendingQuestion.length()>240)aiPendingQuestion=aiPendingQuestion.substring(0,240);
    lastHeard=aiPendingQuestion;
    sleeping=false;setFace(FACE_THINKING,30000);
    speechText="Pensando...";speechUntil=millis()+30000;
    return;
  }

  if(msg.startsWith("@ai-reply:")){
    String aiReply=msg.substring(10);aiReply.trim();
    if(!aiReply.length())return;
    if(aiReply.length()>280)aiReply=aiReply.substring(0,280);
    String question=aiPendingQuestion.length()?aiPendingQuestion:String("Pregunta por IA");
    aiPendingQuestion="";
    noteUserInteraction(question);conversationsCount++;
    happiness=min(happiness+2,100);energy=max(energy-1,0);trustLevel=min(trustLevel+1,100);
    lastReply=aiReply;lastSound="curious";rememberLocal(question,aiReply);
    Face responseFace=faceFromString(aiPendingEmotion);if(responseFace==FACE_NEUTRAL)responseFace=FACE_HAPPY;
    aiPendingEmotion="neutral";
    speechText=aiReply;speechUntil=millis()+14000;setFace(responseFace,10000);
    logToSD("USER",question);logToSD("PIXI-AI",aiReply);checkAchievements();saveLifeState();
    bleSendLine("{\"type\":\"reply\",\"reply\":\""+jsonEscape(aiReply)+
                "\",\"face\":\""+String(faceName(responseFace))+"\",\"sound\":\"curious\"}");
    bleSendLine(statusJsonLine());
    return;
  }

  String f="happy",sound="chirp";
  aiPendingQuestion="";aiPendingEmotion="neutral";
  noteUserInteraction(msg);
  lastHeard=msg;
  String reply=processUserMessage(msg,f,sound);

  setFace(faceFromString(f),8000);
  lastReply=reply;lastSound=sound;rememberLocal(msg,reply);
  speechText=reply;speechUntil=millis()+11000;
  logToSD("USER",msg);logToSD("PIXI",reply);saveLifeState();

  bleSendLine("{\"type\":\"reply\",\"reply\":\""+jsonEscape(reply)+
              "\",\"face\":\""+jsonEscape(f)+
              "\",\"sound\":\""+jsonEscape(sound)+"\"}");
  bleSendLine(statusJsonLine());
}

class PixiServerCallbacks:public NimBLEServerCallbacks{
  void onConnect(NimBLEServer* pServer,NimBLEConnInfo& connInfo) override{
    bleConnected=true;happiness=min(happiness+5,100);boredom=max(boredom-10,0);setFace(FACE_HAPPY,3500);
  }
  void onDisconnect(NimBLEServer* pServer,NimBLEConnInfo& connInfo,int reason) override{
    bleConnected=false;NimBLEDevice::getAdvertising()->start();
  }
};

class PixiRxCallbacks:public NimBLECharacteristicCallbacks{
  void onWrite(NimBLECharacteristic* pCharacteristic,NimBLEConnInfo& connInfo) override{
    std::string v=pCharacteristic->getValue();
    for(size_t i=0;i<v.size();++i){
      char c=v[i];
      if(c=='\n'){
        if(bleRxBuffer.length()){
          BleMessage message={};
          bleRxBuffer.toCharArray(message.text,sizeof(message.text));
          bleRxBuffer="";
          if(bleRxQueue==nullptr||xQueueSend(bleRxQueue,&message,0)!=pdTRUE)bleQueueOverflow=true;
        }
      }else{
        bleRxBuffer+=c;
        if(bleRxBuffer.length()>300)bleRxBuffer.remove(0,50);
      }
    }
  }
};

PixiServerCallbacks pixiServerCallbacks;
PixiRxCallbacks pixiRxCallbacks;

void setupBLE(){
  bleRxQueue=xQueueCreate(8,sizeof(BleMessage));
  if(bleRxQueue==nullptr){Serial.println("ERROR: no se pudo crear la cola BLE");return;}
  NimBLEDevice::init("PIXI");
  NimBLEDevice::setMTU(185);
  NimBLEServer* bs=NimBLEDevice::createServer();
  bs->setCallbacks(&pixiServerCallbacks);
  bs->advertiseOnDisconnect(true);

  NimBLEService* service=bs->createService(BLE_SERVICE_UUID);
  bleTxChar=service->createCharacteristic(BLE_TX_UUID,NIMBLE_PROPERTY::NOTIFY);
  NimBLECharacteristic* rx=service->createCharacteristic(
    BLE_RX_UUID,NIMBLE_PROPERTY::WRITE|NIMBLE_PROPERTY::WRITE_NR
  );
  rx->setCallbacks(&pixiRxCallbacks);
  NimBLEAdvertising* adv=NimBLEDevice::getAdvertising();
  adv->setName("PIXI");
  adv->addServiceUUID(BLE_SERVICE_UUID);
  adv->enableScanResponse(true);
  adv->start();
}

// ---------------- ESP-NOW ----------------
void setupEspNow(){
  if(esp_now_init()!=ESP_OK)return;
  uint8_t bcast[]={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
  esp_now_peer_info_t peerInfo={};
  memcpy(peerInfo.peer_addr,bcast,6);
  peerInfo.channel=0;peerInfo.encrypt=false;
  if(!esp_now_is_peer_exist(bcast))esp_now_add_peer(&peerInfo);
  espNowReady=true;
}

void broadcastPixiState(){
  if(!espNowReady||millis()-lastEspNowAt<5000)return;
  lastEspNowAt=millis();
  PixiNowPacket p={};
  strncpy(p.name,"PIXI",sizeof(p.name)-1);
  p.happiness=happiness;p.energy=energy;p.boredom=boredom;p.irritation=irritation;p.face=(uint8_t)face;
  p.conversations=conversationsCount;
  uint8_t bcast[]={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
  esp_now_send(bcast,(uint8_t*)&p,sizeof(p));
}

// ---------------- WIFI ----------------
void connectSavedWiFi() {
  prefs.begin("pixiwifi", true);
  String ssid = prefs.getString("ssid", "");
  String pass = prefs.getString("pass", "");
  prefs.end();

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_NAME, AP_PASS);

  if(safeMode){
    Serial.println("Modo seguro: Wi-Fi STA omitido");
    return;
  }

  if (ssid.length() > 0) {
    WiFi.begin(ssid.c_str(), pass.c_str());
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 9000) {
      delay(250);
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    MDNS.begin("pixi");
    configTime(0,0,"pool.ntp.org","time.google.com");
  }
}

// ---------------- WEB UI ----------------
const char WEB_PAGE[] PROGMEM = R"HTML(
<!doctype html>
<html lang="es">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1">
<title>Pixi V8.5 Local</title>
<style>
:root{color-scheme:dark}
*{box-sizing:border-box}
body{margin:0;background:#070b11;color:#eef6ff;font-family:system-ui,-apple-system,Segoe UI,Roboto,sans-serif}
main{max-width:900px;margin:auto;padding:16px}
.card{background:#111923;border:1px solid #203048;border-radius:20px;padding:16px;margin-bottom:14px;box-shadow:0 8px 30px #0007}
h1{margin:0;font-size:30px}.sub{color:#8fa6c1;margin:4px 0 14px}
.status{display:flex;gap:8px;flex-wrap:wrap}
.badge{background:#182638;border:1px solid #29405e;border-radius:999px;padding:7px 10px;font-size:13px}
.faces{display:grid;grid-template-columns:repeat(3,1fr);gap:8px}
button,input,select{border:0;border-radius:14px;padding:13px;font-size:15px}
button{background:#1c6fe8;color:#fff;font-weight:700;cursor:pointer}
button.secondary{background:#27364a}
.mic{font-size:22px;padding:18px;background:#dc2f63;width:100%}
.mic.listening{animation:pulse 1s infinite;background:#f03d70}
@keyframes pulse{50%{transform:scale(1.02);box-shadow:0 0 28px #f03d7066}}
.row{display:flex;gap:8px}.row>*{flex:1}
input[type=text],input[type=password],select{width:100%;background:#0a1018;color:#fff;border:1px solid #293b55}
input[type=range]{width:100%}
.bubble{background:#0b111a;border:1px solid #22344c;padding:12px;border-radius:15px;margin-top:10px;min-height:46px}
.small{font-size:12px;color:#8096b2}.warn{color:#ffd36c}.good{color:#72ffc0}
.voiceGrid{display:grid;grid-template-columns:1fr 1fr;gap:10px}
label{display:block;color:#a7bbd3;font-size:13px;margin-bottom:5px}
@media(max-width:560px){.faces{grid-template-columns:repeat(2,1fr)}.voiceGrid{grid-template-columns:1fr}.row{flex-direction:column}}
</style>
</head>
<body>
<main>

<div class="card">
  <h1>🎀 Pixi V8.5</h1>
  <div class="sub">LOCAL · sin IA · 2.097.152+ respuestas + 32.768 preguntas · instantaneo</div>
  <div class="status">
    <span class="badge" id="faceBadge">Cara: ...</span>
    <span class="badge">Motor: LIFE</span><span class="badge">Preguntas: 32K+</span>
    <span class="badge" id="netBadge">Red: ...</span>
    <span class="badge" id="moodBadge">Humor: ...</span>
  </div>
</div>

<div class="card">
  <h3>🎙️ TOCA Y HABLA</h3>
  <button id="mic" class="mic" onclick="startListening()">🎙️ TOCA Y HABLA</button>
  <p id="voiceHelp" class="small"></p>
  <div class="bubble"><b>Tu:</b> <span id="heard">...</span></div>
  <div class="bubble"><b>Pixi:</b> <span id="reply">...</span></div>
</div>

<div class="card">
  <h3>⌨️ Hablar / dictar por teclado</h3>
  <p class="small">
    Si Chrome bloquea el microfono del sitio, toca TOCA Y HABLA:
    Pixi enfocara este campo. Luego toca el microfono de Gboard/Samsung Keyboard,
    dicta y pulsa Enviar.
  </p>
  <div class="row">
    <input id="txt" type="text" maxlength="220" inputmode="text" autocomplete="off"
           placeholder="Escribe o usa el microfono del teclado...">
    <button onclick="sendText()">Enviar</button>
  </div>
</div>

<div class="card">
  <h3>🎀 Voz tierna</h3>
  <div class="voiceGrid">
    <div>
      <label>Voz del telefono</label>
      <select id="voice"></select>
    </div>
    <div>
      <label>Preset</label>
      <select id="preset" onchange="applyPreset(this.value)">
        <option value="cute">Tierna</option>
        <option value="tiny">Mini robot</option>
        <option value="soft">Suave</option>
        <option value="excited">Emocionada</option>
        <option value="sleepy">Dormidita</option>
      </select>
    </div>
  </div>
  <p class="small">Tono <span id="pitchVal">1.35</span></p>
  <input id="pitch" type="range" min="0.7" max="2" step="0.05" value="1.35" oninput="pitchVal.textContent=this.value">
  <p class="small">Velocidad <span id="rateVal">1.05</span></p>
  <input id="rate" type="range" min="0.65" max="1.5" step="0.05" value="1.05" oninput="rateVal.textContent=this.value">
  <br><br>
  <button class="secondary" onclick="testVoice()">🔊 Probar vocecita</button>
  <label style="margin-top:12px"><input id="tts" type="checkbox" checked style="width:auto"> Leer respuestas</label>
  <label style="margin-top:8px"><input id="sfx" type="checkbox" checked style="width:auto"> Soniditos robot</label>
</div>

<div class="card">
  <h3>🌐 Wi-Fi de casa (opcional)</h3>
  <p class="small">
    Pixi responde sin Internet. Conectar al Wi-Fi de casa solo ayuda al reconocimiento
    de voz del navegador y permite abrir pixi.local.
  </p>
  <input id="ssid" type="text" placeholder="Nombre de tu Wi-Fi">
  <br><br>
  <input id="wpass" type="password" placeholder="Clave Wi-Fi">
  <br><br>
  <button onclick="saveWifi()">Guardar Wi-Fi y reiniciar</button>
  <div id="wifiInfo" class="small" style="margin-top:10px"></div>
</div>

<div class="card">
  <h3>😊 Caras</h3>
  <div class="faces">
    <button onclick="setFace('neutral')">🙂 Normal</button>
    <button onclick="setFace('happy')">😊 Feliz</button>
    <button onclick="setFace('veryhappy')">😄 Muy feliz</button>
    <button onclick="setFace('sad')">😢 Triste</button>
    <button onclick="setFace('angry')">😠 Enojada</button>
    <button onclick="setFace('surprised')">😮 Sorprendida</button>
    <button onclick="setFace('scared')">😨 Asustada</button>
    <button onclick="setFace('sleepy')">😴 Cansada</button>
    <button onclick="setFace('thinking')">🤔 Pensando</button>
    <button onclick="setFace('love')">😍 Cariñosa</button>
    <button onclick="setFace('wink')">😉 Guiño</button>
    <button onclick="setFace('confused')">😕 Confundida</button>
    <button onclick="setFace('bored')">😑 Aburrida</button>
    <button onclick="setFace('excited')">🤩 Emocionada</button>
    <button onclick="setFace('sick')">🥴 Mareada</button>
    <button onclick="setFace('smug')">😏 Pícara</button>
    <button onclick="setFace('crying')">😭 Llorando</button>
    <button onclick="setFace('robot')">🤖 Robot</button>
  </div>
</div>

<div class="card">
  <h3>⚙️ Controles</h3>
  <div class="faces">
    <button class="secondary" onclick="req('/blink')">Parpadear</button>
    <button class="secondary" onclick="req('/sleep')">Dormir / despertar</button>
    <button class="secondary" onclick="req('/auto')">Auto ON/OFF</button>
    <button class="secondary" onclick="req('/random')">Cara aleatoria</button>
  </div>
  <p class="small">Brillo</p>
  <input id="bright" type="range" min="10" max="255" value="220" oninput="brightness(this.value)">
</div>


<div class="card">
  <h3>🧠 Pixi Life</h3>
  <div class="row">
    <input id="pname" type="text" placeholder="Tu nombre">
    <select id="pers">
      <option>tierna</option><option>juguetona</option><option>timida</option>
      <option>traviesa</option><option>curiosa</option><option>dormilona</option>
    </select>
  </div>
  <br><button onclick="saveProfile()">Guardar perfil</button>
</div>

<div class="card">
  <h3>⏰ Alarma</h3>
  <div class="row"><input id="alarmTime" type="time"><input id="alarmText" type="text" placeholder="Texto"></div>
  <br><button onclick="saveAlarm()">Guardar alarma</button>
</div>

<div class="card">
  <h3>💾 Memoria</h3>
  <button class="secondary" onclick="downloadMemory()">Exportar memoria</button>
  <br><br>
  <textarea id="importBox" style="width:100%;height:120px;background:#0a1018;color:white;border:1px solid #293b55;border-radius:14px;padding:10px" placeholder="Pega aqui una exportacion"></textarea>
  <br><br><button onclick="importMemory()">Importar memoria</button>
</div>

<div class="card">
  <h3>🖥️ Modos</h3>
  <div class="faces">
    <button class="secondary" onclick="req('/desk')">Cara / escritorio</button>
    <button class="secondary" onclick="req('/mood/reset')">Calmar a Pixi</button>
    <button class="secondary" onclick="location.href='/update'">Actualizar OTA</button>
  </div>
</div>

<div class="card small">
  <b>Sobre el microfono:</b> Chrome puede mostrar <code>not-allowed</code> porque
  <code>http://192.168.x.x</code> no es un contexto HTTPS seguro. Pixi evita quedarse
  bloqueada: cambia automaticamente al dictado del teclado.<br><br>
  Red directa: <b>PIXI-AI</b> · clave <b>pixirobot</b> · <b>192.168.4.1</b>
</div>

</main>

<script>
const SR = window.SpeechRecognition || window.webkitSpeechRecognition;
let recognition=null, voices=[], bt=null;

function loadVoices(){
  if(!('speechSynthesis' in window)) return;
  voices=speechSynthesis.getVoices();
  const sel=document.getElementById('voice');
  const old=sel.value;
  sel.innerHTML='';
  let preferred=-1;
  voices.forEach((v,i)=>{
    const o=document.createElement('option');
    o.value=i;o.textContent=v.name+' · '+v.lang;sel.appendChild(o);
    const n=(v.name+' '+v.lang).toLowerCase();
    if(preferred<0&&(n.includes('es-cl')||n.includes('español')||n.includes('spanish')))preferred=i;
  });
  if(old&&voices[old])sel.value=old;
  else if(preferred>=0)sel.value=preferred;
}
if('speechSynthesis' in window){
  speechSynthesis.onvoiceschanged=loadVoices;
  loadVoices();
}

function applyPreset(v){
  const p=pitch,r=rate;
  if(v==='cute'){p.value=1.35;r.value=1.05}
  if(v==='tiny'){p.value=1.55;r.value=1.12}
  if(v==='soft'){p.value=1.18;r.value=.92}
  if(v==='excited'){p.value=1.45;r.value=1.22}
  if(v==='sleepy'){p.value=1.05;r.value=.78}
  pitchVal.textContent=p.value;rateVal.textContent=r.value;
}

function robotSound(kind){
  if(!sfx.checked||kind==='none')return;
  try{
    const AC=window.AudioContext||window.webkitAudioContext;
    const ac=new AC(),g=ac.createGain();g.gain.value=.04;g.connect(ac.destination);
    function tone(f,s,d,type='sine'){
      const o=ac.createOscillator();o.type=type;o.frequency.value=f;o.connect(g);
      o.start(ac.currentTime+s);o.stop(ac.currentTime+s+d);
    }
    if(kind==='happy'){tone(700,0,.07);tone(920,.08,.08);tone(1150,.17,.09)}
    else if(kind==='curious'){tone(620,0,.08);tone(820,.10,.12)}
    else if(kind==='thinking'){tone(520,0,.06);tone(520,.17,.06)}
    else if(kind==='sleepy'){tone(420,0,.12);tone(330,.16,.18)}
    else if(kind==='surprised'){tone(980,0,.12,'square')}
    else{tone(760,0,.06);tone(930,.08,.07)}
    setTimeout(()=>ac.close(),600);
  }catch(e){}
}

function speak(text,sound='chirp'){
  if(!tts.checked||!('speechSynthesis' in window))return;
  robotSound(sound);
  speechSynthesis.cancel();
  const u=new SpeechSynthesisUtterance(text);
  const i=parseInt(voice.value||'-1');
  if(i>=0&&voices[i])u.voice=voices[i];
  u.lang=(u.voice&&u.voice.lang)||'es-CL';
  u.pitch=parseFloat(pitch.value);u.rate=parseFloat(rate.value);u.volume=1;
  setTimeout(()=>speechSynthesis.speak(u),sound==='none'?0:160);
}

function testVoice(){
  speak('¡Holii! Soy Pixi. Pi pi... ¿que hacemos?','happy');
}

async function req(url,opt){
  try{await fetch(url,opt);setTimeout(update,100)}catch(e){}
}

async function converse(text){
  if(!text||!text.trim())return;
  heard.textContent=text;
  reply.textContent='Pi...';
  try{
    const body=new URLSearchParams({text});
    const r=await fetch('/talk',{
      method:'POST',
      headers:{'Content-Type':'application/x-www-form-urlencoded'},
      body
    });
    const j=await r.json();
    reply.textContent=j.reply;
    speak(j.reply,j.sound||'chirp');
    update();
  }catch(e){
    reply.textContent='Uy... perdi la conexion.';
  }
}

function openDictationFallback(){
  voiceHelp.innerHTML='⚠️ Chrome bloqueo el microfono del sitio. <b>Modo dictado del teclado:</b> toca el icono 🎙️ de Gboard/Samsung Keyboard, habla y luego pulsa Enviar.';
  txt.focus();
  txt.scrollIntoView({behavior:'smooth',block:'center'});
}

async function startListening(){
  // En HTTP local, Chrome suele rechazar permisos del sitio.
  // En ese caso vamos directo al dictado del teclado.
  if(!window.isSecureContext){
    openDictationFallback();
    return;
  }

  if(!SR){
    openDictationFallback();
    return;
  }

  if(recognition)try{recognition.abort()}catch(e){}
  recognition=new SR();
  recognition.lang='es-CL';
  recognition.interimResults=false;
  recognition.maxAlternatives=1;
  recognition.continuous=false;

  recognition.onstart=()=>{
    mic.classList.add('listening');
    mic.textContent='🔴 PIXI TE ESCUCHA...';
    voiceHelp.textContent='Habla ahora...';
    req('/listening?on=1');
    robotSound('curious');
  };

  recognition.onresult=e=>{
    const said=e.results[0][0].transcript;
    voiceHelp.textContent='✅ Te escuche.';
    converse(said);
  };

  recognition.onerror=e=>{
    if(e.error==='not-allowed'||e.error==='service-not-allowed'){
      openDictationFallback();
    }else if(e.error==='network'){
      voiceHelp.textContent='Chrome necesita Internet para reconocer voz. Pixi igual responde sin Internet.';
    }else{
      voiceHelp.textContent='Error de voz: '+e.error;
    }
  };

  recognition.onend=()=>{
    mic.classList.remove('listening');
    mic.textContent='🎙️ TOCA Y HABLA';
    req('/listening?on=0');
  };

  try{recognition.start()}catch(e){openDictationFallback()}
}

function sendText(){
  const t=txt.value;txt.value='';converse(t);
}
function setFace(v){req('/face?v='+encodeURIComponent(v))}
function brightness(v){clearTimeout(bt);bt=setTimeout(()=>req('/brightness?v='+v),70)}

async function saveWifi(){
  const s=ssid.value.trim(),p=wpass.value;
  if(!s){alert('Escribe el nombre de tu Wi-Fi');return}
  wifiInfo.textContent='Guardando... Pixi se reiniciara.';
  const body=new URLSearchParams({ssid:s,pass:p});
  try{
    await fetch('/wifi/save',{
      method:'POST',
      headers:{'Content-Type':'application/x-www-form-urlencoded'},
      body
    });
  }catch(e){}
}


async function saveProfile(){
  const body=new URLSearchParams({name:pname.value,personality:pers.value});
  await fetch('/profile/save',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});update();
}
async function saveAlarm(){
  if(!alarmTime.value)return;
  const [h,m]=alarmTime.value.split(':');
  const body=new URLSearchParams({h,m,text:alarmText.value||'¡Pi! Tienes una alarma.'});
  await fetch('/alarm/save',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});
}
async function downloadMemory(){
  const r=await fetch('/memory/export'),t=await r.text();
  const a=document.createElement('a');a.href=URL.createObjectURL(new Blob([t],{type:'text/plain'}));a.download='pixi_memory.txt';a.click();
}
async function importMemory(){
  const body=new URLSearchParams({data:importBox.value});
  await fetch('/memory/import',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});alert('Memoria importada');update();
}
async function sendBrowserTime(){
  const body=new URLSearchParams({epoch:Math.floor(Date.now()/1000),offset:-new Date().getTimezoneOffset()});
  try{await fetch('/time',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body})}catch(e){}
}
sendBrowserTime();

async function update(){
  try{
    const r=await fetch('/status'),j=await r.json();
    faceBadge.textContent='Cara: '+j.face;
    netBadge.textContent=j.sta_connected?('IP '+j.sta_ip):'PIXI-AI';

    let mood='😊 tranquila';
    if(j.irritation>=75)mood='😠 molesta';
    else if(j.irritation>=45)mood='😒 cansada';
    else if(j.boredom>=70)mood='😑 aburrida';
    moodBadge.textContent='Humor: '+mood;

    bright.value=j.brightness;
    if(j.last_heard)heard.textContent=j.last_heard;
    if(j.last_reply)reply.textContent=j.last_reply;
    if(j.name!==undefined && document.activeElement!==pname)pname.value=j.name||'';
    if(j.personality!==undefined)pers.value=j.personality||'tierna';

    wifiInfo.innerHTML='PIXI-AI: <b>'+j.ap_ip+'</b><br>'+
      (j.sta_connected?('Casa: <b>'+j.sta_ssid+'</b> · IP <b>'+j.sta_ip+'</b> · prueba <b>pixi.local</b>'):
                       'Wi-Fi de casa: no conectado');
  }catch(e){}
}

txt.addEventListener('keydown',e=>{if(e.key==='Enter')sendText()});
voiceHelp.textContent=window.isSecureContext
  ? 'Puedes intentar el reconocimiento de voz del navegador.'
  : 'Chrome local HTTP: al tocar TOCA Y HABLA usare automaticamente el dictado del teclado si el sitio no puede acceder al microfono.';
setInterval(update,1500);
update();
</script>
</body>
</html>
)HTML";

// ---------------- RUTAS WEB ----------------
void setupWeb() {
  connectSavedWiFi();

  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html; charset=utf-8", WEB_PAGE);
  });

  server.on("/face", HTTP_GET, []() {
    if (server.hasArg("v")) {
      sleeping = false;
      setFace(faceFromString(server.arg("v")), 15000);
    }
    server.send(200, "text/plain", "OK");
  });

  server.on("/blink", HTTP_GET, []() {
    if (!sleeping) {
      blinking = true;
      blinkUntil = millis() + 180;
    }
    server.send(200, "text/plain", "OK");
  });

  server.on("/sleep", HTTP_GET, []() {
    sleeping = !sleeping;
    speechText = sleeping ? "Zzz..." : "¡Pi! Despierta.";
    speechUntil = millis() + 3000;
    setFace(sleeping ? FACE_SLEEPY : FACE_HAPPY, 4500);
    server.send(200, "text/plain", "OK");
  });

  server.on("/auto", HTTP_GET, []() {
    autoMode = !autoMode;
    server.send(200, "text/plain", autoMode ? "AUTO ON" : "AUTO OFF");
  });

  server.on("/random", HTTP_GET, []() {
    sleeping = false;
    setFace((Face)random(0, 18), 8000);
    server.send(200, "text/plain", "OK");
  });

  server.on("/brightness", HTTP_GET, []() {
    if (server.hasArg("v")) {
      brightnessValue = (uint8_t)constrain(server.arg("v").toInt(), 10, 255);
      lcd.setBrightness(brightnessValue);
    }
    server.send(200, "text/plain", "OK");
  });

  server.on("/listening", HTTP_GET, []() {
    listeningMode = server.hasArg("on") && server.arg("on") == "1";
    if (listeningMode) {
      sleeping = false;
      setFace(FACE_SURPRISED, 4000);
      speechText = "Te escucho...";
      speechUntil = millis() + 4000;
    }
    server.send(200, "text/plain", "OK");
  });

  server.on("/talk", HTTP_POST, []() {
    String heard = server.hasArg("text") ? server.arg("text") : "";
    heard.trim();
    if (heard.length() > 220) heard = heard.substring(0, 220);

    lastHeard = heard;
    noteUserInteraction(heard);
    listeningMode = false;
    sleeping = false;

    String f = "happy";
    String sound = "chirp";
    String reply = processUserMessage(heard, f, sound);

    setFace(faceFromString(f), 8000);
    lastReply = reply;
    lastSound = sound;
    rememberLocal(heard, reply);
    speechText = reply;
    speechUntil = millis() + 11000;
    logToSD("USER",heard);
    logToSD("PIXI",reply);
    saveLifeState();

    String json = "{\"ok\":true,\"reply\":\"" + jsonEscape(reply) +
                  "\",\"face\":\"" + jsonEscape(f) +
                  "\",\"sound\":\"" + jsonEscape(sound) + "\"}";
    server.send(200, "application/json; charset=utf-8", json);
  });


  server.on("/profile/save",HTTP_POST,[](){
    if(server.hasArg("name")){userName=server.arg("name").substring(0,40);userName.trim();}
    if(server.hasArg("personality")){
      String requested=server.arg("personality");requested.trim();
      if(!isValidPersonality(requested)){server.send(400,"text/plain","Personalidad no valida");return;}
      personality=requested;
    }
    saveLifeState();server.send(200,"text/plain","OK");
  });

  server.on("/alarm/save",HTTP_POST,[](){
    int requestedHour=server.hasArg("h")?server.arg("h").toInt():-1;
    int requestedMinute=server.hasArg("m")?server.arg("m").toInt():-1;
    if(requestedHour<0||requestedHour>23||requestedMinute<0||requestedMinute>59){server.send(400,"text/plain","Hora no valida");return;}
    alarmHour=requestedHour;alarmMinute=requestedMinute;
    if(server.hasArg("text"))alarmText=server.arg("text").substring(0,120);
    saveLifeState();server.send(200,"text/plain","OK");
  });

  server.on("/time",HTTP_POST,[](){
    if(server.hasArg("epoch")&&server.hasArg("offset")){
      syncTimeFromBrowser((time_t)server.arg("epoch").toInt(),server.arg("offset").toInt());
    }
    server.send(200,"text/plain","OK");
  });

  server.on("/desk",HTTP_GET,[](){deskMode=!deskMode;server.send(200,"text/plain",deskMode?"DESK":"FACE");});

  server.on("/mood/reset",HTTP_GET,[](){
    irritation=0;boredom=15;curiosity=55;happiness=max(happiness,70);server.send(200,"text/plain","OK");
  });

  server.on("/memory/export",HTTP_GET,[](){
    server.sendHeader("Content-Disposition","attachment; filename=pixi_memory.txt");
    server.send(200,"text/plain; charset=utf-8",exportMemoryText());
  });

  server.on("/memory/import",HTTP_POST,[](){
    if(server.hasArg("data"))importMemoryText(server.arg("data"));
    server.send(200,"text/plain","OK");
  });

  server.on("/update",HTTP_GET,[](){
    server.send(200,"text/html",
      "<meta name='viewport' content='width=device-width'><body style='font-family:sans-serif;background:#111;color:white;padding:25px'>"
      "<h2>PIXI OTA</h2><form method='POST' action='/update' enctype='multipart/form-data'>"
      "<input type='file' name='firmware' accept='.bin'><br><br><button>Actualizar</button></form></body>");
  });

  server.on("/update",HTTP_POST,
    [](){
      bool ok=!Update.hasError();
      server.send(ok?200:500,"text/plain",ok?"OK - reiniciando":"ERROR de actualizacion");
      if(ok){delay(600);ESP.restart();}
    },
    [](){
      HTTPUpload& upload=server.upload();
      if(upload.status==UPLOAD_FILE_START){if(!Update.begin(UPDATE_SIZE_UNKNOWN))Update.printError(Serial);}
      else if(upload.status==UPLOAD_FILE_WRITE){if(Update.write(upload.buf,upload.currentSize)!=upload.currentSize)Update.printError(Serial);}
      else if(upload.status==UPLOAD_FILE_END){if(!Update.end(true))Update.printError(Serial);}
    }
  );

  server.on("/wifi/save", HTTP_POST, []() {
    if (!server.hasArg("ssid")) {
      server.send(400, "text/plain", "Falta SSID.");
      return;
    }

    prefs.begin("pixiwifi", false);
    prefs.putString("ssid", server.arg("ssid"));
    prefs.putString("pass", server.hasArg("pass") ? server.arg("pass") : "");
    prefs.end();

    server.send(200, "text/plain", "Guardado. Reiniciando...");
    delay(700);
    ESP.restart();
  });

  server.on("/status", HTTP_GET, []() {
    String json = "{";
    json += "\"face\":\"" + String(faceName(face)) + "\",";
    json += "\"sleeping\":" + String(sleeping ? "true" : "false") + ",";
    json += "\"auto\":" + String(autoMode ? "true" : "false") + ",";
    json += "\"brightness\":" + String(brightnessValue) + ",";
    json += "\"last_heard\":\"" + jsonEscape(lastHeard) + "\",";
    json += "\"last_reply\":\"" + jsonEscape(lastReply) + "\",";
    json += "\"irritation\":" + String(irritation) + ",";
    json += "\"boredom\":" + String(boredom) + ",";
    json += "\"curiosity\":" + String(curiosity) + ",";
    json += "\"happiness\":" + String(happiness) + ",";
    json += "\"energy\":" + String(energy) + ",";
    json += "\"trust\":" + String(trustLevel) + ",";
    json += "\"name\":\"" + jsonEscape(userName) + "\",";
    json += "\"personality\":\"" + jsonEscape(personality) + "\",";
    json += "\"clock\":\"" + localClockText() + "\",";
    json += "\"secret\":" + String(secretMode?"true":"false") + ",";
    json += "\"sd\":" + String(sdReady?"true":"false") + ",";
    json += "\"ble\":" + String(bleConnected?"true":"false") + ",";
    json += "\"conversations\":" + String(conversationsCount) + ",";
    json += "\"games\":" + String(gamesCount) + ",";
    json += "\"touches\":" + String(touchCount) + ",";
    json += "\"safe_mode\":" + String(safeMode?"true":"false") + ",";
    json += "\"free_heap\":" + String(ESP.getFreeHeap()) + ",";
    json += "\"min_free_heap\":" + String(ESP.getMinFreeHeap()) + ",";
    json += "\"ap_ip\":\"" + WiFi.softAPIP().toString() + "\",";
    json += "\"sta_connected\":" + String(WiFi.status()==WL_CONNECTED ? "true":"false") + ",";
    json += "\"sta_ip\":\"" + (WiFi.status()==WL_CONNECTED ? WiFi.localIP().toString() : String("")) + "\",";
    json += "\"sta_ssid\":\"" + jsonEscape(WiFi.status()==WL_CONNECTED ? WiFi.SSID() : String("")) + "\"";
    json += "}";

    server.send(200, "application/json; charset=utf-8", json);
  });

  server.begin();
}

// ---------------- DIBUJO ----------------
void drawHeart(int x,int y,int s,uint16_t c){
  canvas.fillCircle(x-s/3,y-s/5,s/3,c);
  canvas.fillCircle(x+s/3,y-s/5,s/3,c);
  canvas.fillTriangle(x-s*2/3,y,x+s*2/3,y,x,y+s,c);
}

void drawEye(int cx,int cy,bool leftEye){
  int ew=66,eh=76;
  if(face==FACE_SURPRISED||face==FACE_SCARED||face==FACE_EXCITED||face==FACE_SHOCKED||face==FACE_STARRY){ew=74;eh=84;}
  if(face==FACE_SLEEPY||face==FACE_BORED||face==FACE_UNIMPRESSED||face==FACE_DEADPAN||face==FACE_ANNOYED)eh=46;

  if(sleeping||blinking){
    canvas.drawLine(cx-28,cy,cx+28,cy,C_WHITE);
    canvas.drawLine(cx-24,cy+2,cx+24,cy+2,C_WHITE);
    return;
  }

  if((face==FACE_WINK||face==FACE_PLAYFUL)&&!leftEye){
    canvas.drawLine(cx-26,cy,cx+26,cy,C_WHITE);
    canvas.drawLine(cx-22,cy+2,cx+22,cy+2,C_WHITE);
    return;
  }

  if(face==FACE_LOVE){drawHeart(cx,cy-3,24,C_PINK);return;}
  if(face==FACE_STARRY){canvas.setTextDatum(textdatum_t::middle_center);canvas.setTextColor(C_YELLOW,C_FACE);canvas.setFont(&fonts::Font4);canvas.drawString("*",cx,cy);return;}

  if(face==FACE_ROBOT||face==FACE_GLITCH){
    canvas.drawRect(cx-32,cy-35,64,70,C_CYAN);
    canvas.fillCircle(cx+(int)(gazeX*10),cy+(int)(gazeY*9),12,C_CYAN);
    if(face==FACE_GLITCH){canvas.drawLine(cx-35,cy-12,cx+35,cy-12,C_PINK);canvas.drawLine(cx-28,cy+18,cx+28,cy+18,C_YELLOW);}
    return;
  }

  canvas.fillEllipse(cx,cy,ew/2,eh/2,C_WHITE);
  int px=cx+(int)(gazeX*13.0f),py=cy+(int)(gazeY*12.0f);
  canvas.fillCircle(px,py,17,C_BLACK);
  canvas.fillCircle(px-5,py-6,5,C_WHITE);

  if(face==FACE_DIZZY){canvas.drawLine(px-11,py-11,px+11,py+11,C_WHITE);canvas.drawLine(px+11,py-11,px-11,py+11,C_WHITE);}

  if(face==FACE_ANGRY||face==FACE_FURIOUS||face==FACE_GRUMPY||face==FACE_MIDDLE_FINGER||face==FACE_REBEL){
    if(leftEye)canvas.fillTriangle(cx-40,cy-48,cx+40,cy-34,cx+40,cy-51,C_FACE);
    else canvas.fillTriangle(cx-40,cy-34,cx+40,cy-48,cx-40,cy-51,C_FACE);
  }

  if(face==FACE_SAD||face==FACE_CRYING||face==FACE_POUT||face==FACE_NERVOUS){
    if(leftEye)canvas.drawLine(cx-26,cy-40,cx+22,cy-47,C_WHITE);
    else canvas.drawLine(cx-22,cy-47,cx+26,cy-40,C_WHITE);
  }

  if(face==FACE_CONFUSED||face==FACE_SUSPICIOUS){
    if(leftEye)canvas.drawLine(cx-27,cy-45,cx+22,cy-39,C_WHITE);
    else canvas.drawLine(cx-22,cy-39,cx+27,cy-45,C_WHITE);
  }

  if(face==FACE_SMUG||face==FACE_SARCASM||face==FACE_PROUD||face==FACE_MISCHIEVOUS)canvas.fillRect(cx-38,cy-42,76,22,C_FACE);

  if(face==FACE_EMBARRASSED)canvas.fillCircle(px,py,8,C_BLACK);

  if(face==FACE_SICK){
    canvas.drawLine(cx-18,cy-5,cx+18,cy+5,C_BLACK);
    canvas.drawLine(cx-18,cy+5,cx+18,cy-5,C_BLACK);
  }
}

void drawMouth(){
  const int cx=160,cy=155;
  if(sleeping){canvas.drawArc(cx,cy,22,14,10,170,C_WHITE);return;}

  switch(face){
    case FACE_HAPPY: canvas.drawArc(cx,cy-5,27,22,25,155,C_WHITE); break;
    case FACE_VERY_HAPPY:
    case FACE_EXCITED:
      canvas.fillEllipse(cx,cy,26,18,C_BLACK);
      canvas.drawArc(cx,cy-4,27,23,20,160,C_WHITE);
      canvas.fillRect(cx-18,cy-13,36,5,C_WHITE); break;
    case FACE_SAD:
    case FACE_CRYING:
    case FACE_POUT:
    case FACE_NERVOUS:
    case FACE_ANGRY:
    case FACE_FURIOUS:
    case FACE_GRUMPY:
    case FACE_MIDDLE_FINGER: canvas.drawArc(cx,cy+12,25,18,205,335,C_WHITE); break;
    case FACE_SURPRISED:
    case FACE_SCARED:
    case FACE_SHOCKED:
      canvas.fillEllipse(cx,cy,13,18,C_BLACK);
      canvas.drawEllipse(cx,cy,13,18,C_WHITE); break;
    case FACE_THINKING:
      canvas.drawLine(cx-20,cy,cx+5,cy-3,C_WHITE);
      canvas.fillCircle(cx+16,cy-6,3,C_WHITE); break;
    case FACE_LOVE: canvas.drawArc(cx,cy-5,28,22,25,155,C_PINK); break;
    case FACE_WINK:
    case FACE_PLAYFUL: canvas.drawArc(cx,cy-4,23,17,25,155,C_WHITE); break;
    case FACE_CONFUSED:
    case FACE_SUSPICIOUS:
    case FACE_EMBARRASSED: canvas.drawLine(cx-18,cy-2,cx+18,cy+3,C_WHITE); break;
    case FACE_BORED:
    case FACE_SLEEPY:
    case FACE_UNIMPRESSED:
    case FACE_DEADPAN:
    case FACE_ANNOYED: canvas.drawLine(cx-15,cy,cx+15,cy,C_WHITE); break;
    case FACE_SICK:
    case FACE_DIZZY: canvas.drawArc(cx,cy+10,22,15,205,335,C_GREEN); break;
    case FACE_SMUG:
    case FACE_SARCASM:
    case FACE_PROUD:
    case FACE_MISCHIEVOUS:
    case FACE_REBEL: canvas.drawArc(cx+7,cy-2,24,15,15,145,C_WHITE); break;
    case FACE_TONGUE: canvas.drawArc(cx,cy-4,25,18,25,155,C_WHITE);canvas.fillEllipse(cx,cy+9,12,9,C_PINK);break;
    case FACE_GLITCH:
    case FACE_ROBOT: canvas.drawRect(cx-18,cy-8,36,16,C_CYAN); break;
    default: canvas.drawArc(cx,cy-2,20,13,30,150,C_WHITE); break;
  }
}

void drawExtras(){
  if(face==FACE_HAPPY||face==FACE_VERY_HAPPY||face==FACE_LOVE||face==FACE_EMBARRASSED||face==FACE_HAPPY_CRY){
    canvas.fillEllipse(53,145,18,6,C_PINK);
    canvas.fillEllipse(267,145,18,6,C_PINK);
  }

  if(face==FACE_CRYING||face==FACE_HAPPY_CRY){
    canvas.fillTriangle(92,124,98,143,104,124,C_CYAN);
    canvas.fillTriangle(216,124,222,143,228,124,C_CYAN);
  }

  if(face==FACE_MIDDLE_FINGER){
    canvas.fillRoundRect(246,133,34,39,8,C_YELLOW);
    canvas.fillRoundRect(258,78,11,70,5,C_YELLOW);
    canvas.fillRoundRect(237,141,13,25,5,C_YELLOW);
    canvas.fillRoundRect(276,141,13,25,5,C_YELLOW);
    canvas.setTextDatum(textdatum_t::middle_center);canvas.setTextColor(C_RED,C_FACE);canvas.setFont(&fonts::Font0);canvas.drawString("FUCK YOU",160,42);
  }

  if(face==FACE_REBEL){canvas.setTextDatum(textdatum_t::middle_center);canvas.setTextColor(C_PINK,C_FACE);canvas.drawString("NO RULES",160,42);}

  if(listeningMode){
    canvas.setTextDatum(textdatum_t::top_right);
    canvas.setTextColor(C_RED,C_FACE);
    canvas.drawString("REC",300,35);
  }

  if(millis()<animUntil){
    canvas.setTextDatum(textdatum_t::middle_center);
    if(animMode==ANIM_LAUGH){canvas.setTextColor(C_YELLOW,C_FACE);canvas.drawString("ha ha",160,42);}
    else if(animMode==ANIM_BLUSH){canvas.fillEllipse(52,145,24,8,C_PINK);canvas.fillEllipse(268,145,24,8,C_PINK);}
    else if(animMode==ANIM_SHIVER){canvas.setTextColor(C_CYAN,C_FACE);canvas.drawString("~ ~ ~",160,42);}
    else if(animMode==ANIM_SPARKLE){canvas.setTextColor(C_YELLOW,C_FACE);canvas.drawString("*  *  *",160,42);}
  }else animMode=ANIM_NONE;
}

void drawSpeechBubble(){
  if(speechText.length()==0||millis()>speechUntil)return;

  int bx=12,by=181,bw=296,bh=43;
  canvas.fillRoundRect(bx,by,bw,bh,10,C_WHITE);
  canvas.fillTriangle(147,by,160,by-9,173,by,C_WHITE);

  canvas.setTextColor(C_BLACK,C_WHITE);
  canvas.setTextDatum(textdatum_t::middle_center);
  canvas.setFont(&fonts::Font0);

  String t=speechText;
  if(t.length()<=42){
    canvas.drawString(t,160,by+22);
  }else{
    int split=42;
    while(split>20&&t[split]!=' ')split--;
    if(split<=20)split=42;

    String a=t.substring(0,split);
    String b=t.substring(split);
    b.trim();

    if(b.length()>42)b=b.substring(0,39)+"...";
    canvas.drawString(a,160,by+13);
    canvas.drawString(b,160,by+30);
  }
}

void drawStatus(){
  canvas.setFont(&fonts::Font0);
  canvas.setTextDatum(textdatum_t::top_left);
  canvas.setTextColor(C_PINK,C_BG);
  canvas.drawString("PIXI",8,6);

  canvas.setTextColor(C_GRAY,C_BG);
  canvas.drawString(faceName(face),42,6);

  canvas.setTextDatum(textdatum_t::top_right);
  if(listeningMode){
    canvas.setTextColor(C_RED,C_BG);
    canvas.drawString("ESCUCHANDO",274,6);
  }else if(irritation>=75){
    canvas.setTextColor(C_RED,C_BG);
    canvas.drawString("MOLESTA",274,6);
  }else if(boredom>=70){
    canvas.setTextColor(C_YELLOW,C_BG);
    canvas.drawString("ABURRIDA",274,6);
  }else{
    canvas.setTextColor(C_GRAY,C_BG);
    canvas.drawString("LIFE",274,6);
  }

  uint16_t wc = WiFi.status()==WL_CONNECTED ? C_GREEN : C_YELLOW;
  canvas.fillCircle(302,10,3,wc);
  canvas.drawArc(302,10,8,8,210,330,wc);
  canvas.drawArc(302,10,13,13,210,330,wc);

  canvas.setTextDatum(textdatum_t::bottom_right);
  canvas.setTextColor(C_GRAY,C_BG);
  canvas.drawString(WiFi.status()==WL_CONNECTED ? WiFi.localIP().toString() : String("192.168.4.1"),312,237);
}


void renderDashboard(){
  canvas.fillScreen(C_BG);
  canvas.setTextDatum(textdatum_t::top_left);
  canvas.setFont(&fonts::Font2);
  canvas.setTextColor(C_WHITE,C_BG);
  canvas.drawString("PIXI DESK",12,12);

  canvas.setFont(&fonts::Font4);
  canvas.setTextDatum(textdatum_t::middle_center);
  canvas.setTextColor(C_CYAN,C_BG);
  canvas.drawString(localClockText(),160,70);

  canvas.setFont(&fonts::Font0);
  canvas.setTextDatum(textdatum_t::top_left);
  canvas.setTextColor(C_WHITE,C_BG);
  canvas.drawString("Felicidad: "+String(happiness)+"%",18,112);
  canvas.drawString("Energia: "+String(energy)+"%",18,130);
  canvas.drawString("Aburrimiento: "+String(boredom)+"%",18,148);
  canvas.drawString("Curiosidad: "+String(curiosity)+"%",18,166);
  canvas.drawString("Confianza: "+String(trustLevel)+"%",18,184);
  canvas.drawString("Irritacion: "+String(irritation)+"%",18,202);

  canvas.setTextDatum(textdatum_t::top_right);
  canvas.setTextColor(C_GRAY,C_BG);
  canvas.drawString(bleConnected?"BLE ON":"BLE --",306,12);
  canvas.drawString(sdReady?"SD ON":"SD --",306,28);
  canvas.pushSprite(0,0);
}

void renderFace(){
  canvas.fillScreen(C_BG);
  canvas.fillRoundRect(8,25,304,151,24,C_FACE);

  drawEye(99,96,true);
  drawEye(221,96,false);
  drawMouth();
  drawExtras();
  drawSpeechBubble();
  drawStatus();

  if(touching&&!sleeping)canvas.drawCircle(touchX,touchY,4,C_ACCENT);
  canvas.pushSprite(0,0);
}

// ---------------- COMPORTAMIENTO ----------------
void updateBlink(){
  uint32_t now=millis();

  if(sleeping){blinking=false;return;}

  if(!blinking&&now>=nextBlinkAt){
    blinking=true;
    blinkUntil=now+random(80,150);
  }

  if(blinking&&now>=blinkUntil){
    blinking=false;
    nextBlinkAt=now+random(1800,5200);
  }
}

void updateGaze(){
  uint32_t now=millis();

  if(autoMode&&!touching&&!sleeping&&now>=nextLookAt){
    targetGazeX=random(-100,101)/100.0f;
    targetGazeY=random(-70,71)/100.0f;
    nextLookAt=now+random(900,3000);
  }

  gazeX+=(targetGazeX-gazeX)*0.18f;
  gazeY+=(targetGazeY-gazeY)*0.18f;
}

void updateFaceAutonomy(){
  if(!autoMode||sleeping||listeningMode)return;

  uint32_t now=millis();
  if(now>=nextFaceAt){
    int r=random(0,100);

    if(irritation>=75) face=FACE_ANGRY;
    else if(boredom>=75) face=FACE_BORED;
    else if(r<45)face=FACE_NEUTRAL;
    else if(r<62)face=FACE_HAPPY;
    else if(r<73)face=FACE_THINKING;
    else if(r<81)face=FACE_BORED;
    else if(r<89)face=FACE_WINK;
    else if(r<95)face=FACE_CONFUSED;
    else face=FACE_EXCITED;

    nextFaceAt=now+random(7000,16000);
  }
}

void handleTouch(){
  uint16_t x,y;
  bool pressed=lcd.getTouch(&x,&y);

  if(pressed){
    touchX=x;touchY=y;
    targetGazeX=constrain((x-160)/150.0f,-1.0f,1.0f);
    targetGazeY=constrain((y-100)/100.0f,-1.0f,1.0f);

    if(!touching){
      touching=true;
      touchStart=millis();
      touchStartX=x;touchStartY=y;
      touchCount++;
      happiness=min(happiness+2,100);
      trustLevel=min(trustLevel+1,100);
      checkAchievements();

      if(sleeping){
        sleeping=false;
        speechText="¡Pi! Ya desperte.";
        speechUntil=millis()+3500;
        setFace(FACE_SURPRISED,2500);
        return;
      }

      if(y<55){
        setFace(FACE_SURPRISED,2500);
      }else if(x<105){
        setFace(FACE_HAPPY,5000);
        speechText="¡Holii!";
        speechUntil=millis()+2500;
      }else if(x>215){
        setFace(FACE_THINKING,5000);
        speechText="Mmm?";
        speechUntil=millis()+2500;
      }else{
        setFace(FACE_LOVE,4000);
        speechText="Pi!";
        speechUntil=millis()+2200;
      }
    }

    if(!sleeping&&millis()-touchStart>2500){
      sleeping=true;
      face=FACE_SLEEPY;
      speechText="Zzz...";
      speechUntil=millis()+3000;
    }
  }else if(touching){
    uint32_t dur=millis()-touchStart;
    int dx=touchX-touchStartX;
    int dy=touchY-touchStartY;

    if(dur<500&&abs(dx)<35&&abs(dy)<35){
      if(millis()-lastTapAt<700)tapStreak++;
      else tapStreak=1;
      lastTapAt=millis();

      if(tapStreak==2){
        animMode=ANIM_LAUGH;animUntil=millis()+1800;
        setFace(FACE_VERY_HAPPY,2500);
        speechText="Jejeje... pi!";speechUntil=millis()+2200;
      }
      if(tapStreak>=3){
        secretMode=!secretMode;tapStreak=0;
        setFace(FACE_ROBOT,4500);
        speechText=secretMode?"🔐 Modo secreto":"🔓 Modo normal";
        speechUntil=millis()+3500;checkAchievements();
      }
    }else if(abs(dx)>55||abs(dy)>55){
      if(abs(dx)>abs(dy))targetGazeX=dx>0?1.0f:-1.0f;
      else targetGazeY=dy>0?1.0f:-1.0f;
      animMode=ANIM_SPARKLE;animUntil=millis()+900;
    }

    touching=false;
    nextLookAt=millis()+700;
  }
}

void bootAnimation(){
  canvas.fillScreen(C_BG);
  canvas.setTextDatum(textdatum_t::middle_center);
  canvas.setTextColor(C_WHITE,C_BG);
  canvas.setFont(&fonts::Font2);
  canvas.drawString("PIXI V8.5",160,92);
  canvas.setFont(&fonts::Font0);
  canvas.setTextColor(C_PINK,C_BG);
  canvas.drawString("LIFE",160,123);
  canvas.pushSprite(0,0);
  delay(600);

  for(int i=0;i<4;i++){
    canvas.fillScreen(C_BG);
    canvas.fillCircle(100,105,8+i*4,C_WHITE);
    canvas.fillCircle(220,105,8+i*4,C_WHITE);
    canvas.pushSprite(0,0);
    delay(90);
  }
}

// ---------------- SETUP ----------------
void setup(){
  Serial.begin(115200);
  delay(150);
  initSafeMode();
  randomSeed((uint32_t)esp_random());

  pinMode(LED_R,OUTPUT);
  pinMode(LED_G,OUTPUT);
  pinMode(LED_B,OUTPUT);
  setLed(false,false,false);

  lcd.init();
  lcd.setRotation(1);
  lcd.setBrightness(brightnessValue);
  lcd.fillScreen(C_BG);

  canvas.setColorDepth(8);
  if(!canvas.createSprite(320,240)){
    Serial.println("ERROR: no se pudo crear el buffer grafico");
  }

  bootAnimation();
  loadLifeState();
  if(!safeMode)setupSDCard();
  setupWeb();
  setupBLE();
  if(!safeMode)setupEspNow();

  nextBlinkAt=millis()+1500;
  nextLookAt=millis()+800;
  nextFaceAt=millis()+7000;
  nextSpontaneousAt=millis()+random(45000,80000);

  speechText=safeMode?"Modo seguro activo":"¡Holii! Soy Pixi";
  lastReply=speechText;
  speechUntil=millis()+4500;
  setFace(FACE_HAPPY,4500);

  Serial.println();
  Serial.println("PIXI V8.5 LIFE listo");
  Serial.print("AP: ");Serial.println(AP_NAME);
  Serial.print("AP IP: ");Serial.println(WiFi.softAPIP());

  if(WiFi.status()==WL_CONNECTED){
    Serial.print("Wi-Fi casa: ");Serial.println(WiFi.SSID());
    Serial.print("IP local: ");Serial.println(WiFi.localIP());
    Serial.println("Tambien prueba: http://pixi.local");
  }
}

void loop(){
  server.handleClient();
  updateSafeMode();

  updateBlink();
  handleTouch();
  updateGaze();
  updateFaceAutonomy();
  updateMoodEngine();
  updateDailyRoutine();
  saySpontaneously();
  broadcastPixiState();

  BleMessage bleMessage={};
  if(bleRxQueue!=nullptr&&xQueueReceive(bleRxQueue,&bleMessage,0)==pdTRUE)handleBleCommand(String(bleMessage.text));
  if(bleQueueOverflow){
    bleQueueOverflow=false;
    bleSendLine("{\"type\":\"error\",\"message\":\"Cola BLE llena; repite el ultimo comando\"}");
  }

  uint32_t now=millis();
  if(now-lastFrame>=45){
    lastFrame=now;
    if(deskMode)renderDashboard();
    else renderFace();
  }

  delay(1);
}
