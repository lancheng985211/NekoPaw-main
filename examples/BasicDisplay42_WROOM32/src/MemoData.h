#pragma once
#include <ArduinoJson.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
namespace memo_data {
constexpr int64_t clockMin=1704067200LL,clockMax=4102444799LL;
inline bool clockValid(int64_t now){return now>=clockMin&&now<=clockMax;}
inline bool textValid(const char* s,size_t bytes){
 if(!s||strlen(s)!=bytes||bytes>96)return false;
 size_t i=0,characters=0;
 while(i<bytes){
  uint32_t cp;unsigned more;unsigned char first=s[i++];
  if(first<128){cp=first;more=0;}
  else if(first>=0xC2&&first<=0xDF){cp=first&31;more=1;}
  else if(first>=0xE0&&first<=0xEF){cp=first&15;more=2;}
  else if(first>=0xF0&&first<=0xF4){cp=first&7;more=3;}
  else return false;
  if(i+more>bytes)return false;
  for(unsigned j=0;j<more;++j){unsigned char next=s[i++];if((next&0xC0)!=0x80)return false;cp=(cp<<6)|(next&63);}
  if((more==1&&cp<128)||(more==2&&cp<2048)||(more==3&&cp<65536)||cp>0x10FFFF||(cp>=0xD800&&cp<=0xDFFF)||cp<32||(cp>=127&&cp<=159))return false;
  if(++characters>64)return false;
 }
 return true;
}
inline bool itemsValid(JsonVariantConst value,const char*& reason){
 reason="备忘录格式无效";
 if(!value.is<JsonArrayConst>()||value.size()>8)return false;
 for(JsonObjectConst item:value.as<JsonArrayConst>()){
  if(!item["text"].is<const char*>()||!item["due"].is<int64_t>()||!item["face"].is<unsigned>())return false;
  JsonString text=item["text"].as<JsonString>();
  if(!textValid(text.c_str(),text.size())){reason="事件最多96字节（约32个汉字），不能含换行或控制字符";return false;}
  bool meaningful=false;for(size_t i=0;i<text.size();++i)if(text.c_str()[i]!=' '){meaningful=true;break;}
  if(!meaningful){reason="请填写事件内容";return false;}
  int64_t due=item["due"].as<int64_t>();
  if(due!=0&&!clockValid(due)){reason="目标时间须在2024至2099年内";return false;}
  if(item["face"].as<unsigned>()>7){reason="请选择有效表情";return false;}
 }
 return true;
}
inline bool storedValid(const JsonDocument& doc){const char* reason=nullptr;return doc["schema"]==1&&doc["revision"].is<uint32_t>()&&itemsValid(doc["items"],reason);}
inline void blank(JsonDocument& doc){doc.clear();doc["schema"]=1;doc["revision"]=0U;doc["items"].to<JsonArray>();}
inline void countdown(int64_t due,int64_t now,char* out,size_t capacity){
 if(!capacity)return;
 if(!due){snprintf(out,capacity,"--");return;}
 if(!clockValid(now)){snprintf(out,capacity,"待校时");return;}
 if(due<=now){snprintf(out,capacity,"已到期");return;}
 int64_t minutes=(due-now+59)/60,days=minutes/1440,hours=minutes/60%24,rest=minutes%60;
 if(days&&hours)snprintf(out,capacity,"%lld天%lld小时",(long long)days,(long long)hours);
 else if(days)snprintf(out,capacity,"%lld天",(long long)days);
 else if(minutes>=60&&rest)snprintf(out,capacity,"%lld小时%lld分",(long long)(minutes/60),(long long)rest);
 else if(minutes>=60)snprintf(out,capacity,"%lld小时",(long long)(minutes/60));
 else snprintf(out,capacity,"%lld分钟",(long long)minutes);
}
inline size_t removeLast(char* text,size_t length){
 if(!length)return 0;
 --length;while(length&&(static_cast<unsigned char>(text[length])&0xC0)==0x80)--length;
 text[length]=0;return length;
}
// measure(text, fontIndex) uses the actual font widths on the device.
template<class Measure> uint8_t fitText(const char* input,char* out,size_t capacity,int width,Measure measure){
 if(!capacity)return 2;
 size_t length=strlen(input);if(length>=capacity){length=capacity-1;while(length&&(static_cast<unsigned char>(input[length])&0xC0)==0x80)--length;}
 memcpy(out,input,length);out[length]=0;
 for(uint8_t font=0;font<3;++font)if(measure(out,font)<=width)return font;
 do{
  length=removeLast(out,length);
  if(length+3>=capacity)continue;
  memcpy(out+length,"...",4);
  if(measure(out,2)<=width)return 2;
  out[length]=0;
 }while(length);
 if(capacity>3&&measure("...",2)<=width)memcpy(out,"...",4);else out[0]=0;
 return 2;
}
} // namespace memo_data
