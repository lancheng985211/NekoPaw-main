#pragma once
#include <ArduinoJson.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <U8g2_for_Adafruit_GFX.h>
#include "NekoPawScreenHeader.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "WeatherSchedule.h"
#include "WeatherData.h"
#include "WeatherRootCA.h"
#include "CourseSchedule.h"

template<class DisplayT> class WeatherManager {
public:
  explicit WeatherManager(DisplayT& epd) : epd_(epd) {}
  void begin() {
    mutex_ = xSemaphoreCreateMutex();
    if (!mutex_) { Serial.println("Weather mutex allocation failed"); return; }
    prefsReady_ = prefs_.begin("nekoweather", false);
    JsonDocument cached;
    if (prefsReady_ && prefs_.isKey("cache") && !deserializeJson(cached, prefs_.getString("cache", "")) && validCache(cached)) state_ = cached;
    font_.begin(epd_); font_.setFontMode(1); font_.setForegroundColor(GxEPD_BLACK); font_.setBackgroundColor(GxEPD_WHITE);
    Serial.println(state_["fetchedAt"].isNull() ? "Weather cache: empty" : "Weather cache: loaded");
    started_ = true;
    if (xTaskCreate(taskEntry, "weather", 12288, this, 1, &task_) != pdPASS) {
      task_ = nullptr; error_ = "天气任务启动失败";
    }
    Serial.println(task_ ? "Weather service started (hourly-v3; Beijing hourly refresh, next 8 hours)" : "Weather service task failed");
    Serial.println("Weather status page: http://<device-ip>:81/weather");
  }
  void setVisible(bool value) { visible_ = value; signature_ = ""; }
  void snapshot(JsonDocument& output) {
    if (!mutex_) { output["status"] = "天气未启动"; return; }
    xSemaphoreTake(mutex_, portMAX_DELAY); output.set(state_);
    output["error"] = error_; output["cacheSaved"] = cacheSaved_; output["fetching"] = fetching_;
    xSemaphoreGive(mutex_);
    int64_t now = time(nullptr);
    output["city"] = "长沙"; output["online"] = WiFi.status() == WL_CONNECTED;
    output["clockValid"] = now >= weather_schedule::kValidClock;
    output["stale"] = now < weather_schedule::kValidClock || weather_schedule::due(now, output["fetchedAt"] | int64_t(0)) || !output["hourly"].is<JsonArray>();
    output["now"] = now; output["updateSchedule"] = "hourly"; output["service"] = "weather-hourly-v3";
    weather_data::nextHours(output, now);
    output["nextUpdate"] = now >= weather_schedule::kValidClock ? weather_schedule::nextSlot(now) : 0;
  }
  void loop() {
    if (!started_ || !visible_ || millis() - lastPaintCheck_ < 1000) return;
    lastPaintCheck_ = millis(); JsonDocument data; snapshot(data);
    // Connection, clock and cached data changes repaint; transient HTTP attempts do not.
    String next = String(data["fetchedAt"] | int64_t(0)) + ":" + (data["online"] | false) + ":" + (data["stale"] | true) + ":" +
      (data["clockValid"] | false) + ":" + String(time(nullptr) / 3600);
    const String minute=nekopaw_screen::minuteKey();
    if (next != signature_) { render(data); signature_ = next; headerMinute_=minute; }
    else if(minute!=headerMinute_){renderHeaderOnly();headerMinute_=minute;}
  }
private:
  DisplayT& epd_; Preferences prefs_; JsonDocument state_;
  SemaphoreHandle_t mutex_ = nullptr; TaskHandle_t task_ = nullptr;
  U8G2_FOR_ADAFRUIT_GFX font_;
  bool started_ = false, visible_ = false, prefsReady_ = false, cacheSaved_ = true, fetching_ = false;
  String error_, signature_, headerMinute_; uint32_t lastPaintCheck_ = 0;
  static bool validCache(const JsonDocument& doc) { return weather_data::validCache(doc); }
  static void taskEntry(void* arg) { static_cast<WeatherManager*>(arg)->worker(); }
  void worker() {
    uint32_t lastAttempt = 0; int64_t attemptSlot = 0; bool attempted = false, wasOnline = false;
    bool waitingForClock = false;
    for (;;) {
      bool online = WiFi.status() == WL_CONNECTED, reconnected = online && !wasOnline; wasOnline = online;
      int64_t now = time(nullptr), fetched; bool hourlyReady;
      if (now < weather_schedule::kValidClock) {
        if (!waitingForClock) Serial.println("Weather waiting for Beijing time: open the course or weather webpage to sync");
        waitingForClock = true;
      } else if (waitingForClock) { Serial.println("Weather clock ready"); waitingForClock = false; }
      xSemaphoreTake(mutex_, portMAX_DELAY); fetched = state_["fetchedAt"] | int64_t(0); hourlyReady = state_["hourly"].is<JsonArray>() && state_["hourly"].size() >= 8; xSemaphoreGive(mutex_);
      if (online && now >= weather_schedule::kValidClock && (weather_schedule::due(now, fetched) || !hourlyReady)) {
        int64_t slot = weather_schedule::latestSlot(now);
        if (!attempted || reconnected || slot != attemptSlot || uint32_t(millis() - lastAttempt) >= weather_schedule::kRetryMs) {
          attempted = true; attemptSlot = slot; lastAttempt = millis();
          xSemaphoreTake(mutex_, portMAX_DELAY); fetching_ = true; xSemaphoreGive(mutex_);
          JsonDocument fresh; String failure;
          if (fetch(fresh, failure)) {
            String json; serializeJson(fresh, json);
            bool saved = prefsReady_ && prefs_.putString("cache", json) == json.length();
            xSemaphoreTake(mutex_, portMAX_DELAY); state_ = fresh; error_ = saved ? "" : "天气已更新，但缓存保存失败"; cacheSaved_ = saved; xSemaphoreGive(mutex_);
            Serial.println(saved ? "Weather updated and cached" : "Weather updated; cache write failed");
          } else {
            xSemaphoreTake(mutex_, portMAX_DELAY); error_ = failure; xSemaphoreGive(mutex_);
            Serial.println(String("Weather fetch failed: ") + failure);
          }
          xSemaphoreTake(mutex_, portMAX_DELAY); fetching_ = false; xSemaphoreGive(mutex_);
        }
      }
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }
  bool fetch(JsonDocument& output, String& failure) {
    WiFiClientSecure tls; tls.setCACert(kWeatherRootCA); tls.setHandshakeTimeout(12);
    HTTPClient http; http.setConnectTimeout(10000); http.setTimeout(12000); http.useHTTP10(true);
    const char* url = "https://api.open-meteo.com/v1/forecast?latitude=28.19874&longitude=112.97087&current=temperature_2m,weather_code&hourly=temperature_2m,weather_code,precipitation_probability&forecast_hours=48&temperature_unit=celsius&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max&timezone=Asia%2FShanghai&forecast_days=7";
    if (!http.begin(tls, url)) { failure = "无法初始化HTTPS"; return false; }
    int status = http.GET();
    if (status != 200) { failure = String("天气接口状态 ") + status; http.end(); return false; }
    JsonDocument raw;
    auto result = deserializeJson(raw, http.getStream(), DeserializationOption::NestingLimit(6)); http.end();
    if (result || !weather_data::normalize(raw, int64_t(time(nullptr)), output)) {
      failure = "天气逐小时数据或日期无效，保留旧缓存"; return false;
    }
    return true;
  }
  static const char* description(int code) {
    if (code < 0) return "未知";
    if (code == 0) return "晴"; if (code <= 2) return "多云"; if (code == 3) return "阴";
    if (code == 45 || code == 48) return "雾"; if (code >= 95) return "雷雨";
    if (code >= 71 && code <= 77 || code == 85 || code == 86) return "雪";
    if (code == 65 || code == 82) return "大雨"; if (code >= 51 && code <= 67 || code >= 80 && code <= 82) return "雨";
    return "未知";
  }
  void text(int x, int y, const String& s, bool small = false) {
    font_.setFont(small ? u8g2_font_6x10_tf : u8g2_font_wqy16_t_gb2312); font_.setCursor(x,y); font_.print(s);
  }
  void label(int x, int y, const String& value) {
    font_.setFont(u8g2_font_wqy12_t_gb2312); font_.setCursor(x,y); font_.print(value);
  }
  void center(int x,int y,int width,const String& value,const uint8_t* face=u8g2_font_6x10_tf){
    font_.setFont(face); font_.setCursor(x+(width-font_.getUTF8Width(value.c_str()))/2,y);font_.print(value);
  }
  void icon(int x,int y,int code,int scale=1){
    if(code<0){text(x+8,y+18,"?",true);return;}
    if(code==0){
      epd_.drawCircle(x+12*scale,y+10*scale,5*scale,GxEPD_BLACK);
      for(int i=0;i<8;i++){float a=i*PI/4;epd_.drawLine(x+(12+cos(a)*8)*scale,y+(10+sin(a)*8)*scale,x+(12+cos(a)*10)*scale,y+(10+sin(a)*10)*scale,GxEPD_BLACK);}
      return;
    }
    epd_.drawCircle(x+7*scale,y+9*scale,5*scale,GxEPD_BLACK);
    epd_.drawCircle(x+13*scale,y+6*scale,6*scale,GxEPD_BLACK);
    epd_.drawCircle(x+19*scale,y+10*scale,4*scale,GxEPD_BLACK);
    epd_.fillRect(x+5*scale,y+9*scale,15*scale,5*scale,GxEPD_WHITE);
    epd_.drawLine(x+5*scale,y+14*scale,x+20*scale,y+14*scale,GxEPD_BLACK);
    bool snow=(code>=71&&code<=77)||code==85||code==86;
    bool rain=(code>=51&&code<=67)||(code>=80&&code<=82)||code>=95;
    if(snow)for(int i=0;i<3;i++)epd_.fillCircle(x+(6+i*6)*scale,y+19*scale,scale,GxEPD_BLACK);
    else if(rain)for(int i=0;i<3;i++)epd_.drawLine(x+(7+i*6)*scale,y+17*scale,x+(5+i*6)*scale,y+21*scale,GxEPD_BLACK);
    else if(code==45||code==48)epd_.drawLine(x+4*scale,y+19*scale,x+21*scale,y+19*scale,GxEPD_BLACK);
  }
  void weatherHeader(){
    text(16,29,"长沙");
    String stamp="北京时间待校准";const int64_t now=time(nullptr);
    if(now>=weather_schedule::kValidClock){time_t bj=now+28800;tm t{};gmtime_r(&bj,&t);char out[32];strftime(out,sizeof(out),"%m/%d %H:%M",&t);const char* weeks[]={"日","一","二","三","四","五","六"};stamp=String(out)+" 周"+weeks[t.tm_wday]+" 北京时间";}
    font_.setFont(u8g2_font_wqy12_t_gb2312);font_.setCursor(384-font_.getUTF8Width(stamp.c_str()),27);font_.print(stamp);
  }
  void renderHeaderOnly(){
    epd_.setRotation(0);epd_.setPartialWindow(0,0,400,35);epd_.firstPage();
    do{epd_.fillRect(0,0,400,35,GxEPD_WHITE);weatherHeader();}while(epd_.nextPage());
  }
  void render(const JsonDocument& data){
    int64_t fetched=data["fetchedAt"]|int64_t(0),now=time(nullptr);
    bool clock=now>=weather_schedule::kValidClock,cached=fetched>=weather_schedule::kValidClock;
    time_t base=(clock?now:(cached?fetched:weather_schedule::kValidClock))+28800;tm local{};gmtime_r(&base,&local);
    char date[11];strftime(date,sizeof(date),"%Y-%m-%d",&local);
    epd_.setRotation(0);epd_.setFullWindow();epd_.firstPage();
    do{
      epd_.fillScreen(GxEPD_WHITE);weatherHeader();
      if(cached){
        String temperature=String(int(round(data["current"]["temperature"].as<float>())));
        font_.setFont(u8g2_font_logisoso42_tn);int unitX=16+font_.getUTF8Width(temperature.c_str())+7;font_.setCursor(16,91);font_.print(temperature);
        font_.setFont(u8g2_font_helvB14_tf);font_.setCursor(unitX+5,70);font_.print("C");epd_.drawCircle(unitX,57,2,GxEPD_BLACK);
        int code=data["current"]["code"]|-1;icon(213,41,code,2);center(202,94,60,description(code),u8g2_font_wqy12_t_gb2312);
        JsonObjectConst today;for(JsonObjectConst d:data["daily"].as<JsonArrayConst>())if(d["date"].as<String>()==date)today=d;
        label(272,58,"今日高低温");
        String range=today.isNull()?"--":String(int(round(today["min"].as<float>())))+"~"+int(round(today["max"].as<float>()))+"°C";
        center(268,84,116,range,u8g2_font_helvB14_tf);
      }else{label(16,65,"等待获取天气");label(16,86,clock?"请检查热点和网络":"请用管理网页校准时间");}
      epd_.drawLine(16,102,384,102,GxEPD_BLACK);label(16,116,"未来8小时");label(240,116,"每小时 / 降雨概率");
      for(int i=0;i<8;i++){
        int x=16+i*46;if(i)epd_.drawLine(x,122,x,191,GxEPD_BLACK);
        int64_t at=clock?weather_schedule::nextSlot(now)+i*3600:0;char timeLabel[6]="--:--";
        if(at){time_t bj=at+28800;tm t{};gmtime_r(&bj,&t);strftime(timeLabel,sizeof(timeLabel),"%H:%M",&t);}
        center(x,132,46,timeLabel);JsonObjectConst h=at?weather_data::hourAt(data["hourly"].as<JsonArrayConst>(),at):JsonObjectConst();
        if(!h.isNull()){
          icon(x+11,137,h["code"]|-1);center(x,175,46,String(int(round(h["temperature"].as<float>())))+"°",u8g2_font_helvB12_tf);
          center(x,189,46,h["rain"].isNull()?"--%":String(int(round(h["rain"].as<float>())))+"%");
        }else{center(x,160,46,"--");center(x,179,46,"暂无",u8g2_font_wqy12_t_gb2312);}
      }
      epd_.drawLine(16,195,384,195,GxEPD_BLACK);label(16,209,"未来三天");label(223,209,"高低温 / 降雨概率");
      for(int i=0;i<3;i++){
        int x=16+i*123;if(i)epd_.drawLine(x-2,216,x-2,280,GxEPD_BLACK);
        time_t at=base+(i+1)*86400;tm t{};gmtime_r(&at,&t);char wanted[11];strftime(wanted,sizeof(wanted),"%Y-%m-%d",&t);
        const char* names[]={"明天","后天","大后天"};label(x+5,226,String(clock?names[i]:"")+" "+String(wanted).substring(5));
        JsonObjectConst day;for(JsonObjectConst d:data["daily"].as<JsonArrayConst>())if(d["date"].as<String>()==wanted)day=d;
        if(!day.isNull()){
          icon(x+20,232,day["code"]|-1);label(x+54,248,description(day["code"]|-1));
          center(x,266,120,String(int(round(day["min"].as<float>())))+"~"+int(round(day["max"].as<float>()))+"°C",u8g2_font_helvB12_tf);
          center(x,279,120,day["rain"].isNull()?"降雨未知":String("降雨 ")+int(round(day["rain"].as<float>()))+"%",u8g2_font_wqy12_t_gb2312);
        }else label(x+24,259,"暂无预报");
      }
      epd_.drawLine(16,284,384,284,GxEPD_BLACK);
      String stamp="--";if(cached){time_t bj=fetched+28800;tm t{};gmtime_r(&bj,&t);char out[20];strftime(out,sizeof(out),"%m/%d %H:%M",&t);stamp=out;}
      text(16,297,String("Open-Meteo ")+stamp,true);
      label(231,297,!clock?"待校时":!(data["online"]|false)?"离线缓存":(data["stale"]|true)?"缓存":"已更新");text(353,297,"04/08",true);
    }while(epd_.nextPage());
  }
};
