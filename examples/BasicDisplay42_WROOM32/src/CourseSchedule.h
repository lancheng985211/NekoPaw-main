#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

namespace course_schedule {
inline bool validTime(const String& s) {
  return s.length() == 5 && s[2] == ':' && isDigit(s[0]) && isDigit(s[1]) && isDigit(s[3]) &&
    isDigit(s[4]) && s.substring(0, 2).toInt() < 24 && s.substring(3).toInt() < 60;
}
inline bool validDate(const String& s) {
  if (s.length() != 10 || s[4] != '-' || s[7] != '-') return false;
  for (int i = 0; i < 10; ++i) if (i != 4 && i != 7 && !isDigit(s[i])) return false;
  int y = s.substring(0, 4).toInt(), m = s.substring(5, 7).toInt(), d = s.substring(8).toInt();
  const int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  return y >= 2024 && y <= 2099 && m >= 1 && m <= 12 && d >= 1 &&
    d <= days[m - 1] + (m == 2 && y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
}
inline int dayNumber(const String& date) {
  int year = date.substring(0, 4).toInt(), month = date.substring(5, 7).toInt(), previous = year - 1;
  const int beforeMonth[] = {0,31,59,90,120,151,181,212,243,273,304,334};
  return previous * 365 + previous / 4 - previous / 100 + previous / 400 + beforeMonth[month - 1] +
    date.substring(8).toInt() - 1 + (month > 2 && year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}
inline int teachingWeek(const JsonDocument& doc, const char* date) {
  String start = doc["termStart"] | "";
  if (!start.length()) return 0;
  int days = dayNumber(date) - dayNumber(start);
  return days < 0 ? 0 : days / 7 + 1;
}
inline bool weeksValid(JsonVariantConst value) {
  if (value.isNull()) return true; // No restriction: every teaching week.
  if (!value.is<JsonArrayConst>() || value.size() < 1 || value.size() > 30) return false;
  int previous = 0;
  for (JsonVariantConst item : value.as<JsonArrayConst>()) {
    if (!item.is<int>() || item.as<int>() <= previous || item.as<int>() > 30) return false;
    previous = item.as<int>();
  }
  return true;
}
inline bool onWeek(JsonVariantConst meeting, int week) {
  if (meeting["weeks"].isNull()) return true;
  for (int number : meeting["weeks"].as<JsonArrayConst>()) if (number == week) return true;
  return false;
}
inline bool rowValid(JsonVariantConst row) {
  if (!row.is<JsonObjectConst>()) return false;
  for (const char* field : {"name", "start", "end", "room"}) if (!row[field].is<const char*>()) return false;
  const String name = row["name"].as<String>(), room = row["room"].as<String>();
  const String start = row["start"].as<String>(), end = row["end"].as<String>();
  return name.length() && name.length() <= 48 && room.length() <= 36 && validTime(start) && validTime(end) && start < end && weeksValid(row["weeks"]);
}
inline bool rowsValid(JsonVariantConst value) {
  if (!value.is<JsonArrayConst>() || value.size() != 5) return false;
  for (JsonVariantConst row : value.as<JsonArrayConst>()) if (!row.isNull() && !rowValid(row)) return false;
  return true;
}
inline bool validate(const JsonDocument& doc, String& error) {
  error = "课表格式无效，请刷新管理网页";
  if (doc["schema"].as<int>() != 3 || !doc["courses"].is<JsonArrayConst>() || doc["courses"].size() > 40 ||
      !doc["overrides"].is<JsonArrayConst>() || doc["overrides"].size() > 10) return false;
  String start = doc["termStart"] | "";
  if (!doc["termStart"].is<const char*>() || !doc["termWeeks"].is<int>() || doc["termWeeks"].as<int>() < 1 || doc["termWeeks"].as<int>() > 30) return false;
  if (start.length() && (!validDate(start) || (dayNumber(start) - dayNumber("2024-01-01")) % 7 != 0)) {
    error = "学期第一周起始日期必须是有效的周一"; return false;
  }
  auto courses = doc["courses"].as<JsonArrayConst>();
  int totalMeetings = 0;
  for (size_t i = 0; i < courses.size(); ++i) {
    auto c = courses[i];
    for (const char* field : {"id", "name", "teacher"}) if (!c[field].is<const char*>()) return false;
    String id = c["id"].as<String>(), name = c["name"].as<String>(), teacher = c["teacher"].as<String>();
    if (!id.length() || id.length() > 32 || !name.length() || name.length() > 48 || teacher.length() > 36) return false;
    for (size_t j = 0; j < i; ++j) if (id == courses[j]["id"].as<String>()) return false;
    if (!c["meetings"].is<JsonArrayConst>() || c["meetings"].size() < 1 || c["meetings"].size() > 70) {
      error = "每门课需要至少一个上课时间，合计最多70个"; return false;
    }
    for (JsonObjectConst m : c["meetings"].as<JsonArrayConst>()) {
      ++totalMeetings;
      if (!m["weekday"].is<int>() || m["weekday"].as<int>() < 0 || m["weekday"].as<int>() > 6 ||
          !m["start"].is<const char*>() || !m["end"].is<const char*>() || !m["room"].is<const char*>()) return false;
      String first = m["start"].as<String>(), last = m["end"].as<String>();
      if (!validTime(first) || !validTime(last) || first >= last || m["room"].as<String>().length() > 36 || !weeksValid(m["weeks"])) {
        error = "请检查课程的起止时间、周数和教室"; return false;
      }
      if (!m["weeks"].isNull() && !start.length()) { error = "选择指定周数前，请先填写学期第一周的周一日期"; return false; }
    }
  }
  if (totalMeetings > 70) { error = "上课时间总数不能超过70个"; return false; }
  auto overrides = doc["overrides"].as<JsonArrayConst>();
  for (size_t i = 0; i < overrides.size(); ++i) {
    auto item = overrides[i];
    if (!item["date"].is<const char*>() || !validDate(item["date"].as<String>()) || !rowsValid(item["lessons"])) return false;
    for (size_t j = 0; j < i; ++j) if (item["date"].as<String>() == overrides[j]["date"].as<String>()) return false;
  }
  return true;
}
inline void blank(JsonDocument& doc) {
  doc.clear(); doc["schema"] = 3; doc["termStart"] = ""; doc["termWeeks"] = 20;
  doc["courses"].to<JsonArray>(); doc["overrides"].to<JsonArray>();
}
// Upgrade a copy in RAM. The previous NVS entry remains intact until the user saves.
inline bool migrate(JsonDocument& doc, String& error) {
  if (doc["schema"].as<int>() == 3) return validate(doc, error);
  auto week = doc["weekly"].as<JsonArrayConst>();
  if (week.size() != 7 || !doc["overrides"].is<JsonArrayConst>()) return false;
  for (JsonVariantConst rows : week) if (!rowsValid(rows)) return false;
  JsonDocument converted; blank(converted);
  converted["termStart"] = doc["termStart"] | ""; converted["termWeeks"] = doc["termWeeks"] | 20;
  converted["revision"] = doc["revision"] | 0U;
  converted["overrides"].set(doc["overrides"]);
  auto courses = converted["courses"].as<JsonArray>();
  for (int day = 0; day < 7; ++day) for (JsonVariantConst row : week[day].as<JsonArrayConst>()) {
    if (row.isNull()) continue;
    JsonObject course;
    for (JsonObject existing : courses) if (existing["name"].as<String>() == row["name"].as<String>()) { course = existing; break; }
    if (course.isNull()) {
      course = courses.add<JsonObject>(); course["id"] = String("legacy-") + courses.size();
      course["name"] = row["name"]; course["teacher"] = ""; course["meetings"].to<JsonArray>();
    }
    auto meeting = course["meetings"].as<JsonArray>().add<JsonObject>();
    meeting["weekday"] = day;
    for (const char* field : {"start", "end", "room"}) meeting[field] = row[field];
    if (!row["weeks"].isNull()) meeting["weeks"].set(row["weeks"]);
  }
  if (!validate(converted, error)) return false;
  doc = converted; return true;
}
inline bool overlap(JsonVariantConst a, JsonVariantConst b) {
  return a["start"].as<String>() < b["end"].as<String>() && b["start"].as<String>() < a["end"].as<String>();
}
// Check conflicts by actual weekday, time and week; adjacent times are permitted.
inline bool conflicts(const JsonDocument& doc, String& error) {
  for (int w = 1; w <= (doc["termWeeks"] | 20); ++w) for (int day = 0; day < 7; ++day) {
    JsonVariantConst found[5]; int count = 0;
    for (JsonObjectConst c : doc["courses"].as<JsonArrayConst>()) for (JsonObjectConst m : c["meetings"].as<JsonArrayConst>()) {
      if (m["weekday"].as<int>() != day || !onWeek(m, w)) continue;
      for (int i = 0; i < count; ++i) if (overlap(found[i], m)) {
        error = String("第") + w + "周的同一天存在上课时间重叠，请调整后保存"; return true;
      }
      if (count >= 5) { error = String("第") + w + "周某天超过五节课，当前屏幕最多显示五节"; return true; }
      found[count++] = m;
    }
  }
  for (JsonObjectConst day : doc["overrides"].as<JsonArrayConst>()) {
    auto rows = day["lessons"].as<JsonArrayConst>();
    for (size_t i = 0; i < rows.size(); ++i) for (size_t j = 0; j < i; ++j)
      if (!rows[i].isNull() && !rows[j].isNull() && overlap(rows[i], rows[j])) {
        error = day["date"].as<String>() + " 的临时课程时间重叠"; return true;
      }
  }
  return false;
}
inline void insert(JsonArray rows, const char* name, JsonVariantConst source) {
  int pos = 0;
  while (pos < 5 && !rows[pos].isNull() && rows[pos]["start"].as<String>() <= source["start"].as<String>()) ++pos;
  if (pos == 5) return;
  for (int i = 4; i > pos; --i) rows[i].set(rows[i - 1]);
  auto row = rows[pos].to<JsonObject>(); row["name"] = name;
  for (const char* field : {"start", "end", "room"}) row[field] = source[field];
}
inline void resolveRows(const JsonDocument& doc, const char* date, int weekday, JsonDocument& output) {
  output.clear(); auto rows = output.to<JsonArray>(); for (int i = 0; i < 5; ++i) rows.add(nullptr);
  for (JsonObjectConst item : doc["overrides"].as<JsonArrayConst>()) if (item["date"].as<String>() == date) {
    for (JsonVariantConst row : item["lessons"].as<JsonArrayConst>()) if (!row.isNull()) insert(rows, row["name"], row);
    return;
  }
  int week = teachingWeek(doc, date); const String start = doc["termStart"] | "";
  if (start.length() && (week < 1 || week > (doc["termWeeks"] | 20))) return;
  for (JsonObjectConst c : doc["courses"].as<JsonArrayConst>()) for (JsonObjectConst m : c["meetings"].as<JsonArrayConst>())
    if (m["weekday"].as<int>() == weekday && onWeek(m, week)) insert(rows, c["name"], m);
}
} // namespace course_schedule
