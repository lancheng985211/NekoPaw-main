#pragma once
#include <Arduino.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <GxEPD2_BW.h>
#include <time.h>

namespace nekopaw_screen {
constexpr int64_t kValidClockMin = 1704067200LL;
constexpr int64_t kValidClockMax = 4102444799LL;

inline String minuteKey() {
  const int64_t now = time(nullptr);
  return now >= kValidClockMin && now <= kValidClockMax ? String(now / 60) : String("clock-invalid");
}

template <class DisplayT>
void drawHeader(DisplayT& epd, U8G2_FOR_ADAFRUIT_GFX& font, bool home = false, bool wifiConnected = false) {
  font.setForegroundColor(GxEPD_BLACK);
  font.setBackgroundColor(GxEPD_WHITE);
  epd.drawCircle(19, 16, 7, GxEPD_BLACK);
  epd.fillCircle(15, 7, 2, GxEPD_BLACK);
  epd.fillCircle(22, 6, 2, GxEPD_BLACK);
  font.setFont(home ? u8g2_font_helvB10_tf : u8g2_font_helvB12_tf);
  font.setCursor(34, 22);
  font.print("DE BRUYNE 17");

  const int64_t now = time(nullptr);
  String clock = "--:--", date = "待校时";
  if (now >= kValidClockMin && now <= kValidClockMax) {
    time_t local = static_cast<time_t>(now + 28800);
    tm beijing{};
    if (gmtime_r(&local, &beijing)) {
      char t[6], d[20];
      strftime(t, sizeof(t), "%H:%M", &beijing);
      const char* weekdays[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};
      snprintf(d, sizeof(d), "%02d/%02d %s", beijing.tm_mon + 1, beijing.tm_mday, weekdays[beijing.tm_wday]);
      clock = t; date = d;
    }
  }

  font.setFont(u8g2_font_helvB14_tf);
  font.setCursor(home ? 160 : 286, 17);
  font.print(clock);
  font.setFont(u8g2_font_wqy12_t_gb2312);
  font.setCursor(home ? 217 : 338, 16);
  font.print("北京时间");
  font.setFont(u8g2_font_wqy12_t_gb2312);
  font.setCursor(home ? 160 : 286, 31);
  font.print(date);

  if (home) {
    epd.drawRoundRect(300, 6, 88, 26, 12, GxEPD_BLACK);
    epd.drawLine(304, 13, 310, 10, GxEPD_BLACK);
    epd.drawLine(310, 10, 316, 13, GxEPD_BLACK);
    epd.drawLine(306, 17, 310, 15, GxEPD_BLACK);
    epd.drawLine(310, 15, 314, 17, GxEPD_BLACK);
    epd.fillCircle(310, 21, 1, GxEPD_BLACK);
    if (!wifiConnected) epd.drawLine(303, 9, 317, 24, GxEPD_BLACK);
    font.setFont(u8g2_font_wqy12_t_gb2312);
    font.setCursor(320, 22);
    font.print(wifiConnected ? "WiFi已连接" : "WiFi未连接");
  }
  epd.drawLine(12, 38, 388, 38, GxEPD_BLACK);
}
}  // namespace nekopaw_screen
