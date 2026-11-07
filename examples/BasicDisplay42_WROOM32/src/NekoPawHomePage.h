#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <U8g2_for_Adafruit_GFX.h>
#include "NekoPawScreenHeader.h"
#include "NekoPawHomeArt.h"

template <class DisplayT> class NekoPawHomePage {
public:
  explicit NekoPawHomePage(DisplayT& epd) : epd_(epd) {}
  void begin() {
    font_.begin(epd_);
    font_.setFontMode(1);
    font_.setForegroundColor(GxEPD_BLACK);
    font_.setBackgroundColor(GxEPD_WHITE);
    started_ = true;
  }
  void setVisible(bool value) {
    const bool entering = value && !visible_;
    visible_ = value;
    if (entering) {
      lastSlideAt_ = millis();
      lastMinute_ = nekopaw_screen::minuteKey();
      lastWifi_ = WiFi.status() == WL_CONNECTED;
      render();
    }
  }
  void loop() {
    if (!started_ || !visible_ || millis() - lastCheck_ < 500) return;
    lastCheck_ = millis();
    bool slideChanged = false, headerChanged = false;
    const uint32_t now = millis();
    if (now - lastSlideAt_ >= 60000UL) {
      frame_ = (frame_ + 1) % 6;
      lastSlideAt_ = now;
      slideChanged = true;
    }
    const String minute = nekopaw_screen::minuteKey();
    if (minute != lastMinute_) { lastMinute_ = minute; headerChanged = true; }
    const bool connected = WiFi.status() == WL_CONNECTED;
    if (connected != lastWifi_) { lastWifi_ = connected; headerChanged = true; }
    if (slideChanged) render();
    else if (headerChanged) renderHeaderOnly();
  }
private:
  DisplayT& epd_;
  U8G2_FOR_ADAFRUIT_GFX font_;
  bool started_ = false, visible_ = false, lastWifi_ = false;
  uint8_t frame_ = 0;
  uint32_t lastSlideAt_ = 0, lastCheck_ = 0;
  String lastMinute_;
  void text(int x, int baseline, const String& value, const uint8_t* face) {
    font_.setFont(face);
    font_.setCursor(x, baseline);
    font_.print(value);
  }
  void renderHeaderOnly() {
    epd_.setRotation(0);
    epd_.setPartialWindow(0, 0, 400, 40);
    epd_.firstPage();
    do {
      epd_.fillRect(0, 0, 400, 40, GxEPD_WHITE);
      nekopaw_screen::drawHeader(epd_, font_, true, WiFi.status() == WL_CONNECTED);
    } while (epd_.nextPage());
  }
  void render() {
    const char* captions[] = {"涂上颜色", "软乎乎的小伙伴", "勇敢去做", "抱抱时刻", "一起加油", "雨天也同行"};
    const bool connected = WiFi.status() == WL_CONNECTED;
    epd_.setRotation(0);
    epd_.setFullWindow();
    epd_.firstPage();
    do {
      epd_.fillScreen(GxEPD_WHITE);
      nekopaw_screen::drawHeader(epd_, font_, true, connected);
      epd_.drawRoundRect(12, 43, 376, 200, 9, GxEPD_BLACK);
      epd_.drawBitmap(26, 49, kNekoPawHomeArt[frame_], 348, 153, GxEPD_BLACK);
      epd_.drawLine(13, 211, 387, 211, GxEPD_BLACK);
      text(23, 228, captions[frame_], u8g2_font_wqy12_t_gb2312);
      text(244, 228, "每60秒切换", u8g2_font_wqy12_t_gb2312);
      for (int i = 0; i < 6; ++i) {
        const int x = 354 + i * 6;
        if (i == frame_) epd_.fillCircle(x, 226, 2, GxEPD_BLACK);
        else epd_.drawCircle(x, 226, 2, GxEPD_BLACK);
      }
      const char* pages[] = {"宠物", "课表", "天气", "日历", "Codex", "外卖", "备忘录"};
      for (int i = 0; i < 7; ++i) {
        const int x = 12 + i * 54;
        epd_.drawRoundRect(x, 251, 52, 29, 6, GxEPD_BLACK);
        font_.setFont(u8g2_font_wqy12_t_gb2312);
        const int width = font_.getUTF8Width(pages[i]);
        font_.setCursor(x + (52 - width) / 2, 270);
        font_.print(pages[i]);
      }
      text(16, 294, "离线仍可查看已保存内容", u8g2_font_wqy12_t_gb2312);
      text(274, 294, String("插画") + String(frame_ + 1) + "/6", u8g2_font_wqy12_t_gb2312);
      text(352, 294, "01/08", u8g2_font_6x10_tf);
    } while (epd_.nextPage());
  }
};
