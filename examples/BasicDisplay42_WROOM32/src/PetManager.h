#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include "NekoPawScreenHeader.h"
#include "PetAnimation.h"
#include "PetArt.h"
#include "PetRefreshPolicy.h"

// The shared display is only touched from the Arduino main loop. NVS stores
// settings, not animation frames or per-frame counters.
template <class DisplayT> class PetManager {
public:
  explicit PetManager(DisplayT& epd) : epd_(epd) {}

  void begin() {
    if (started_) return;
    nvsOpen_ = prefs_.begin("nekopet", false);
    storageReady_ = nvsOpen_;
    if (!storageReady_) warning_ = "宠物设置暂不能保存，自动活动仍可使用";
    if (storageReady_ && prefs_.isKey("name")) {
      char stored[26] = {};
      const size_t count = prefs_.getString("name", stored, sizeof(stored));
      String candidate(stored);
      if (!count || count != candidate.length() + 1U || !validName(candidate)) {
        storageReady_ = false;
        warning_ = "原宠物名字读取失败，已保留原存储并禁止覆盖";
      } else name_ = candidate;
    }
    // A known key must be a valid bool; 255 distinguishes read failure from false.
    if (nvsOpen_ && prefs_.isKey("paused")) {
      const uint8_t value = prefs_.getUChar("paused", 255);
      if (value > 1) {
        storageReady_ = false;
        warning_ = "原暂停设置读取失败，已保留原存储并禁止覆盖";
      } else pausedSetting_ = value != 0;
    }
    const uint32_t now = millis();
    engine_.begin(now, micros() ^ static_cast<uint32_t>(time(nullptr)) ^ 0x4E504554UL);
    engine_.setVisible(visible_, now);
    engine_.setPaused(pausedSetting_, now);
    lastMode_ = engine_.mode();
    font_.begin(epd_);
    font_.setFontMode(1);
    font_.setForegroundColor(GxEPD_BLACK);
    font_.setBackgroundColor(GxEPD_WHITE);
    dirty_ = true;
    fullRequested_ = true;
    started_ = true;
    Serial.println("Pet service started (Samoyed, reduced-flash v2; target 300ms, cleanup at rest after 30s, 60s fallback)");
  }

  void setVisible(bool value) {
    if (visible_ == value) return;
    visible_ = value;
    if (started_) {
      engine_.setVisible(value, millis());
      if (!value) epd_.powerOff();
    }
    if (value) { fullRequested_ = true; dirty_ = true; }
  }

  void loop() {
    if (!started_ || !visible_) return;
    const uint32_t now = millis();
    const bool advanced = engine_.tick(now);
    noteMode();
    const String minute = nekopaw_screen::minuteKey();
    const bool headerChanged = minute != headerMinute_;
    const bool resting = engine_.mode() == pet_animation::Mode::Rest ||
                         engine_.mode() == pet_animation::Mode::Idle || engine_.paused();
    // Rest, pause and clock changes are ordinary partial updates. Only the
    // timed cleanup policy or entering this page may request a full waveform.
    const bool full = refreshPolicy_.needsFull(now, resting, fullRequested_);
    if (!advanced && !dirty_ && !full && !headerChanged) return;
    render(full, full || headerChanged, minute);
    dirty_ = false;
    fullRequested_ = false;
  }

  void snapshot(JsonDocument& out) {
    out.clear();
    out["schema"] = 1;
    out["service"] = "nekopaw-pet-v1";
    out["name"] = name_;
    out["visible"] = visible_;
    out["paused"] = engine_.paused();
    out["autonomous"] = true;
    out["action"] = actionName(engine_.mode());
    out["actionText"] = actionText(engine_.mode());
    out["frame"] = engine_.frame();
    out["pose"] = engine_.pose();
    out["targetIntervalMs"] = 300;
    out["lastRefreshMs"] = lastRefreshMs_;
    out["partialRefreshes"] = partialRefreshes_;
    out["fullRefreshes"] = fullRefreshes_;
    out["partialSinceFull"] = refreshPolicy_.partialSinceFull();
    out["refreshPolicy"] = "rest-cleanup-v2";
    out["cleanupIntervalMs"] = uint32_t(pet_refresh::Policy::cleanupIntervalMs);
    out["maxCleanupDelayMs"] = uint32_t(pet_refresh::Policy::maxCleanupDelayMs);
    out["storageReady"] = storageReady_;
    out["warning"] = warning_;
    out["patCount"] = patCount_;
    out["foodCount"] = foodCount_;
    out["counterScope"] = "session";
    out["hasAudio"] = false;
    const int64_t now = time(nullptr);
    out["clockValid"] = now >= nekopaw_screen::kValidClockMin && now <= nekopaw_screen::kValidClockMax;
  }

  bool command(const JsonDocument& input, String& reason, int& status) {
    status = 400;
    if (!started_) { reason = "宠物服务尚未启动"; status = 503; return false; }
    if (!input.is<JsonObject>() || !input["command"].is<const char*>()) {
      reason = "宠物操作格式无效"; return false;
    }
    const JsonString rawCommand = input["command"].as<JsonString>();
    const char* commandName = rawCommand.c_str();
    if (!commandName || strlen(commandName) != rawCommand.size()) {
      reason = "宠物操作名称无效"; return false;
    }
    const String cmd(commandName);
    if (cmd == "rename") {
      if (!input["name"].is<const char*>()) { reason = "请填写宠物名字"; return false; }
      const JsonString rawName = input["name"].as<JsonString>();
      if (!rawName.c_str() || strlen(rawName.c_str()) != rawName.size()) {
        reason = "名字不能包含控制字符"; return false;
      }
      String next(rawName.c_str());
      if (!validName(next)) { reason = "名字须为1至8个字、最多24字节，且不含控制字符"; return false; }
      next.trim();
      if (!next.length()) { reason = "名字不能为空"; return false; }
      if (!storageReady_) return storageError(reason, status);
      if (next != name_ && prefs_.putString("name", next) != next.length()) {
        reason = "名字保存失败，原名字仍保留"; status = 507; return false;
      }
      name_ = next; dirty_ = true; status = 200; return true;
    }
    if (cmd == "pause" || cmd == "resume") {
      const bool paused = cmd == "pause";
      if (!savePaused(paused, reason, status)) return false;
      engine_.setPaused(paused, millis());
      dirty_ = true; status = 200; return true;
    }
    pet_animation::Mode mode = pet_animation::Mode::Idle;
    const bool patted = cmd == "pat";
    if (patted || cmd == "jump") mode = pet_animation::Mode::Jump;
    else if (cmd == "roll") mode = pet_animation::Mode::Roll;
    else if (cmd == "bark") mode = pet_animation::Mode::Bark;
    else if (cmd == "eat") mode = pet_animation::Mode::Eat;
    else { reason = "未知的宠物操作"; return false; }
    if (!visible_) { reason = "请先让屏幕显示宠物页"; status = 409; return false; }
    // A direct interaction resumes activity, including the saved pause setting.
    if (engine_.paused() && !savePaused(false, reason, status)) return false;
    engine_.setPaused(false, millis());
    if (patted) {
      engine_.pat(millis());
      increment(patCount_);
      hearts_ = true;
    } else {
      if (!engine_.perform(mode, millis())) { reason = "暂时不能执行这个动作"; return false; }
      hearts_ = false;
      if (mode == pet_animation::Mode::Eat) increment(foodCount_);
    }
    lastMode_ = engine_.mode();
    dirty_ = true; status = 200; return true;
  }

private:
  DisplayT& epd_;
  Preferences prefs_;
  pet_animation::Engine engine_;
  pet_refresh::Policy refreshPolicy_;
  U8G2_FOR_ADAFRUIT_GFX font_;
  String name_ = "雪球", warning_, headerMinute_;
  pet_animation::Mode lastMode_ = pet_animation::Mode::Idle;
  bool started_ = false, visible_ = false, nvsOpen_ = false, storageReady_ = false;
  bool pausedSetting_ = false, dirty_ = true, fullRequested_ = true, hearts_ = false;
  uint32_t lastRefreshMs_ = 0, partialRefreshes_ = 0, fullRefreshes_ = 0;
  uint32_t patCount_ = 0, foodCount_ = 0;

  static void increment(uint32_t& value) { if (value != UINT32_MAX) ++value; }
  bool storageError(String& reason, int& status) {
    reason = "宠物设置暂不能保存，已保留原数据"; status = 507; return false;
  }
  bool savePaused(bool paused, String& reason, int& status) {
    if (!storageReady_) return storageError(reason, status);
    if (paused != pausedSetting_ && prefs_.putBool("paused", paused) != 1) {
      reason = "暂停设置保存失败，原设置仍保留"; status = 507; return false;
    }
    pausedSetting_ = paused;
    return true;
  }
  static bool validName(const String& value) {
    if (!value.length() || value.length() > 24) return false;
    uint8_t characters = 0;
    bool nonspace = false;
    const uint8_t* data = reinterpret_cast<const uint8_t*>(value.c_str());
    size_t offset = 0;
    while (offset < value.length()) {
      uint32_t cp; uint8_t following;
      const uint8_t first = data[offset++];
      if (first <= 0x7F) { cp = first; following = 0; }
      else if (first >= 0xC2 && first <= 0xDF) { cp = first & 0x1F; following = 1; }
      else if (first >= 0xE0 && first <= 0xEF) { cp = first & 0x0F; following = 2; }
      else if (first >= 0xF0 && first <= 0xF4) { cp = first & 0x07; following = 3; }
      else return false;
      if (offset + following > value.length()) return false;
      for (uint8_t i = 0; i < following; ++i) {
        const uint8_t next = data[offset++];
        if ((next & 0xC0) != 0x80) return false;
        cp = (cp << 6) | (next & 0x3F);
      }
      if ((following == 1 && cp < 0x80) || (following == 2 && cp < 0x800) ||
          (following == 3 && cp < 0x10000) || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return false;
      if (cp < 0x20 || (cp >= 0x7F && cp <= 0x9F)) return false;
      if (cp != 0x20 && cp != 0x3000) nonspace = true;
      if (++characters > 8) return false;
    }
    return nonspace;
  }
  void noteMode() {
    const pet_animation::Mode mode = engine_.mode();
    if (mode == pet_animation::Mode::Eat && lastMode_ != mode) increment(foodCount_);
    if (mode != pet_animation::Mode::Jump) hearts_ = false;
    lastMode_ = mode;
  }
  static const char* actionName(pet_animation::Mode mode) {
    switch (mode) {
      case pet_animation::Mode::Walk: return "walk";
      case pet_animation::Mode::Jump: return "jump";
      case pet_animation::Mode::Roll: return "roll";
      case pet_animation::Mode::Bark: return "bark";
      case pet_animation::Mode::Eat: return "eat";
      case pet_animation::Mode::Rest: return "rest";
      default: return "idle";
    }
  }
  static const char* actionText(pet_animation::Mode mode) {
    switch (mode) {
      case pet_animation::Mode::Walk: return "自己溜达中";
      case pet_animation::Mode::Jump: return "开心蹦一下";
      case pet_animation::Mode::Roll: return "开心打个滚";
      case pet_animation::Mode::Bark: return "汪汪，打个招呼";
      case pet_animation::Mode::Eat: return "吃点狗粮";
      case pet_animation::Mode::Rest: return "歇一会儿";
      default: return "停下来看看";
    }
  }
  const char* caption() const {
    if (hearts_) return "汪！还想再被摸摸～";
    switch (engine_.mode()) {
      case pet_animation::Mode::Walk: return "这里走走，那里看看。";
      case pet_animation::Mode::Jump: return "嘿咻！我跳得高不高？";
      case pet_animation::Mode::Roll: return "翻过来，再翻回去～";
      case pet_animation::Mode::Bark: return "汪汪！和你打个招呼。";
      case pet_animation::Mode::Eat: return "啊呜啊呜，真香！";
      case pet_animation::Mode::Rest: return "玩累啦，歇一小会儿。";
      default: return "汪！我在这里陪着你。";
    }
  }
  void text(int x, int y, String value, int width, const uint8_t* font = u8g2_font_wqy12_t_gb2312) {
    font_.setFont(font);
    while (value.length() && font_.getUTF8Width(value.c_str()) > width) {
      int end = value.length() - 1;
      while (end > 0 && (static_cast<uint8_t>(value[end]) & 0xC0) == 0x80) --end;
      value.remove(end);
    }
    font_.setCursor(x, y); font_.print(value);
  }
  void centered(int baseline, const char* value) {
    font_.setFont(u8g2_font_wqy12_t_gb2312);
    text((400 - font_.getUTF8Width(value)) / 2, baseline, value, 364);
  }
  void heart(int x, int y, int size) {
    const int half = size / 2;
    epd_.fillCircle(x + size / 4, y + size / 4, size / 4, GxEPD_BLACK);
    epd_.fillCircle(x + size * 3 / 4, y + size / 4, size / 4, GxEPD_BLACK);
    epd_.fillTriangle(x, y + size / 3, x + size, y + size / 3, x + half, y + size, GxEPD_BLACK);
  }
  void drawDog() {
    const uint8_t frame = engine_.frame();
    const uint8_t safeFrame = frame < pet_art::kFrameCount ? frame : 0;
    const uint8_t* bitmap = pet_art::kFrames[safeFrame];
    const int16_t x = engine_.x();
    int16_t y = engine_.mode() == pet_animation::Mode::Eat ? 71 : 80;
    if (engine_.mode() == pet_animation::Mode::Jump) {
      static const int8_t offsets[6] = {0, 0, -8, -18, -8, 0};
      const uint8_t pose = engine_.pose();
      y += offsets[pose < 6 ? pose : 0];
    }
    if (engine_.mode() != pet_animation::Mode::Walk || engine_.direction() >= 0) {
      epd_.drawBitmap(x, y, bitmap, pet_art::kWidth, pet_art::kHeight, GxEPD_BLACK);
    } else {
      // Mirror only the sprite, never the shared EPD layout or other pages.
      for (uint16_t row = 0; row < pet_art::kHeight; ++row) {
        for (uint16_t col = 0; col < pet_art::kWidth; ++col) {
          const uint8_t byte = pgm_read_byte(bitmap + row * (pet_art::kWidth / 8) + col / 8);
          if (byte & (0x80 >> (col & 7))) epd_.drawPixel(x + pet_art::kWidth - 1 - col, y + row, GxEPD_BLACK);
        }
      }
    }
  }
  void drawBody() {
    font_.setForegroundColor(GxEPD_BLACK); font_.setBackgroundColor(GxEPD_WHITE);
    drawDog();
    text(18, 60, name_, 180, u8g2_font_wqy16_t_gb2312);
    const char* state = engine_.paused() ? "已暂停" : hearts_ ? "好喜欢你的摸摸" : actionText(engine_.mode());
    font_.setFont(u8g2_font_wqy12_t_gb2312);
    text(382 - font_.getUTF8Width(state), 59, state, 180);
    if (hearts_) { heart(78, 122, 12); heart(308, 100, 15); heart(319, 145, 9); }
    if (engine_.mode() == pet_animation::Mode::Bark && (engine_.pose() == 1 || engine_.pose() == 2)) {
      epd_.fillRoundRect(288, 91, 95, 35, 7, GxEPD_WHITE);
      epd_.drawRoundRect(288, 91, 95, 35, 7, GxEPD_BLACK);
      epd_.fillTriangle(291, 121, 303, 125, 290, 133, GxEPD_BLACK);
      text(308, 114, "汪汪！", 70, u8g2_font_wqy16_t_gb2312);
    }
    epd_.drawLine(64, 253, 336, 253, GxEPD_BLACK);
    centered(274, caption());
    epd_.drawLine(18, 282, 382, 282, GxEPD_BLACK);
    text(18, 296, "萨摩耶 · 圆脸奶团", 200);
    text(235, 296, String("摸摸 ") + patCount_ + " 次", 102);
    text(352, 296, "02/08", 36, u8g2_font_6x10_tf);
  }
  void render(bool full, bool header, const String& minute) {
    const uint32_t startedAt = millis();
    epd_.setRotation(0);
    if (full) epd_.setFullWindow();
    else if (header) epd_.setPartialWindow(0, 0, 400, 300);
    else epd_.setPartialWindow(0, 40, 400, 260);
    epd_.firstPage();
    do {
      epd_.fillScreen(GxEPD_WHITE);
      if (header) nekopaw_screen::drawHeader(epd_, font_);
      drawBody();
    } while (epd_.nextPage());
    lastRefreshMs_ = millis() - startedAt;
    if (full) { increment(fullRefreshes_); refreshPolicy_.didFull(millis()); }
    else { increment(partialRefreshes_); refreshPolicy_.didPartial(); }
    if (header) headerMinute_ = minute;
  }
};
