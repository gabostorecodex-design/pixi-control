#include <WiFi.h>
#include <esp_now.h>
struct PixiNowPacket{char name[8];uint8_t happiness,energy,boredom,irritation,face;uint32_t conversations;};
void onReceive(const esp_now_recv_info_t* info,const uint8_t* data,int len){
  if(len!=sizeof(PixiNowPacket))return;PixiNowPacket p;memcpy(&p,data,sizeof(p));
  Serial.printf("PIXI feliz=%u energia=%u aburr=%u irrit=%u face=%u conv=%lu\n",p.happiness,p.energy,p.boredom,p.irritation,p.face,(unsigned long)p.conversations);
}
void setup(){Serial.begin(115200);WiFi.mode(WIFI_STA);if(esp_now_init()!=ESP_OK)return;esp_now_register_recv_cb(onReceive);}
void loop(){}
