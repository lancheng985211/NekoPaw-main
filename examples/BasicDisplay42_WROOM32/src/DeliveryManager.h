#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <time.h>
#include "NekoPawScreenHeader.h"

template<class DisplayT> class DeliveryManager {
public:
 explicit DeliveryManager(DisplayT& epd):epd_(epd){}
 void begin(){
  ready_=prefs_.begin("nekodelivery",false);
  if(ready_&&prefs_.isKey("latest")){JsonDocument cached;String reason;if(!deserializeJson(cached,prefs_.getString("latest",""))&&validate(cached,reason))state_.set(cached);}
  font_.begin(epd_);font_.setFontMode(1);font_.setForegroundColor(GxEPD_BLACK);font_.setBackgroundColor(GxEPD_WHITE);started_=true;
  Serial.println(state_["eventId"].isNull()?"Delivery: waiting for phone notification":"Delivery notification cache: loaded");
 }
 void setVisible(bool value){visible_=value;signature_="";}
 void snapshot(JsonDocument& out){
  out.set(state_);out["service"]="nekopaw-delivery-v1";out["hasData"]=!state_["eventId"].isNull();
  int64_t now=time(nullptr),at=state_["capturedAt"]|int64_t(0);bool clock=now>=1704067200LL&&now<=4102444799LL;
  out["clockValid"]=clock;out["older"]=!clock||(at>0&&(at>now||now-at>=14400));
  out["statusText"]=state_["state"].isNull()?"等待手机通知":name(state_["state"]|"");
 }
 bool accept(const JsonDocument& input,String& reason,int& status){
  if(!validate(input,reason)){status=400;return false;}
  String id=input["eventId"].as<String>();int64_t at=input["capturedAt"].as<int64_t>(),now=time(nullptr);
  if(id==state_["eventId"].as<String>()){status=200;return true;}
  if(at<(state_["capturedAt"]|int64_t(0))){reason="设备已有较新的外卖通知";status=409;return false;}
  if(now>=1704067200LL&&at>now+300){reason="手机通知时间超前，请检查手机与设备时间";status=422;return false;}
  JsonDocument candidate;candidate["schema"]=1;candidate["eventId"]=id;candidate["source"]="phone-notification";
  candidate["capturedAt"]=at;candidate["state"]=input["state"];candidate["eta"]=input["eta"];candidate["test"]=false;
  candidate["receivedAt"]=now>=1704067200LL?now:0;
  String json;serializeJson(candidate,json);
  if(!ready_||prefs_.putString("latest",json)!=json.length()){reason="通知保存失败，已保留原数据";status=507;return false;}
  state_.set(candidate);signature_="";status=200;return true;
 }
 void loop(){
  if(!started_||!visible_||millis()-lastCheck_<1000)return;lastCheck_=millis();JsonDocument data;snapshot(data);
  String next=String(data["eventId"]|"")+":"+(data["clockValid"]|false)+":"+(data["older"]|true);
  const String minute=nekopaw_screen::minuteKey();
  if(next!=signature_){render(data);signature_=next;headerMinute_=minute;}
  else if(minute!=headerMinute_){renderHeaderOnly();headerMinute_=minute;}
 }
private:
 DisplayT& epd_;Preferences prefs_;JsonDocument state_;U8G2_FOR_ADAFRUIT_GFX font_;bool ready_=false,started_=false,visible_=false;
 uint32_t lastCheck_=0;String signature_,headerMinute_;
 static const char* name(const String& state){
  if(state=="placed")return "已下单";if(state=="accepted")return "商家已接单";if(state=="preparing")return "商家备餐中";
  if(state=="ready")return "商家已出餐";if(state=="pickup")return "骑手取餐中";if(state=="delivering")return "骑手配送中";
  if(state=="delivered")return "商品已送达";if(state=="cancelled")return "订单已取消";return "";
 }
 static bool validate(const JsonDocument& input,String& reason){
  if(!input["schema"].is<int>()||input["schema"]!=1||input["source"]!="phone-notification"||
    !input["test"].is<bool>()||input["test"].as<bool>()||!input["capturedAt"].is<int64_t>()||
    input["capturedAt"].as<int64_t>()<1704067200LL||input["capturedAt"].as<int64_t>()>4102444799LL){reason="通知版本或时间格式无效";return false;}
  String id=input["eventId"]|"",state=input["state"]|"",eta=input["eta"]|"";
  if(id.length()!=64){reason="通知标识无效";return false;}
  for(char c:id)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f'))){reason="通知标识无效";return false;}
  if(!*name(state)){reason="未识别的外卖状态";return false;}
  if(!input["eta"].is<const char*>()||(eta.length()&&(eta.length()!=5||eta[2]!=':'||eta[0]<'0'||eta[0]>'2'||eta[1]<'0'||eta[1]>'9'||eta[3]<'0'||eta[3]>'5'||eta[4]<'0'||eta[4]>'9'||(eta[0]=='2'&&eta[1]>'3')))){reason="预计时间格式无效";return false;}
  if((state=="delivered"||state=="cancelled")&&eta.length()){reason="结束状态不应包含预计送达时间";return false;}return true;
 }
 static String stamp(int64_t epoch){if(epoch<1704067200LL)return "待校时";time_t value=epoch+28800;tm t{};gmtime_r(&value,&t);char out[20];strftime(out,sizeof(out),"%m/%d %H:%M",&t);return out;}
 void text(int x,int y,String value,int width,const uint8_t* font=u8g2_font_wqy12_t_gb2312){
  font_.setFont(font);while(value.length()&&font_.getUTF8Width(value.c_str())>width){int end=value.length()-1;while(end>0&&(static_cast<uint8_t>(value[end])&0xc0)==0x80)--end;value.remove(end);}font_.setCursor(x,y);font_.print(value);
 }
 void center(int y,String value,const uint8_t* font=u8g2_font_wqy16_t_gb2312){font_.setFont(font);text((400-font_.getUTF8Width(value.c_str()))/2,y,value,364,font);}
 void renderHeaderOnly(){
  epd_.setRotation(0);epd_.setPartialWindow(0,0,400,40);epd_.firstPage();
  do{epd_.fillRect(0,0,400,40,GxEPD_WHITE);nekopaw_screen::drawHeader(epd_,font_);}while(epd_.nextPage());
 }
 void render(const JsonDocument& data){
  bool has=data["hasData"]|false;String state=data["state"]|"",eta=data["eta"]|"";
  epd_.setRotation(0);epd_.setFullWindow();epd_.firstPage();
  do{
   epd_.fillScreen(GxEPD_WHITE);nekopaw_screen::drawHeader(epd_,font_);text(18,60,"外卖进度",270,u8g2_font_wqy16_t_gb2312);text(338,60,"07 / 08",44,u8g2_font_6x10_tf);epd_.drawLine(18,66,382,66,GxEPD_BLACK);
   center(88,"最近一条外卖通知");epd_.drawRect(18,99,364,49,GxEPD_BLACK);center(130,data["statusText"]|"等待手机通知");
   if(has){
    String estimate=(state=="delivered"||state=="cancelled")?"本次通知已结束":eta.length()?String("预计送达 ")+eta:"通知未提供预计送达时间";
    center(180,estimate);text(18,215,String("手机通知：")+stamp(data["capturedAt"]|int64_t(0)),364);
    text(18,240,String("设备收到：")+stamp(data["receivedAt"]|int64_t(0)),364);
   }else{center(190,"手机安装外卖助手并开启通知读取");center(222,"识别成功后通过同一热点自动同步");}
   epd_.drawLine(18,274,382,274,GxEPD_BLACK);text(18,293,"基于手机通知 · 只显示已收到的状态",290);text(314,293,!has?"未接入":(data["older"]|true)?"较早通知":"已接收",68);
  }while(epd_.nextPage());
 }
};
