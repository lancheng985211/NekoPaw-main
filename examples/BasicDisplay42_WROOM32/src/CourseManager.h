#pragma once

#include <ArduinoJson.h>
#include <Preferences.h>
#include <WebServer.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <time.h>
#include <sys/time.h>
#include "CoursePage.h"
#include "CourseSchedule.h"
#include "WeatherManager.h"
#include "WeatherPage.h"
#include "CalendarManager.h"
#include "CalendarPage.h"
#include "CodexUsageManager.h"
#include "CodexUsagePage.h"
#include "DeliveryManager.h"
#include "DeliveryPage.h"
#include "PetManager.h"
#include "PetPage.h"
#include "MemoManager.h"
#include "MemoPage.h"
#include "ScreenPages.h"
#include "ScreenNavigation.h"
#include "NekoPawHomePage.h"
#include "HomePage.h"

// A separate server keeps the existing NekoPaw bridge on port 80 available.
template <class DisplayT> class CourseManager {
public:
  explicit CourseManager(DisplayT& epd, WeatherManager<DisplayT>& weather, CalendarManager<DisplayT>& calendar, CodexUsageManager<DisplayT>& codex, DeliveryManager<DisplayT>& delivery, PetManager<DisplayT>& pet, MemoManager<DisplayT>& memo) : epd_(epd), server_(81), weather_(weather), calendar_(calendar), codex_(codex), delivery_(delivery), pet_(pet), memo_(memo), home_(epd) {}
  void cyclePage(int direction) { choosePage(screen_pages::cycle(activePage_, direction)); }
  const char* currentPageName() const { return screen_pages::name(activePage_); }
  void choosePage(uint8_t value) {
    if(value>=screen_pages::count)return;
    if(storageReady_&&activePage_!=value)prefs_.putUChar("pageIndexV4",value);
    activePage_=value;updateVisibility();
    if(value==screen_pages::Course){lastSignature_="";pending_=true;changedAt_=millis();}
  }

  void begin() {
    storageReady_ = prefs_.begin("nekocourses", false);
    String saved = storageReady_ && prefs_.isKey("schedule") ? prefs_.getString("schedule", "") : "";
    if (saved.length()) {
      String reason;
      if (deserializeJson(document_, saved) || !course_schedule::migrate(document_, reason)) {
        restoreFailed_ = true;
        course_schedule::blank(document_);
      } else revision_ = document_["revision"] | 0U;
    } else course_schedule::blank(document_);
    if(!storageReady_) activePage_=screen_pages::Home;
    else if(prefs_.isKey("pageIndexV4")) activePage_=prefs_.getUChar("pageIndexV4",screen_pages::Home);
    else {
      activePage_=prefs_.isKey("pageIndexV3")
        ? screen_pages::fromSevenPageIndex(prefs_.getUChar("pageIndexV3",255))
        : static_cast<uint8_t>(screen_pages::Home);
      prefs_.putUChar("pageIndexV4",activePage_);
    }
    if(activePage_>=screen_pages::count){activePage_=screen_pages::Home;if(storageReady_)prefs_.putUChar("pageIndexV4",activePage_);}
    home_.begin();
    updateVisibility();
    font_.begin(epd_);
    font_.setFont(u8g2_font_wqy16_t_gb2312);
    font_.setFontMode(1);
    font_.setForegroundColor(GxEPD_BLACK);
    font_.setBackgroundColor(GxEPD_WHITE);
    const char* headers[] = {"Origin", "Content-Type"};
    server_.collectHeaders(headers, 2);
    server_.on("/", HTTP_GET, [this]() { server_.sendHeader("Cache-Control", "no-store"); server_.send_P(200, "text/html; charset=utf-8", kCoursePage); });
    server_.on("/home", HTTP_GET, [this]() { server_.sendHeader("Cache-Control", "no-store"); server_.send_P(200, "text/html; charset=utf-8", kHomePage); });
    server_.on("/api/schedule", HTTP_GET, [this]() { sendState(); });
    server_.on("/api/schedule", HTTP_POST, [this]() { save(); });
    server_.on("/api/clock", HTTP_POST, [this]() {
      if (!checkWrite()) return;
      JsonDocument body;
      if (deserializeJson(body, server_.arg("plain")) || !body["epoch"].is<int64_t>()) {
        error(400, "时间格式无效"); return;
      }
      int64_t seconds = body["epoch"].as<int64_t>();
      if (seconds < 1704067200LL || seconds > 4102444799LL) { error(400, "时间超出范围"); return; }
      timeval now{}; now.tv_sec = seconds;
      settimeofday(&now, nullptr);
      server_.send(200, "application/json", "{\"ok\":true}");
    });
    server_.on("/weather", HTTP_GET, [this]() { server_.send_P(200,"text/html; charset=utf-8",kWeatherPage); });
    server_.on("/api/weather", HTTP_GET, [this]() { sendSnapshot(weather_, false); });
    server_.on("/calendar", HTTP_GET, [this]() { server_.sendHeader("Cache-Control","no-store");server_.send_P(200,"text/html; charset=utf-8",kCalendarPage); });
    server_.on("/api/calendar", HTTP_GET, [this]() { sendSnapshot(calendar_, true); });
    server_.on("/codex", HTTP_GET, [this]() { server_.sendHeader("Cache-Control","no-store");server_.send_P(200,"text/html; charset=utf-8",kCodexUsagePage); });
    server_.on("/api/codex", HTTP_GET, [this]() { sendSnapshot(codex_, true); });
    server_.on("/api/codex", HTTP_POST, [this]() { if(!checkWrite())return;JsonDocument doc;if(deserializeJson(doc,server_.arg("plain"))){error(400,"用量数据格式无效");return;}String reason;int status;if(!codex_.accept(doc,reason,status)){error(status,reason.c_str());return;}server_.send(200,"application/json","{\"ok\":true}"); });
    server_.on("/delivery", HTTP_GET, [this]() { server_.sendHeader("Cache-Control","no-store");server_.send_P(200,"text/html; charset=utf-8",kDeliveryPage); });
    server_.on("/api/delivery", HTTP_GET, [this]() { sendSnapshot(delivery_, true); });
    server_.on("/api/delivery", HTTP_POST, [this]() { if(!checkWrite())return;JsonDocument doc;if(deserializeJson(doc,server_.arg("plain"))){error(400,"通知格式无效");return;}String reason;int status;if(!delivery_.accept(doc,reason,status)){error(status,reason.c_str());return;}server_.send(200,"application/json","{\"ok\":true}"); });
    server_.on("/pet", HTTP_GET, [this]() { server_.sendHeader("Cache-Control","no-store");server_.send_P(200,"text/html; charset=utf-8",kPetPage); });
    server_.on("/static/screen-navigation.js", HTTP_GET, [this]() { server_.sendHeader("Cache-Control","no-store");server_.send_P(200,"application/javascript; charset=utf-8",kScreenNavigationJs); });
    server_.on("/memo", HTTP_GET,[this](){server_.sendHeader("Cache-Control","no-store");server_.send_P(200,"text/html; charset=utf-8",kMemoPage);});
    server_.on("/api/memo", HTTP_GET, [this]() { sendSnapshot(memo_, true); });
    server_.on("/api/memo", HTTP_POST,[this](){if(!checkWrite())return;JsonDocument doc;if(deserializeJson(doc,server_.arg("plain"))){error(400,"备忘录格式无效");return;}String reason;int status=400;if(!memo_.accept(doc,reason,status)){error(status,reason.c_str());return;}server_.send(200,"application/json","{\"ok\":true}");});
    server_.on("/api/page", HTTP_GET, [this]() { sendPageState(); });
    server_.on("/api/pet", HTTP_GET, [this]() { sendSnapshot(pet_, true); });
    server_.on("/api/pet", HTTP_POST, [this]() { if(!checkWrite())return;JsonDocument doc;if(deserializeJson(doc,server_.arg("plain"))){error(400,"宠物指令格式无效");return;}String reason;int status=400;if(!pet_.command(doc,reason,status)){error(status,reason.c_str());return;}server_.send(200,"application/json","{\"ok\":true}"); });
    server_.on("/api/page", HTTP_POST, [this]() {
      if(!checkWrite())return;
      JsonDocument doc;uint8_t index=0;
      if(deserializeJson(doc,server_.arg("plain")) || !doc["page"].is<const char*>()) {error(400,"页面名称无效");return;}
      const JsonString requested=doc["page"].as<JsonString>();
      if(!requested.c_str() || strlen(requested.c_str())!=requested.size() || !screen_pages::indexForName(requested.c_str(),index)){error(400,"页面名称无效");return;}
      choosePage(index);server_.send(200,"application/json","{\"ok\":true}");
    });
    server_.begin();
    started_ = true;
    Serial.println("Screen pages (8 pages, home-carousel v1): 1 home -> 2 pet -> 3 course -> 4 weather -> 5 calendar -> 6 codex -> 7 delivery -> 8 memo");
    Serial.print("Memo manager: http://");Serial.print(WiFi.localIP());Serial.println(":81/memo");
    configTime(8 * 3600, 0, "ntp.aliyun.com", "pool.ntp.org");
    Serial.print("Course editor: http://"); Serial.print(WiFi.localIP()); Serial.println(":81/");
    Serial.print("Calendar page: http://");Serial.print(WiFi.localIP());Serial.println(":81/calendar");
    Serial.print("Codex usage page: http://");Serial.print(WiFi.localIP());Serial.println(":81/codex");
    Serial.print("Pet manager: http://");Serial.print(WiFi.localIP());Serial.println(":81/pet");
    Serial.print("Delivery page: http://");Serial.print(WiFi.localIP());Serial.println(":81/delivery");
  }

  void loop() {
    if (!started_) return;
    server_.handleClient();
    home_.loop();
    if(activePage_!=screen_pages::Course)return;
    time_t stamp = time(nullptr);
    if (stamp < 1704067200) return; // Phone or NTP must supply a valid date after a cold boot.
    tm local{}; localtime_r(&stamp, &local);
    char date[11]; strftime(date, sizeof(date), "%Y-%m-%d", &local);
    if (lastDate_ != date) { lastDate_ = date; pending_ = true; changedAt_ = millis(); }
    if (pending_ && millis() - changedAt_ >= 800) {
      pending_ = false;
      const int weekday = (local.tm_wday + 6) % 7;
      JsonDocument today;
      course_schedule::resolveRows(document_, date, weekday, today);
      JsonArrayConst rows = today.as<JsonArrayConst>();
      String signature; serializeJson(rows, signature);
      signature = String(date) + signature;
      signature += String(course_schedule::teachingWeek(document_, date));
      signature += String(":") + (document_["termWeeks"] | 20);
      if (signature != lastSignature_) { render(date, weekday, rows); lastSignature_ = signature; lastHeaderMinute_ = nekopaw_screen::minuteKey(); }
    }
    const String minute=nekopaw_screen::minuteKey();
    if(minute!=lastHeaderMinute_){if(lastSignature_.length())renderHeaderOnly();lastHeaderMinute_=minute;}
  }

private:
  DisplayT& epd_;
  WebServer server_;
  WeatherManager<DisplayT>& weather_;
  CalendarManager<DisplayT>& calendar_;
  CodexUsageManager<DisplayT>& codex_;
  DeliveryManager<DisplayT>& delivery_;
  PetManager<DisplayT>& pet_;
  MemoManager<DisplayT>& memo_;
  NekoPawHomePage<DisplayT> home_;
  uint8_t activePage_ = 0;
  Preferences prefs_;
  U8G2_FOR_ADAFRUIT_GFX font_;
  JsonDocument document_;
  uint32_t revision_ = 0, changedAt_ = 0;
  bool storageReady_ = false, pending_ = true, started_ = false, restoreFailed_ = false;
  String lastDate_, lastSignature_, lastHeaderMinute_;
  static constexpr size_t kMaxStorage = 6000;

  void updateVisibility() {
    home_.setVisible(activePage_==screen_pages::Home);
    pet_.setVisible(activePage_==screen_pages::Pet);
    weather_.setVisible(activePage_==screen_pages::Weather);
    calendar_.setVisible(activePage_==screen_pages::Calendar);
    codex_.setVisible(activePage_==screen_pages::Codex);
    delivery_.setVisible(activePage_==screen_pages::Delivery);
    memo_.setVisible(activePage_==screen_pages::Memo);
  }

  // Keep each endpoint's existing JSON fields while sharing serialization.
  template<class ManagerT>
  void sendSnapshot(ManagerT& manager, bool includePage) {
    JsonDocument doc;
    manager.snapshot(doc);
    if (includePage) doc["page"] = currentPageName();
    String json;
    serializeJson(doc, json);
    server_.sendHeader("Cache-Control", "no-store");
    server_.send(200, "application/json; charset=utf-8", json);
  }

  void sendPageState() {
    JsonDocument doc;doc["service"]="nekopaw-pages-v2";
    doc["page"]=currentPageName();doc["pageNumber"]=activePage_+1;doc["pageCount"]=screen_pages::count;
    JsonArray pages=doc["pages"].to<JsonArray>();
    for(uint8_t i=0;i<screen_pages::count;++i){JsonObject item=pages.add<JsonObject>();item["name"]=screen_pages::name(i);item["label"]=screen_pages::label(i);}
    String json;serializeJson(doc,json);server_.sendHeader("Cache-Control","no-store");server_.send(200,"application/json; charset=utf-8",json);
  }

  void error(int status, const char* message) {
    JsonDocument body; body["error"] = message;
    String json; serializeJson(body, json);
    server_.send(status, "application/json; charset=utf-8", json);
  }
  bool checkWrite() {
    String origin = server_.header("Origin");
    if (origin.length() && origin != String("http://") + server_.hostHeader()) {
      error(403, "请在设备自己的管理网页操作"); return false;
    }
    if (!server_.header("Content-Type").startsWith("application/json")) {
      error(415, "请求需要 JSON 格式"); return false;
    }
    if (server_.arg("plain").length() > kMaxStorage + 512) { error(413, "课表内容过多，请减少临时调整"); return false; }
    return true;
  }
  void sendState() {
    JsonDocument body; body["revision"] = revision_; body["data"].set(document_);
    if (restoreFailed_) body["warning"] = "原课表读取失败，已禁止覆盖，请保留设备数据并联系维护";
    String json; serializeJson(body, json);
    server_.sendHeader("Cache-Control", "no-store");
    server_.send(200, "application/json; charset=utf-8", json);
  }
  void save() {
    if (!checkWrite()) return;
    if (restoreFailed_) { error(507, "原课表读取失败，已禁止覆盖，请先恢复数据"); return; }
    JsonDocument body;
    if (deserializeJson(body, server_.arg("plain")) || !body["data"].is<JsonObject>() || !body["revision"].is<uint32_t>()) {
      error(400, "课表格式无效"); return;
    }
    if (body["revision"].as<uint32_t>() != revision_) { error(409, "其他页面已修改课表，请刷新后重试"); return; }
    JsonDocument candidate; candidate.set(body["data"]);
    String reason;
    if (!course_schedule::validate(candidate, reason)) { error(400, reason.c_str()); return; }
    if (course_schedule::conflicts(candidate, reason)) { error(400, reason.c_str()); return; }
    candidate["revision"] = revision_ + 1;
    String serialized; serializeJson(candidate, serialized);
    if (serialized.length() > kMaxStorage) { error(413, "课表超过存储上限，请删除过期的临时调整"); return; }
    if (!storageReady_ || prefs_.putString("schedule", serialized) != serialized.length()) {
      error(507, "设备存储失败，原课表仍保留，请重试"); return;
    }
    document_ = candidate; ++revision_; pending_ = true; changedAt_ = millis();
    sendState();
  }
  void text(int x, int baseline, String value, int maxWidth) {
    while (value.length() && font_.getUTF8Width(value.c_str()) > maxWidth) {
      int end = value.length() - 1;
      while (end > 0 && (static_cast<uint8_t>(value[end]) & 0xc0) == 0x80) --end;
      value.remove(end);
    }
    font_.setCursor(x, baseline); font_.print(value);
  }
  void renderHeaderOnly() {
    epd_.setRotation(0);epd_.setPartialWindow(0,0,400,40);epd_.firstPage();
    do{epd_.fillRect(0,0,400,40,GxEPD_WHITE);nekopaw_screen::drawHeader(epd_,font_);}while(epd_.nextPage());
  }
  void render(const char* date, int weekday, JsonArrayConst rows) {
    const char* weekdays[] = {"周一", "周二", "周三", "周四", "周五", "周六", "周日"};
    epd_.setRotation(0); epd_.setFullWindow(); epd_.firstPage();
    do {
      epd_.fillScreen(GxEPD_WHITE);
      nekopaw_screen::drawHeader(epd_,font_);
      const String termStart = document_["termStart"] | "";
      int week = course_schedule::teachingWeek(document_, date), total = document_["termWeeks"] | 20;
      String label = !termStart.length() ? "课表" : (week >= 1 && week <= total ? String("第") + week + "周" : "非教学周");
      text(10, 57, String(date) + "  " + weekdays[weekday] + " " + label, 380);
      bool any = false;
      for (int i = 0; i < 5; ++i) {
        int top = 68 + i * 42;
        epd_.drawLine(8, top, 392, top, GxEPD_BLACK);
        text(12, top + 25, String(i + 1), 25);
        JsonVariantConst lesson = rows[i];
        if (lesson.isNull()) { text(106, top + 25, "无课", 278); continue; }
        any = true;
        text(42, top + 16, lesson["start"].as<String>(), 60);
        text(42, top + 33, lesson["end"].as<String>(), 60);
        text(106, top + 16, lesson["name"].as<String>(), 278);
        text(106, top + 33, lesson["room"].as<String>(), 278);
      }
      text(10, 294, any ? "手机编辑：设备 IP 的 :81 端口" : "今日无课 · 手机可添加课程", 318);
      text(348, 294, "03/08", 44);
    } while (epd_.nextPage());
  }
};

