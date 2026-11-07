#pragma once
#include <ArduinoJson.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "WeatherSchedule.h"
namespace weather_data {
inline bool leap(int y){return y%4==0&&(y%100!=0||y%400==0);}
inline bool dateEpoch(const char* s,int64_t& epoch){
 if(!s||strlen(s)!=10||s[4]!='-'||s[7]!='-')return false;
 for(int i=0;i<10;++i)if(i!=4&&i!=7&&(s[i]<'0'||s[i]>'9'))return false;
 int y=(s[0]-'0')*1000+(s[1]-'0')*100+(s[2]-'0')*10+s[3]-'0';
 int m=(s[5]-'0')*10+s[6]-'0',d=(s[8]-'0')*10+s[9]-'0';
 static const int days[]={31,28,31,30,31,30,31,31,30,31,30,31};
 if(y<2024||y>2099||m<1||m>12||d<1||d>days[m-1]+(m==2&&leap(y)))return false;
 static const int before[]={0,31,59,90,120,151,181,212,243,273,304,334};int p=y-1;
 epoch=(int64_t(p)*365+p/4-p/100+p/400+before[m-1]+d-1+(m>2&&leap(y))-719162)*86400-28800;
 return true;
}
inline bool isoEpoch(const char* s,int64_t& epoch){
 if(!s||strlen(s)!=16||s[10]!='T'||s[13]!=':')return false;
 char date[11];memcpy(date,s,10);date[10]=0;if(!dateEpoch(date,epoch))return false;
 for(int i:{11,12,14,15})if(s[i]<'0'||s[i]>'9')return false;
 int h=(s[11]-'0')*10+s[12]-'0',m=(s[14]-'0')*10+s[15]-'0';
 if(h>23||m>59)return false;
 epoch+=h*3600+m*60;return true;
}
inline bool number(JsonVariantConst v){return v.is<float>()&&isfinite(v.as<float>());}
inline bool temperature(JsonVariantConst v){return number(v)&&v.as<float>()>=-100&&v.as<float>()<=100;}
inline bool probability(JsonVariantConst v){return v.isNull()||(number(v)&&v.as<float>()>=0&&v.as<float>()<=100);}
inline bool code(JsonVariantConst v){return v.is<int>()&&v.as<int>()>=0&&v.as<int>()<=99;}
inline bool validCache(const JsonDocument& doc){
 int64_t currentTime=0;
 if(!doc["fetchedAt"].is<int64_t>()||doc["fetchedAt"].as<int64_t>()<weather_schedule::kValidClock||
    !temperature(doc["current"]["temperature"])||!code(doc["current"]["code"])||
    !isoEpoch(doc["current"]["time"]|static_cast<const char*>(nullptr),currentTime)||
    !doc["daily"].is<JsonArrayConst>()||doc["daily"].size()<4||doc["daily"].size()>7)return false;
 int64_t previous=0;
 for(JsonObjectConst day:doc["daily"].as<JsonArrayConst>()){
  int64_t at=0;
  if(!dateEpoch(day["date"]|static_cast<const char*>(nullptr),at)||(previous&&at!=previous+86400)||
     !temperature(day["min"])||!temperature(day["max"])||day["min"].as<float>()>day["max"].as<float>()||!code(day["code"])||!probability(day["rain"]))return false;
  previous=at;
 }
 // Daily-only caches from the previous firmware remain usable.
 if(doc["hourly"].isNull())return true;
 if(!doc["hourly"].is<JsonArrayConst>()||doc["hourly"].size()<8||doc["hourly"].size()>48)return false;
 previous=0;
 for(JsonObjectConst h:doc["hourly"].as<JsonArrayConst>()){
  if(!h["time"].is<int64_t>())return false;
  int64_t at=h["time"].as<int64_t>();
  if(at<weather_schedule::kValidClock||at%3600||(previous&&at!=previous+3600)||!temperature(h["temperature"])||!code(h["code"])||!probability(h["rain"]))return false;
  previous=at;
 }
 return true;
}
inline bool normalize(const JsonDocument& raw,int64_t fetched,JsonDocument& out){
 if(raw["utc_offset_seconds"]!=28800||!raw["hourly"]["time"].is<JsonArrayConst>()||raw["hourly"]["time"].size()!=48||
    !raw["daily"]["time"].is<JsonArrayConst>()||raw["daily"]["time"].size()!=7)return false;
 for(const char* key:{"temperature_2m","weather_code","precipitation_probability"})
  if(!raw["hourly"][key].is<JsonArrayConst>()||raw["hourly"][key].size()!=48)return false;
 for(const char* key:{"weather_code","temperature_2m_max","temperature_2m_min","precipitation_probability_max"})
  if(!raw["daily"][key].is<JsonArrayConst>()||raw["daily"][key].size()!=7)return false;
 int64_t source=0,today=0;
 if(!isoEpoch(raw["current"]["time"]|static_cast<const char*>(nullptr),source)||
    !dateEpoch(raw["daily"]["time"][0]|static_cast<const char*>(nullptr),today)||
    fetched<weather_schedule::kValidClock||(source+28800)/86400!=(fetched+28800)/86400||
    today!=((fetched+28800)/86400)*86400-28800)return false;
 out.clear();out["schema"]=2;out["fetchedAt"]=fetched;
 out["current"]["temperature"]=raw["current"]["temperature_2m"];
 out["current"]["code"]=raw["current"]["weather_code"];out["current"]["time"]=raw["current"]["time"];
 JsonArray daily=out["daily"].to<JsonArray>();
 for(int i=0;i<7;++i){JsonObject d=daily.add<JsonObject>();d["date"]=raw["daily"]["time"][i];d["min"]=raw["daily"]["temperature_2m_min"][i];d["max"]=raw["daily"]["temperature_2m_max"][i];d["code"]=raw["daily"]["weather_code"][i];d["rain"]=raw["daily"]["precipitation_probability_max"][i];}
 JsonArray hours=out["hourly"].to<JsonArray>();
 for(int i=0;i<48;++i){
  int64_t at=0;if(!isoEpoch(raw["hourly"]["time"][i]|static_cast<const char*>(nullptr),at)||at%3600)return false;
  JsonObject h=hours.add<JsonObject>();h["time"]=at;h["temperature"]=raw["hourly"]["temperature_2m"][i];h["code"]=raw["hourly"]["weather_code"][i];h["rain"]=raw["hourly"]["precipitation_probability"][i];
 }
 const int64_t first=out["hourly"][0]["time"]|int64_t(0),last=out["hourly"][47]["time"]|int64_t(0);
 return validCache(out)&&first<=weather_schedule::nextSlot(fetched)&&last>=weather_schedule::nextSlot(fetched)+7*3600;
}
inline JsonObjectConst hourAt(JsonArrayConst hours,int64_t target){
 for(JsonObjectConst h:hours)if(h["time"].as<int64_t>()==target)return h;
 return JsonObjectConst();
}
inline void nextHours(JsonDocument& doc,int64_t now){
 JsonArray result=doc["nextHours"].to<JsonArray>();if(now<weather_schedule::kValidClock)return;
 for(int i=0;i<8;++i){
  int64_t target=weather_schedule::nextSlot(now)+i*3600;
  JsonObjectConst h=hourAt(doc["hourly"].as<JsonArrayConst>(),target);JsonObject cell=result.add<JsonObject>();
  if(!h.isNull())cell.set(h);else{cell["time"]=target;cell["missing"]=true;}
 }
}
} // namespace weather_data
