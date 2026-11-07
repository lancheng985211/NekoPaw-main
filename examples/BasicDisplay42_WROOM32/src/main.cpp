#include <Arduino.h>
#include <WiFi.h>
#include "wifi_config.h"
#include <GxEPD2_BW.h>
#include <gdey/GxEPD2_420_GDEY042T81.h>

#include <NekoPaw.h>
#include "CourseManager.h"
#include <nekopaw/adapters/ButtonInputAdapter.h>
#include <nekopaw/adapters/GxEPD2DisplayAdapter.h>

#ifndef NEKOPAW_WIFI_SSID
#define NEKOPAW_WIFI_SSID ""
#endif

#ifndef NEKOPAW_WIFI_PASSWORD
#define NEKOPAW_WIFI_PASSWORD ""
#endif

static const bool kHasWiFiCredentials = sizeof(NEKOPAW_WIFI_SSID) > 1 && sizeof(NEKOPAW_WIFI_PASSWORD) > 1;

namespace {

constexpr int kEpdSclk = 18;
constexpr int kEpdMiso = -1;
constexpr int kEpdMosi = 23;
constexpr int kEpdCs = 5;
constexpr int kEpdDc = 17;
constexpr int kEpdRst = 16;
constexpr int kEpdBusy = 4;
constexpr int kButton1Pin = 32;
constexpr int kButton2Pin = 33;

// 0 表示屏幕以 400 × 300 的方向工作。
constexpr uint8_t kScreenRotation = 0;

// GDEY042T81：4.2 英寸、黑白、400 × 300、SSD1683。

using ExampleEpdDriver = GxEPD2_420_GDEY042T81;

// The bridge reports dimensions after rotation, so bitmap uploads match the visible canvas.
constexpr uint16_t kNativeScreenWidth = ExampleEpdDriver::WIDTH;
constexpr uint16_t kNativeScreenHeight = ExampleEpdDriver::HEIGHT;
constexpr bool kScreenRotated = (kScreenRotation % 2U) != 0U;
constexpr uint16_t kScreenWidth = kScreenRotated ? kNativeScreenHeight : kNativeScreenWidth;
constexpr uint16_t kScreenHeight = kScreenRotated ? kNativeScreenWidth : kNativeScreenHeight;

using ExampleDisplay = GxEPD2_BW<ExampleEpdDriver, ExampleEpdDriver::HEIGHT>;

nekopaw::GxEPD2DisplayAdapter<ExampleDisplay>::Layout makeDisplayLayout() {
  nekopaw::GxEPD2DisplayAdapter<ExampleDisplay>::Layout layout;
  layout.width = kScreenWidth;
  layout.height = kScreenHeight;
  layout.rotation = kScreenRotation;
  layout.supportsPartial = true;
  return layout;
}

nekopaw::ButtonInputAdapter::Config makeButtonConfig(int pin, const char* id) {
  nekopaw::ButtonInputAdapter::Config config;
  config.pin = pin;
  config.id = id;
  return config;
}

nekopaw::NekoPaw::Config makePawConfig() {
  nekopaw::NekoPaw::Config config;
  config.httpPort = 80;
  config.deviceId = nullptr;
  config.description = "BasicDisplay e-ink bridge";
  return config;
}

ExampleDisplay epd(ExampleEpdDriver(kEpdCs, kEpdDc, kEpdRst, kEpdBusy));
const nekopaw::GxEPD2DisplayAdapter<ExampleDisplay>::Pins kDisplayPins = {kEpdSclk, kEpdMiso, kEpdMosi, kEpdCs,
                                                                           kEpdDc,   kEpdRst,  kEpdBusy};
const nekopaw::GxEPD2DisplayAdapter<ExampleDisplay>::Layout kDisplayLayout = makeDisplayLayout();
nekopaw::GxEPD2DisplayAdapter<ExampleDisplay> display(epd, kDisplayPins, kDisplayLayout);
WeatherManager<ExampleDisplay> weatherManager(epd);
CalendarManager<ExampleDisplay> calendarManager(epd);
CodexUsageManager<ExampleDisplay> codexUsageManager(epd);
DeliveryManager<ExampleDisplay> deliveryManager(epd);
PetManager<ExampleDisplay> petManager(epd);
MemoManager<ExampleDisplay> memoManager(epd);
CourseManager<ExampleDisplay> courseManager(epd, weatherManager, calendarManager, codexUsageManager, deliveryManager, petManager, memoManager);
class PageButton : public nekopaw::ButtonInputAdapter {
public:
  PageButton(const Config& config,int direction):nekopaw::ButtonInputAdapter(config),direction_(direction) {}
  Event poll() override { Event event=nekopaw::ButtonInputAdapter::poll(); if(event==Event::Click)courseManager.cyclePage(direction_); return event; }
private:
  int direction_;
};
const nekopaw::ButtonInputAdapter::Config kButton1Config = makeButtonConfig(kButton1Pin, "button1");
const nekopaw::ButtonInputAdapter::Config kButton2Config = makeButtonConfig(kButton2Pin, "button2");
PageButton button1(kButton1Config,1);
PageButton button2(kButton2Config,-1);
const nekopaw::NekoPaw::Config kPawConfig = makePawConfig();
nekopaw::NekoPaw paw(kPawConfig);

void showScreen(const char* title, const String& body, const char* footer = nullptr, const char* style = "default",
                bool fullRefresh = true) {
  nekopaw::DisplayProvider::TextContent content;
  content.title = title;
  content.body = body.c_str();
  content.footer = footer;
  content.style = style;
  (void)display.showText(content, fullRefresh);
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(NEKOPAW_WIFI_SSID, NEKOPAW_WIFI_PASSWORD);

  Serial.printf("Connecting to %s", NEKOPAW_WIFI_SSID);
  const uint32_t startedAt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startedAt < 30000UL) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
}

void showMissingCredentialsScreen() {
  showScreen("NekoPaw", "WiFi credentials missing.\nEdit platformio_override.ini", "Need SSID + PASSWORD", "alert");
}

void showConnectingScreen() {
  String body = "Connecting to WiFi...\nSSID: ";
  body += NEKOPAW_WIFI_SSID;
  showScreen("NekoPaw", body, "Waiting for network", "default");
}

void showWiFiFailureScreen() {
  showScreen("NekoPaw", "WiFi connection failed.\nCheck SSID/password", "Bridge offline", "alert");
}

void showWelcomeScreen(const String& ip) {
  String body = "WiFi connected.\nIP: ";
  body += ip;
  body += "\nGET /api/bridge/device";
  showScreen("NekoPaw Ready", body, "POST /api/bridge/display/text", "success");
}

} // namespace

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println("NekoPaw BasicDisplay");

  paw.setDisplay(&display);

  // 当前硬件：墨水屏和两个翻页按键。
  paw.addInput(&button1);
  paw.addInput(&button2);

  if (!kHasWiFiCredentials) {
    showMissingCredentialsScreen();
    Serial.println("WiFi credentials are not configured.");
    Serial.println("Configure src/wifi_config.h with your WiFi SSID and password.");
    Serial.println("    -DNEKOPAW_WIFI_SSID=\\\"your-wifi-ssid\\\"");
    Serial.println("    -DNEKOPAW_WIFI_PASSWORD=\\\"your-wifi-password\\\"");
    return;
  }

  showConnectingScreen();
  connectWiFi();
  if (WiFi.status() != WL_CONNECTED) {
    showWiFiFailureScreen();
    Serial.println("WiFi unavailable; starting local services and cached pages.");
  }

  const String ip = WiFi.localIP().toString();
  Serial.print(WiFi.status() == WL_CONNECTED ? "WiFi connected, IP: " : "WiFi offline, IP: ");
  Serial.println(ip);
  if (WiFi.status() == WL_CONNECTED) showWelcomeScreen(ip);

  if (!paw.begin()) {
    Serial.println("NekoPaw begin failed.");
    return;
  }

  petManager.begin();
  memoManager.begin();
  courseManager.begin();
  weatherManager.begin();
  calendarManager.begin();
  codexUsageManager.begin();
  deliveryManager.begin();

  Serial.print("Device info: http://");
  Serial.print(ip);
  Serial.println("/api/bridge/device");
  Serial.print("Sensors: http://");
  Serial.print(ip);
  Serial.println("/api/bridge/sensors");
  Serial.print("Events: http://");
  Serial.print(ip);
  Serial.println("/api/bridge/events");
  Serial.print("Confirm: http://");
  Serial.print(ip);
  Serial.println("/api/bridge/display/confirm");
  Serial.print("Outputs: http://");
  Serial.print(ip);
  Serial.println("/api/bridge/outputs?id=led_rgb");
}

void loop() {
  static uint32_t reconnectAt=0;
  if(kHasWiFiCredentials && WiFi.status()!=WL_CONNECTED && millis()-reconnectAt>=30000){reconnectAt=millis();WiFi.reconnect();}
  courseManager.loop();
  weatherManager.loop();
  calendarManager.loop();
  codexUsageManager.loop();
  deliveryManager.loop();
  petManager.loop();
  memoManager.loop();
  paw.loop();
  delay(10);
}

