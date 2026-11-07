#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <time.h>
#include "NekoPawScreenHeader.h"

namespace codex_usage {
constexpr int64_t kClockMin=1704067200LL,kClockMax=4102444799LL;
constexpr int64_t kStaleSeconds=900;
constexpr float remaining(float used) { return used>=100?0:used<0?100:100-used; }
constexpr bool expired(int64_t now,int64_t reset) { return now>=kClockMin&&reset>=kClockMin&&now>=reset; }
constexpr bool stale(int64_t now,int64_t fetched) { return now<kClockMin||now<fetched||now-fetched>=kStaleSeconds; }
static_assert(remaining(13)==87&&remaining(100)==0&&remaining(105)==0,"Remaining is not used percentage");
static_assert(!expired(kClockMin+9,kClockMin+10)&&expired(kClockMin+10,kClockMin+10),"Do not invent fresh data at reset");
static_assert(stale(0,kClockMin)&&stale(kClockMin+900,kClockMin)&&!stale(kClockMin+899,kClockMin),"Cache age boundary");
}

template<class DisplayT> class CodexUsageManager {
public:
  explicit CodexUsageManager(DisplayT& epd):epd_(epd) {}
  void begin() {
    ready_=prefs_.begin("nekocodex",false);
    if(ready_&&prefs_.isKey("usage")){
      JsonDocument cached;String reason;
      if(!deserializeJson(cached,prefs_.getString("usage",""))&&validate(cached,reason))state_.set(cached);
    }
    font_.begin(epd_);font_.setFontMode(1);font_.setForegroundColor(GxEPD_BLACK);font_.setBackgroundColor(GxEPD_WHITE);started_=true;
    Serial.println(state_["fetchedAt"].isNull()?"Codex usage: waiting for PC sync":"Codex usage cache: loaded");
  }
  void setVisible(bool value){visible_=value;signature_="";}
  void snapshot(JsonDocument& out){
    out.set(state_);int64_t now=time(nullptr),fetched=out["fetchedAt"]|int64_t(0);
    out["hasData"]=fetched>0;out["clockValid"]=now>=codex_usage::kClockMin&&now<=codex_usage::kClockMax;
    out["stale"]=codex_usage::stale(now,fetched);out["syncInterval"]=300;
    for(const char* key:{"primary","secondary"}){
      JsonVariantConst cached=state_[key];auto window=out[key].to<JsonObject>();
      if(!cached.isNull())window.set(cached.as<JsonObjectConst>());
      bool known=!cached["usedPercent"].isNull();
      int64_t reset=cached["resetsAt"]|int64_t(0);window["expired"]=codex_usage::expired(now,reset);
      if(known)window["remainingPercent"]=codex_usage::remaining(cached["usedPercent"].as<float>());
      else window["remainingPercent"]=nullptr;
    }
  }
  bool accept(const JsonDocument& input,String& reason,int& status){
    if(!validate(input,reason)){status=400;return false;}
    int64_t fetched=input["fetchedAt"].as<int64_t>(),old=state_["fetchedAt"]|int64_t(0),now=time(nullptr);
    if(fetched<old){reason="这份用量比设备缓存更旧，已保留原数据";status=409;return false;}
    if(now>=codex_usage::kClockMin&&(fetched>now+120||fetched<now-120)){reason="电脑和设备时间不一致，请先校准时间";status=409;return false;}
    // Whitelist fields: auth tokens, account IDs and other upstream metadata never enter NVS.
    JsonDocument candidate;candidate["schema"]=1;candidate["limitId"]="codex";candidate["fetchedAt"]=fetched;candidate["plan"]=input["plan"]|"";
    for(const char* key:{"primary","secondary"}){
      JsonVariantConst source=input[key];
      if(source.isNull()){candidate[key]=nullptr;continue;}
      for(const char* field:{"usedPercent","windowMinutes","resetsAt"})candidate[key][field]=source[field];
    }
    if(input["tokens"].isNull()){if(!state_["tokens"].isNull())candidate["tokens"].set(state_["tokens"]);}
    else candidate["tokens"].set(input["tokens"]);
    String serialized;serializeJson(candidate,serialized);
    if(!ready_||prefs_.putString("usage",serialized)!=serialized.length()){reason="用量保存失败，旧缓存保留";status=507;return false;}
    state_.set(candidate);signature_="";status=200;return true;
  }
  void loop(){
    if(!started_||!visible_||millis()-lastCheck_<1000)return;lastCheck_=millis();
    JsonDocument data;snapshot(data);String signature;
    // Change only on a new snapshot / stale state / reset boundary, not every second.
    signature=String(data["fetchedAt"]|int64_t(0))+":"+(data["clockValid"]|false)+":"+(data["stale"]|true)+":"+
      (data["primary"]["expired"]|false)+":"+(data["secondary"]["expired"]|false);
    const String minute=nekopaw_screen::minuteKey();
    if(signature!=signature_){render(data);signature_=signature;headerMinute_=minute;}
    else if(minute!=headerMinute_){renderHeaderOnly();headerMinute_=minute;}
  }
private:
  DisplayT& epd_;Preferences prefs_;JsonDocument state_;U8G2_FOR_ADAFRUIT_GFX font_;
  bool ready_=false,started_=false,visible_=false;uint32_t lastCheck_=0;String signature_,headerMinute_;
  static bool validWindow(JsonVariantConst window){
    if(window.isNull())return true;if(!window.is<JsonObjectConst>())return false;
    JsonVariantConst used=window["usedPercent"],minutes=window["windowMinutes"],reset=window["resetsAt"];
    if(!used.isNull()&&(!used.is<float>()||!isfinite(used.as<float>())||used.as<float>()<0||used.as<float>()>10000))return false;
    if(!minutes.isNull()&&(!minutes.is<int>()||minutes.as<int>()<1||minutes.as<int>()>525600))return false;
    if(!reset.isNull()&&(!reset.is<int64_t>()||reset.as<int64_t>()<codex_usage::kClockMin||reset.as<int64_t>()>codex_usage::kClockMax))return false;
    return true;
  }
  static bool validCount(JsonVariantConst value){
    return value.is<uint64_t>()&&value.as<uint64_t>()<=9000000000000000ULL;
  }
  static bool validTokens(JsonVariantConst tokens){
    if(tokens.isNull())return true;
    if(!tokens.is<JsonObjectConst>())return false;
    JsonVariantConst today=tokens["today"];
    if(!today.is<JsonObjectConst>()||!validCount(today["totalTokens"])||!validCount(today["inputTokens"])||!validCount(today["outputTokens"]))return false;
    JsonVariantConst history=tokens["history"];
    if(history.isNull())return true;
    if(!history.is<JsonArrayConst>()||history.as<JsonArrayConst>().size()>7)return false;
    for(JsonVariantConst day:history.as<JsonArrayConst>()){
      if(!day.is<JsonObjectConst>()||!day["date"].is<const char*>()||day["date"].as<String>().length()>10||!validCount(day["totalTokens"]))return false;
    }
    return true;
  }
  static bool validate(const JsonDocument& input,String& reason){
    if(input["schema"]!=1||input["limitId"]!="codex"||!input["fetchedAt"].is<int64_t>()||
       input["fetchedAt"].as<int64_t>()<codex_usage::kClockMin||input["fetchedAt"].as<int64_t>()>codex_usage::kClockMax){reason="用量数据版本或时间无效";return false;}
    if(!input["plan"].is<const char*>()||input["plan"].as<String>().length()>24){reason="账户方案名称无效";return false;}
    if(!validWindow(input["primary"])||!validWindow(input["secondary"])||!validTokens(input["tokens"])||
       (input["primary"]["usedPercent"].isNull()&&input["secondary"]["usedPercent"].isNull())){reason="额度或 Token 统计数据格式无效";return false;}
    return true;
  }
  static String tokenCount(JsonVariantConst value){
    if(!value.is<uint64_t>())return "--";
    return String(value.as<uint64_t>()/10000.0,1);
  }
  static String stamp(int64_t epoch){
    if(epoch<codex_usage::kClockMin)return "未知";time_t shifted=epoch+28800;tm t{};gmtime_r(&shifted,&t);char value[20];strftime(value,sizeof(value),"%m/%d %H:%M",&t);return value;
  }
  void text(int x,int baseline,String value,int width,const uint8_t* font=u8g2_font_wqy12_t_gb2312){
    font_.setFont(font);while(value.length()&&font_.getUTF8Width(value.c_str())>width){int end=value.length()-1;while(end>0&&(static_cast<uint8_t>(value[end])&0xc0)==0x80)--end;value.remove(end);}
    font_.setCursor(x,baseline);font_.print(value);
  }
  static String title(JsonVariantConst window,int index){
    int minutes=window["windowMinutes"]|0;
    if(minutes==10080)return "每周额度剩余";
    if(minutes&&minutes%60==0)return String(minutes/60)+" 小时额度剩余";
    if(minutes)return String(minutes)+" 分钟额度剩余";
    return index==0?"当前额度剩余":"其他额度剩余";
  }
  void renderHeaderOnly(){
    epd_.setRotation(0);epd_.setPartialWindow(0,0,400,40);epd_.firstPage();
    do{epd_.fillRect(0,0,400,40,GxEPD_WHITE);nekopaw_screen::drawHeader(epd_,font_);}while(epd_.nextPage());
  }
  void render(const JsonDocument& data){
    epd_.setRotation(0);epd_.setFullWindow();epd_.firstPage();
    do{
      epd_.fillScreen(GxEPD_WHITE);nekopaw_screen::drawHeader(epd_,font_);text(18,60,"Codex 用量",290,u8g2_font_wqy16_t_gb2312);
      text(338,60,"06 / 08",44,u8g2_font_6x10_tf);epd_.fillRect(18,66,364,2,GxEPD_BLACK);
      int index=0;for(const char* key:{"primary","secondary"}){
        int x=index?205:18;JsonVariantConst window=data[key];bool expired=window["expired"]|false,known=!window["remainingPercent"].isNull();
        epd_.drawRect(x,72,177,74,GxEPD_BLACK);text(x+9,90,title(window,index),112);
        String percent=known&&!expired?String(int(round(window["remainingPercent"].as<float>())))+"%":"--";
        text(x+121,91,percent,48,u8g2_font_helvB14_tf);
        epd_.drawRect(x+9,101,159,9,GxEPD_BLACK);
        if(known&&!expired){int fill=int(round(153*window["remainingPercent"].as<float>()/100));if(fill>0)epd_.fillRect(x+12,104,fill,3,GxEPD_BLACK);}
        String reset=expired?"等待额度同步":String("重置 ")+stamp(window["resetsAt"]|int64_t(0));
        text(x+9,135,reset,158,u8g2_font_wqy12_t_gb2312);++index;
      }
      epd_.drawRect(18,156,364,124,GxEPD_BLACK);text(30,177,"Token 消耗",220,u8g2_font_wqy12_t_gb2312);
      text(310,176,"单位：万",64,u8g2_font_wqy12_t_gb2312);epd_.fillRect(30,184,340,1,GxEPD_BLACK);
      JsonVariantConst today=data["tokens"]["today"];
      text(30,204,"今日总消耗",110,u8g2_font_wqy12_t_gb2312);
      text(30,232,tokenCount(today["totalTokens"]),120,u8g2_font_helvB18_tf);
      text(170,207,"输入",55,u8g2_font_wqy12_t_gb2312);text(170,232,tokenCount(today["inputTokens"]),80,u8g2_font_helvB14_tf);
      text(264,207,"输出",55,u8g2_font_wqy12_t_gb2312);text(264,232,tokenCount(today["outputTokens"]),80,u8g2_font_helvB14_tf);
      epd_.drawLine(30,241,370,241,GxEPD_BLACK);text(30,263,"近 7 天",70,u8g2_font_wqy12_t_gb2312);
      JsonArrayConst history=data["tokens"]["history"].as<JsonArrayConst>();
      if(!(data["tokens"]["complete"]|true))text(112,263,"部分压缩日志未计入",245,u8g2_font_wqy12_t_gb2312);
      else if(history.isNull()||history.size()==0)text(112,263,"暂无 Token 统计数据",245,u8g2_font_wqy12_t_gb2312);
      else{
        uint64_t maxCount=1;for(JsonVariantConst day:history)if(day["totalTokens"].as<uint64_t>()>maxCount)maxCount=day["totalTokens"].as<uint64_t>();
        int i=0;for(JsonVariantConst day:history){int h=max(3,int(23.0*day["totalTokens"].as<uint64_t>()/maxCount));epd_.fillRect(120+i*34,266-h,20,h,GxEPD_BLACK);++i;}
      }
      bool has=data["hasData"]|false,clock=data["clockValid"]|false,stale=data["stale"]|true;
      text(18,296,has?String("额度同步：")+stamp(data["fetchedAt"]|int64_t(0)):"等待电脑首次同步",245);
      text(270,296,!has?"暂无额度":!clock?"待校时":stale?"缓存/待同步":"已同步",112);
    }while(epd_.nextPage());
  }
};
