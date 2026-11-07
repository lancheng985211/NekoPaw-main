#pragma once

#include <stdint.h>
#include <string.h>

namespace screen_pages {

enum Index : uint8_t { Home, Pet, Course, Weather, Calendar, Codex, Delivery, Memo, Count };
constexpr uint8_t count = Count;

inline const char* name(uint8_t index) {
  static const char* const names[] = {
    "home", "pet", "course", "weather", "calendar", "codex", "delivery", "memo"
  };
  return names[index < count ? index : 0U];
}

inline const char* label(uint8_t index) {
  static const char* const labels[] = {
    "首页", "宠物", "课表", "天气", "日历", "Codex 用量", "外卖", "备忘录"
  };
  return labels[index < count ? index : static_cast<uint8_t>(Home)];
}

inline bool indexForName(const char* value, uint8_t& result) {
  if (!value) return false;
  for (uint8_t index = 0U; index < count; ++index) {
    if (strcmp(value, name(index)) == 0) {
      result = index;
      return true;
    }
  }
  return false;
}

// V3 stored seven pages starting with pet. Preserve the selected feature when
// inserting home; leave the original key untouched for a reversible upgrade.
inline uint8_t fromSevenPageIndex(uint8_t oldIndex) {
  return oldIndex < 7U ? static_cast<uint8_t>(oldIndex + 1U) : static_cast<uint8_t>(Home);
}

inline uint8_t cycle(uint8_t current, int direction) {
  const int start = current < count ? current : 0;
  // Reduce first: even INT_MIN/INT_MAX directions cannot overflow the addition.
  const int step = direction % static_cast<int>(count);
  return static_cast<uint8_t>((start + step + count) % count);
}

} // namespace screen_pages
