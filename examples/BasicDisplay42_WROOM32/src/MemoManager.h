#pragma once
#include <Preferences.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <time.h>
#include "MemoData.h"
#include "MemoFaces.h"
template<class DisplayT> class MemoManager {
public:
 explicit MemoManager(DisplayT& epd):epd_(epd){}
 void begin(){
  memo_data::blank(state_);storageReady_=prefs_.begin("nekomemo",false);
  if(!storageReady_)warning_="备忘录存储未就绪，暂不能保存";
  else if(prefs_.isKey("items")){
   String saved=prefs_.getString("items","");JsonDocument restored;
   if(!saved.length()||saved.length()>3072||deserializeJson(restored,saved)||!memo_data::storedValid(restored)){
    storageReady_=false;warning_="原备忘录读取失败，已保留原存储并禁止覆盖";
   }else state_=restored;
  }
  font_.begin(epd_);font_.setFontMode(1);font_.setForegroundColor(GxEPD_BLACK);font_.setBackgroundColor(GxEPD_WHITE);
  started_=true;Serial.println("Memo service started (8 items, adaptive 16/14/12px event text)");
 }
 void setVisible(bool value){if(value&&!visible_){dirty_=true;full_=true;}visible_=value;}
 void snapshot(JsonDocument& doc){
  doc.set(state_);doc["service"]="nekopaw-memo-v1";doc["now"]=int64_t(time(nullptr));
  doc["clockValid"]=memo_data::clockValid(time(nullptr));doc["storageReady"]=storageReady_;doc["warning"]=warning_;
 }
 bool accept(const JsonDocument& input,String& reason,int& status){
  status=400;
  if(!started_){reason="备忘录尚未启动";status=503;return false;}
  if(!storageReady_){reason=warning_;status=507;return false;}
  if(!input["revision"].is<uint32_t>()){reason="备忘录版本格式无效";return false;}
  uint32_t revision=state_["revision"]|0U;
  if(input["revision"].as<uint32_t>()!=revision){reason="备忘录已被其他页面修改，请先保留草稿再重新读取";status=409;return false;}
  const char* invalid=nullptr;if(!memo_data::itemsValid(input["items"],invalid)){reason=invalid;return false;}
  if(revision==UINT32_MAX){reason="版本计数已达上限，原数据保留";status=507;return false;}
  JsonDocument next;next["schema"]=1;next["revision"]=revision+1;JsonArray items=next["items"].to<JsonArray>();
  for(JsonObjectConst item:input["items"].as<JsonArrayConst>()){
   JsonObject out=items.add<JsonObject>();out["text"]=item["text"];out["due"]=item["due"];out["face"]=item["face"];
  }
  String json;serializeJson(next,json);
  if(json.length()>3072||prefs_.putString("items",json)!=json.length()){reason="备忘录保存失败，原数据仍保留";status=507;return false;}
  state_=next;dirty_=true;reason="";status=200;return true;
 }
 void loop(){
  if(!started_||!visible_||millis()-checkedAt_<250)return;checkedAt_=millis();
  int64_t now=time(nullptr),minute=memo_data::clockValid(now)?now/60:-1;
  if(!dirty_&&minute==lastMinute_)return;
  render(now,full_||partials_>=5);lastMinute_=minute;dirty_=false;full_=false;
 }
private:
 DisplayT& epd_;Preferences prefs_;JsonDocument state_;U8G2_FOR_ADAFRUIT_GFX font_;
 bool started_=false,storageReady_=false,visible_=false,dirty_=true,full_=true;
 String warning_;int64_t lastMinute_=-2;uint32_t checkedAt_=0;uint8_t partials_=0;
 const uint8_t* faceForFont(uint8_t n){return n==0?u8g2_font_wqy16_t_gb2312:n==1?u8g2_font_wqy14_t_gb2312:u8g2_font_wqy12_t_gb2312;}
 void text(int x,int baseline,const char* value,const uint8_t* face=u8g2_font_wqy12_t_gb2312){font_.setFont(face);font_.setCursor(x,baseline);font_.print(value);}
 void right(int edge,int baseline,const char* value,const uint8_t* face=u8g2_font_wqy12_t_gb2312){font_.setFont(face);text(edge-font_.getUTF8Width(value),baseline,value,face);}
 void eventText(int x,int top,const char* value){
  char fitted[100];uint8_t face=memo_data::fitText(value,fitted,sizeof(fitted),106,[this](const char* s,uint8_t f){font_.setFont(faceForFont(f));return int(font_.getUTF8Width(s));});
  text(x,top+(face==0?19:face==1?18:17),fitted,faceForFont(face));
 }
 void render(int64_t now,bool full){
  epd_.setRotation(0);if(full)epd_.setFullWindow();else epd_.setPartialWindow(0,0,400,300);
  epd_.firstPage();do{
   epd_.fillScreen(GxEPD_WHITE);text(16,31,"备忘录",u8g2_font_wqy16_t_gb2312);
   char stamp[48]="北京时间待校准";
   if(memo_data::clockValid(now)){time_t bj=now+28800;tm t{};gmtime_r(&bj,&t);strftime(stamp,sizeof(stamp),"%m/%d %H:%M",&t);}
   right(384,23,stamp,u8g2_font_6x10_tf);right(384,37,"北京时间");
   epd_.drawLine(16,41,384,41,GxEPD_BLACK);text(16,57,"时间（可选）");text(113,57,"事件");text(249,57,"表情");right(384,57,"倒计时");
   JsonArrayConst items=state_["items"].as<JsonArrayConst>();
   for(int i=0;i<8;i++){
    int top=62+i*26;epd_.drawLine(16,top+25,384,top+25,GxEPD_BLACK);
    if(i>=int(items.size())){text(16,top+17,"--",u8g2_font_6x10_tf);text(135,top+17,"--",u8g2_font_6x10_tf);continue;}
    JsonObjectConst item=items[i];int64_t due=item["due"]|int64_t(0);char when[20]="未设时间";
    if(due){time_t bj=due+28800;tm t{};gmtime_r(&bj,&t);strftime(when,sizeof(when),"%m/%d %H:%M",&t);text(16,top+17,when,u8g2_font_6x10_tf);}
    else text(16,top+17,when);
    epd_.drawRect(113,top+6,15,15,GxEPD_BLACK);char index[4];snprintf(index,sizeof(index),"%d",i+1);text(117,top+18,index,u8g2_font_6x10_tf);
    eventText(135,top,item["text"]|"");unsigned face=item["face"]|0U;
    epd_.drawBitmap(249,top+1,memo_faces::frames[face<8?face:0],32,24,GxEPD_BLACK);
    char remaining[48];memo_data::countdown(due,now,remaining,sizeof(remaining));right(384,top+17,remaining);
   }
   epd_.drawLine(16,274,384,274,GxEPD_BLACK);
   text(16,295,storageReady_?"手机管理：设备IP:81/memo":"存储异常，原数据保留",u8g2_font_wqy12_t_gb2312);
   text(352,295,"08/08",u8g2_font_6x10_tf);
  }while(epd_.nextPage());if(full)partials_=0;else++partials_;
 }
};
