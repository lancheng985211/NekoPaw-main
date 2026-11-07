#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <time.h>
#include "NekoPawScreenHeader.h"
#include "CalendarLogic.h"

template<class DisplayT> class CalendarManager {
public:
  explicit CalendarManager(DisplayT& epd):epd_(epd) {}
  void begin() { font_.begin(epd_);font_.setFontMode(1);started_=true;Serial.println("Calendar service started (offline HKO 2026-2030; holidays 2026)"); }
  void setVisible(bool value) { visible_=value;signature_=""; }
  void loop() {
    if(!started_||!visible_||millis()-lastCheck_<1000)return;
    lastCheck_=millis();tm date{};bool valid=today(date);
    String next=valid?String(date.tm_year)+":"+date.tm_yday:String("waiting");
    const String minute=nekopaw_screen::minuteKey();
    if(next!=signature_){if(valid)render(date);else renderWaiting();signature_=next;headerMinute_=minute;}
    else if(valid&&minute!=headerMinute_){renderHeaderOnly();headerMinute_=minute;}
  }
  void snapshot(JsonDocument& out) {
    tm now{};bool clock=today(now);out["clockValid"]=clock;out["firstYear"]=calendar_data::kFirstYear;
    out["lastYear"]=calendar_data::kLastYear;out["holidayYear"]=2026;
    if(!clock){out["status"]="等待校准北京时间";return;}
    int y=now.tm_year+1900,m=now.tm_mon+1,d=now.tm_mday;
    out["year"]=y;out["month"]=m;out["day"]=d;out["weekday"]=now.tm_wday;
    out["firstWeekday"]=calendar_logic::weekday(y,m,1);out["rows"]=calendar_logic::rows(y,m);
    out["supported"]=calendar_logic::supported(y,m,d);out["holidayKnown"]=calendar_logic::holidayYearKnown(y);
    out["details"]=details(y,m,d);out["lunar"]=lunarText(y,m,d);out["note"]=note(y,m,d);
    out["source"]="香港天文台；国务院年度放假通知";
    auto days=out["days"].to<JsonArray>();
    for(int day=1;day<=calendar_logic::monthDays(y,m);++day){
      auto item=days.add<JsonObject>();item["day"]=day;item["label"]=calendar_logic::label(y,m,day);
      item["festival"]=calendar_logic::festival(y,m,day);item["term"]=calendar_logic::term(y,m,day);
      item["badge"]=badgeText(calendar_logic::badge(y,m,day));
    }
  }
private:
  DisplayT& epd_;U8G2_FOR_ADAFRUIT_GFX font_;bool started_=false,visible_=false;
  uint32_t lastCheck_=0;String signature_,headerMinute_;
  static bool today(tm& out) {
    time_t stamp=time(nullptr);if(stamp<1704067200LL||stamp>4102444799LL)return false;
    stamp+=28800;return gmtime_r(&stamp,&out)!=nullptr;
  }
  static const char* badgeText(char badge) { return badge=='R'?"休":badge=='W'?"班":""; }
  static String lunarText(int y,int m,int d) {
    using namespace calendar_logic;
    if(!supported(y,m,d))return "农历数据未收录";
    return String("农历")+(lunarLeap(y,m,d)?"闰":"")+kLunarMonths[lunarMonth(y,m,d)]+kLunarDays[lunarDay(y,m,d)];
  }
  static void appendName(String& value,const char* name) {
    if(!*name)return;if(value.length())value+=" · ";value+=name;
  }
  static String details(int y,int m,int d) {
    using namespace calendar_logic;String value;
    appendName(value,fixedFestival(m,d));
    if(!same(fixedFestival(m,d),lunarFestival(y,m,d)))appendName(value,lunarFestival(y,m,d));
    appendName(value,weekdayFestival(y,m,d));
    // Qingming is both a term and a festival; print the name only once.
    appendName(value,termIndex(y,m,d)==7?"清明节":term(y,m,d));
    return value;
  }
  static String note(int y,int m,int d) {
    using namespace calendar_logic;
    String value=lunarText(y,m,d);
    if(!holidayYearKnown(y))return value+" · 当年休班未收录";
    int index=holidayIndex(y,m,d);
    if(index>=0){const auto& h=kHolidays[index];char range[40];snprintf(range,sizeof(range)," · %02d/%02d-%02d/%02d 放假",h.month,h.first,h.month,h.last);value+=range;}
    else if(workday(y,m,d))value+=" · 调休上班";
    return value;
  }
  void colors(bool selected) { font_.setForegroundColor(selected?GxEPD_WHITE:GxEPD_BLACK);font_.setBackgroundColor(selected?GxEPD_BLACK:GxEPD_WHITE); }
  void text(int x,int baseline,String value,int maxWidth,const uint8_t* font=u8g2_font_wqy12_t_gb2312) {
    font_.setFont(font);
    while(value.length()&&font_.getUTF8Width(value.c_str())>maxWidth){
      int end=value.length()-1;while(end>0&&(static_cast<uint8_t>(value[end])&0xc0)==0x80)--end;value.remove(end);
    }
    font_.setCursor(x,baseline);font_.print(value);
  }
  void centered(int x,int baseline,int width,const String& value,const uint8_t* font=u8g2_font_wqy12_t_gb2312) {
    font_.setFont(font);int w=font_.getUTF8Width(value.c_str());text(x+(width-w)/2,baseline,value,width,font);
  }
  void renderHeaderOnly() {
    epd_.setRotation(0);epd_.setPartialWindow(0,0,400,40);epd_.firstPage();
    do{epd_.fillRect(0,0,400,40,GxEPD_WHITE);colors(false);nekopaw_screen::drawHeader(epd_,font_);}while(epd_.nextPage());
  }
  void renderWaiting() {
    epd_.setRotation(0);epd_.setFullWindow();epd_.firstPage();
    do{epd_.fillScreen(GxEPD_WHITE);colors(false);nekopaw_screen::drawHeader(epd_,font_);text(18,60,"日历",364,u8g2_font_wqy16_t_gb2312);
      text(18,122,"等待校准北京时间",364,u8g2_font_wqy16_t_gb2312);
      text(18,151,"联网自动校时，或打开设备的日历网页校时",364);
    }while(epd_.nextPage());
  }
  void render(const tm& now) {
    using namespace calendar_logic;
    int y=now.tm_year+1900,m=now.tm_mon+1,d=now.tm_mday;
    int first=weekday(y,m,1),rowCount=rows(y,m),count=monthDays(y,m);
    const char* weekdays[]={"日","一","二","三","四","五","六"};
    epd_.setRotation(0);epd_.setFullWindow();epd_.firstPage();
    do{
      epd_.fillScreen(GxEPD_WHITE);colors(false);nekopaw_screen::drawHeader(epd_,font_);
      text(18,60,String(y),70,u8g2_font_helvB18_tf);
      text(87,59,"年",20,u8g2_font_wqy16_t_gb2312);
      text(114,60,String(m),35,u8g2_font_helvB18_tf);
      text(m<10?137:151,59,"月",25,u8g2_font_wqy16_t_gb2312);
      text(338,60,"05 / 08",46,u8g2_font_6x10_tf);
      epd_.fillRect(18,66,364,2,GxEPD_BLACK);
      for(int col=0;col<7;++col)centered(18+52*col,88,52,weekdays[col]);
      for(int day=1;day<=count;++day){
        int index=first+day-1,col=index%7,row=index/7;
        int x=18+col*52,top=94+row*155/rowCount,bottom=94+(row+1)*155/rowCount;
        bool selected=day==d;if(selected)epd_.fillRect(x,top,52,bottom-top,GxEPD_BLACK);
        colors(selected);
        centered(x,top+(rowCount==6?14:17),52,String(day),u8g2_font_helvR12_tf);
        centered(x,bottom-3,52,label(y,m,day));
        char rest=badge(y,m,day);
        if(rest){uint16_t color=selected?GxEPD_WHITE:GxEPD_BLACK;epd_.drawRect(x+36,top+2,14,14,color);centered(x+37,top+13,12,badgeText(rest));}
      }
      colors(false);
      // Draw shared edges once, including blank cells. Today stays a solid black rectangle.
      for(int col=0;col<=7;++col)epd_.drawLine(18+col*52,94,18+col*52,249,GxEPD_BLACK);
      for(int row=0;row<=rowCount;++row)epd_.drawLine(18,94+row*155/rowCount,382,94+row*155/rowCount,GxEPD_BLACK);
      String names=details(y,m,d);
      String title=String(m)+"月"+d+"日 周"+weekdays[now.tm_wday];if(names.length())title+=" · "+names;
      text(18,271,title,364);text(18,291,note(y,m,d),364);
    }while(epd_.nextPage());
  }
};
