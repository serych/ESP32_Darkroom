#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <TFT_eSPI.h>
#include <Preferences.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <math.h>

#include "darkroom_hw.h"

namespace {

constexpr uint8_t kLedPreviewBrightness = 64;
constexpr uint16_t kLightHeadMax = 1023;
constexpr uint16_t kTftResetPulseMs = 30;
constexpr uint16_t kTftResetSettlingMs = 180;

constexpr uint8_t kBrightnessSteps[] = {255, 128, 64, 32, 16, 8, 4, 2, 1, 0, 1, 2, 4, 8, 16, 32, 64, 128};
constexpr size_t kBrightnessStepCount = sizeof(kBrightnessSteps) / sizeof(kBrightnessSteps[0]);

constexpr uint16_t kStartupTonesHz[] = {523, 659, 784};
constexpr uint32_t kStartupToneDurationMs = 120;
constexpr uint32_t kStartupGapMs = 80;
constexpr uint32_t kClickToneDurationMs = 15;
constexpr uint32_t kWifiConnectTimeoutMs = 30000;
constexpr char kWifiPrefsNamespace[] = "wifi";
constexpr char kWifiSsidKey[] = "ssid";
constexpr char kWifiPasswordKey[] = "password";
enum class UiMode : uint8_t {
  BootConnect,
  WifiScan,
  WifiScanResult,
  WifiPasswordEntry,
  WifiConnecting,
  WifiConnected,
  ExposureMode,
  ConfigMode,
};

enum class ExposureFocus : uint8_t {
  Exposure,
  Contrast,
  Aperture,
};

struct NetworkEntry {
  String ssid;
  int32_t rssi;
  wifi_auth_mode_t authMode;
};

struct ConfigMenuEntry {
  const char* label;
  const char* value;
};

struct ContrastRgbSetting {
  uint8_t redStep;
  uint8_t greenStep;
  uint8_t blueStep;
};


TFT_eSPI tft = TFT_eSPI();
Adafruit_NeoPixel statusLed(DarkroomHw::kStatusLedCount, DarkroomHw::kStatusLedPin, NEO_GRB + NEO_KHZ800);
Preferences preferences;

bool prevLightOnPressed = false;
bool prevLightOffPressed = false;
bool prevLightTimerPressed = false;
bool prevDeveloperPressed = false;
bool prevEncoderButtonPressed = false;
uint32_t encoderButtonPressedAt = 0;
bool encoderLongPressHandled = false;

UiMode uiMode = UiMode::BootConnect;
String wifiSsid;
String wifiPassword;
bool wifiCredentialsValid = false;
bool wifiConnectTimedOut = false;
uint32_t wifiConnectStartedAt = 0;
uint32_t lastWifiStatusRefreshAt = 0;
bool wifiConnectedScreenShown = false;
uint32_t wifiConnectedShownAt = 0;
bool otaStarted = false;
int16_t wifiScanCount = -1;
constexpr size_t kMaxStoredNetworks = 20;
NetworkEntry networks[kMaxStoredNetworks];
size_t networkCount = 0;
int32_t selectedNetworkIndex = 0;
String selectedSsid;
String passwordBuffer;
size_t passwordCursor = 0;
size_t selectedCharIndex = 0;
constexpr char kPasswordAlphabet[] =
    " abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.@!?#$%&+=/:";
String connectionStatusMessage;
int32_t configMenuIndex = 0;
constexpr ConfigMenuEntry kConfigMenuEntries[] = {
    {"WiFi setup", "saved"},
    {"Head colors", "later"},
    {"Brightness", "later"},
    {"Sensor", "later"},
};
constexpr size_t kConfigMenuEntryCount = sizeof(kConfigMenuEntries) / sizeof(kConfigMenuEntries[0]);
constexpr uint32_t kEncoderLongPressMs = 700;
constexpr float kExposureStepRatio = 1.41421356f;
constexpr uint16_t kMaxExposureSeconds = 600;
constexpr size_t kMaxExposureStepCount = 32;
size_t exposureStepCount = 0;
float exposureSeconds[kMaxExposureStepCount];
int32_t exposureIndex = 0;
int32_t contrastValue = 4;
int32_t apertureValue = 0;
ExposureFocus exposureFocus = ExposureFocus::Exposure;
float contrastCorrectionTable[11] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
ContrastRgbSetting contrastRgbTable[11] = {
    {3, 5, 5}, {3, 5, 5}, {3, 5, 5}, {3, 5, 5}, {3, 5, 5}, {3, 5, 5},
    {3, 5, 5}, {3, 5, 5}, {3, 5, 5}, {3, 5, 5}, {3, 5, 5},
};
bool exposureRunning = false;
uint32_t exposureStartedAt = 0;
uint32_t exposureDurationMs = 0;
uint32_t lastExposureUiRefreshAt = 0;

void drawWifiConnectedUi();
void ensureOtaStarted();
void drawExposureModeUi();
void drawConfigModeUi();
void enterExposureMode();
void enterConfigMode();
void initializeExposureSteps();
String formatExposureSeconds(float seconds);
uint16_t pwmFromLogStep(uint8_t step);
void applyExposureLightOutput();
void stopExposure(bool completed);
void startExposure();
void drawExposureFooter();

void setStatusLed(uint8_t red, uint8_t green, uint8_t blue) {
  statusLed.setPixelColor(0, statusLed.Color(red, green, blue));
  statusLed.show();
}

void setPreviewColor(uint16_t red, uint16_t green, uint16_t blue, uint8_t statusRed, uint8_t statusGreen, uint8_t statusBlue) {
  DarkroomHw::setLightHeadRgb(red, green, blue);
  setStatusLed(statusRed, statusGreen, statusBlue);
}

void hardResetDisplay() {
  pinMode(DarkroomHw::PinAssignment::kTftRst, OUTPUT);
  digitalWrite(DarkroomHw::PinAssignment::kTftRst, HIGH);
  delay(10);
  digitalWrite(DarkroomHw::PinAssignment::kTftRst, LOW);
  delay(kTftResetPulseMs);
  digitalWrite(DarkroomHw::PinAssignment::kTftRst, HIGH);
  delay(kTftResetSettlingMs);
}

void drawBootUi() {
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextFont(4);
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.drawString("Darkroom controler", 8, 10, 4);
}

void drawWifiStatusLine(const String& text, uint16_t color) {
  tft.setRotation(1);
  tft.fillRect(0, 44, tft.width(), 28, TFT_BLACK);
  tft.setTextFont(4);
  tft.setTextColor(color, TFT_BLACK);
  tft.drawString(text, 8, 48, 4);
}

void drawFooterLine(const String& text, uint16_t color = TFT_DARKGREY) {
  tft.setRotation(1);
  tft.fillRect(0, 214, tft.width(), 22, TFT_BLACK);
  tft.setTextFont(2);
  tft.setTextColor(color, TFT_BLACK);
  tft.drawString(text, 8, 218, 2);
}

void loadWifiCredentials() {
  wifiCredentialsValid = false;
  wifiSsid = "";
  wifiPassword = "";

  if (!preferences.begin(kWifiPrefsNamespace, true)) {
    return;
  }

  wifiSsid = preferences.getString(kWifiSsidKey, "");
  wifiPassword = preferences.getString(kWifiPasswordKey, "");
  preferences.end();

  wifiCredentialsValid = !wifiSsid.isEmpty();
}

void beginWifiConnectIfNeeded() {
  if (!wifiCredentialsValid) {
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());
  wifiConnectStartedAt = millis();
  wifiConnectTimedOut = false;
  lastWifiStatusRefreshAt = 0;
}

void updateWifiStatusUi() {
  if (!wifiCredentialsValid) {
    wifiConnectedScreenShown = false;
    drawWifiStatusLine("WiFi setup", TFT_CYAN);
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiConnectedScreenShown) {
      uiMode = UiMode::WifiConnected;
      selectedSsid = WiFi.SSID();
      ensureOtaStarted();
      drawWifiConnectedUi();
      wifiConnectedScreenShown = true;
      wifiConnectedShownAt = millis();
      lastWifiStatusRefreshAt = millis();
    }
    return;
  }

  if (wifiConnectTimedOut) {
    wifiConnectedScreenShown = false;
    drawWifiStatusLine("WiFi setup", TFT_CYAN);
    return;
  }

  wifiConnectedScreenShown = false;
  const uint32_t elapsedMs = millis() - wifiConnectStartedAt;
  const uint32_t remainingSec =
      (elapsedMs >= kWifiConnectTimeoutMs) ? 0 : ((kWifiConnectTimeoutMs - elapsedMs + 999) / 1000);
  drawWifiStatusLine("Trying to connect " + String(remainingSec) + "s", TFT_YELLOW);
}

void drawWifiScanResultUi() {
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextFont(4);
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.drawString("Darkroom controler", 8, 10, 4);

  tft.setTextFont(4);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("WiFi setup", 8, 48, 4);

  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Scan done:", 8, 96, 2);
  tft.drawString(String(wifiScanCount), 92, 96, 2);

  if (wifiScanCount < 0) {
    tft.drawString("Scan error", 8, 122, 2);
  } else if (wifiScanCount == 0) {
    tft.drawString("No SSIDs found", 8, 122, 2);
  } else {
    tft.drawString("WiFi scan completed", 8, 122, 2);
    tft.drawString("SSID:", 8, 148, 2);
    tft.drawString(String(selectedNetworkIndex + 1) + "/" + String(networkCount), 64, 148, 2);

    String selectedSsid = networks[selectedNetworkIndex].ssid;
    if (selectedSsid.isEmpty()) {
      selectedSsid = "<hidden>";
    }
    const String selectedLock = (networks[selectedNetworkIndex].authMode == WIFI_AUTH_OPEN) ? "open" : "lock";
    tft.drawString(selectedSsid.substring(0, 22), 8, 166, 2);
    tft.drawString(String("RSSI ") + String(networks[selectedNetworkIndex].rssi) + "  " + selectedLock, 8, 184, 2);
  }

  drawFooterLine("Rotary = next/prev SSID", TFT_GREEN);
}

void drawPasswordEntryUi() {
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextFont(4);
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.drawString("Darkroom controler", 8, 10, 4);

  tft.setTextFont(4);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("Enter password", 8, 48, 4);

  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("SSID:", 8, 86, 2);
  tft.drawString(selectedSsid.substring(0, 24), 56, 86, 2);

  tft.setTextFont(4);
  tft.drawString("Pass:", 8, 112, 4);
  tft.drawString(passwordBuffer.substring(0, 18), 74, 112, 4);

  tft.setTextFont(2);
  tft.drawString("Cursor:", 8, 156, 2);
  tft.drawString(String(passwordCursor), 70, 156, 2);

  const char selectedChar = kPasswordAlphabet[selectedCharIndex];
  tft.drawString("Char:", 8, 178, 2);
  tft.fillRect(58, 172, 38, 28, TFT_BLACK);
  tft.drawRect(58, 172, 38, 28, TFT_WHITE);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawCentreString(String(selectedChar), 77, 178, 2);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Rotary=char  Enc btn=place", 94, 178, 2);
  tft.drawString("ON:< OFF:del TIMER:> DEV:OK", 8, 198, 2);
  drawFooterLine("Password entry", TFT_GREEN);
}

void drawWifiConnectingUi() {
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextFont(4);
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.drawString("Darkroom controler", 8, 10, 4);

  tft.setTextFont(4);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString("Connecting...", 8, 48, 4);

  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("SSID:", 8, 94, 2);
  tft.drawString(selectedSsid.substring(0, 24), 56, 94, 2);
  tft.drawString(connectionStatusMessage.substring(0, 28), 8, 122, 2);
  drawFooterLine("Please wait", TFT_YELLOW);
}

void drawWifiConnectedUi() {
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextFont(4);
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.drawString("Darkroom controler", 8, 10, 4);

  tft.setTextFont(4);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString("WiFi connected", 8, 48, 4);

  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("SSID:", 8, 94, 2);
  tft.drawString(selectedSsid.substring(0, 24), 56, 94, 2);
  tft.drawString("IP:", 8, 120, 2);
  tft.drawString(WiFi.localIP().toString(), 36, 120, 2);
  tft.drawString("RSSI:", 8, 146, 2);
  tft.drawString(String(WiFi.RSSI()) + " dBm", 48, 146, 2);
  drawFooterLine("Credentials saved, OTA ready", TFT_GREEN);
}

void drawExposureModeUi() {
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextFont(4);
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.drawString("Darkroom controler", 8, 10, 4);

  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString("Expozice", 8, 48, 4);

  const float correctedExposure = exposureSeconds[exposureIndex] * contrastCorrectionTable[contrastValue];

  const uint16_t exposureBackground = (exposureFocus == ExposureFocus::Exposure) ? TFT_DARKGREY : TFT_BLACK;
  const uint16_t contrastBackground = (exposureFocus == ExposureFocus::Contrast) ? TFT_DARKGREY : TFT_BLACK;
  const uint16_t apertureBackground = (exposureFocus == ExposureFocus::Aperture) ? TFT_DARKGREY : TFT_BLACK;

  tft.setTextFont(4);
  tft.fillRect(0, 92, tft.width(), 34, exposureBackground);
  tft.setTextColor((exposureFocus == ExposureFocus::Exposure) ? TFT_YELLOW : TFT_WHITE, exposureBackground);
  tft.drawString("Cas:", 8, 96, 4);
  tft.drawRightString(formatExposureSeconds(correctedExposure), tft.width() - 8, 96, 4);

  tft.setTextFont(4);
  tft.fillRect(0, 132, tft.width(), 34, contrastBackground);
  tft.setTextColor((exposureFocus == ExposureFocus::Contrast) ? TFT_YELLOW : TFT_WHITE, contrastBackground);
  tft.drawString("Kontrast:", 8, 136, 4);
  tft.drawRightString(String(contrastValue), tft.width() - 8, 136, 4);

  tft.setTextFont(4);
  tft.fillRect(0, 172, tft.width(), 34, apertureBackground);
  tft.setTextColor((exposureFocus == ExposureFocus::Aperture) ? TFT_YELLOW : TFT_WHITE, apertureBackground);
  tft.drawString("Clona:", 8, 176, 4);
  tft.drawRightString(String(apertureValue), tft.width() - 8, 176, 4);

  drawExposureFooter();
}

void drawExposureFooter() {
  if (exposureRunning) {
    const uint32_t elapsedMs = millis() - exposureStartedAt;
    const uint32_t remainingMs = (elapsedMs >= exposureDurationMs) ? 0 : (exposureDurationMs - elapsedMs);
    drawFooterLine("Bezi, zbyva " + formatExposureSeconds(static_cast<float>(remainingMs) / 1000.0f), TFT_YELLOW);
  } else {
    drawFooterLine("Timer=start  Off=stop  Long=config", TFT_GREEN);
  }
}

void drawConfigModeUi() {
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextFont(4);
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.drawString("Darkroom controler", 8, 10, 4);

  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("Konfigurace", 8, 48, 4);

  tft.setTextFont(2);
  for (size_t i = 0; i < kConfigMenuEntryCount; ++i) {
    const uint16_t background = (static_cast<int32_t>(i) == configMenuIndex) ? TFT_DARKGREY : TFT_BLACK;
    const uint16_t color = (static_cast<int32_t>(i) == configMenuIndex) ? TFT_YELLOW : TFT_WHITE;
    const int16_t y = 96 + static_cast<int16_t>(i) * 28;
    tft.fillRect(0, y - 2, tft.width(), 24, background);
    tft.setTextColor(color, background);
    tft.drawString(kConfigMenuEntries[i].label, 8, y, 2);
    tft.drawRightString(kConfigMenuEntries[i].value, tft.width() - 8, y, 2);
  }

  drawFooterLine("Rotary=select  Long=expozice", TFT_GREEN);
}

void enterExposureMode() {
  uiMode = UiMode::ExposureMode;
  drawExposureModeUi();
}

void enterConfigMode() {
  uiMode = UiMode::ConfigMode;
  drawConfigModeUi();
}

String formatExposureSeconds(float seconds) {
  if (seconds < 10.0f) {
    return String(seconds, 1) + " s";
  }

  const uint32_t roundedSeconds = static_cast<uint32_t>(seconds + 0.5f);
  if (roundedSeconds < 60) {
    return String(roundedSeconds) + " s";
  }

  const uint32_t minutes = roundedSeconds / 60;
  const uint32_t remainingSeconds = roundedSeconds % 60;
  if (remainingSeconds == 0) {
    return String(minutes) + " min";
  }

  return String(minutes) + "m " + String(remainingSeconds) + "s";
}

uint16_t pwmFromLogStep(uint8_t step) {
  if (step == 0) {
    return 0;
  }

  const uint16_t value = static_cast<uint16_t>(1U << (step - 1));
  return (value > kLightHeadMax) ? kLightHeadMax : value;
}

void initializeExposureSteps() {
  exposureStepCount = 0;
  float value = 1.0f;
  while (exposureStepCount < kMaxExposureStepCount && value <= static_cast<float>(kMaxExposureSeconds)) {
    exposureSeconds[exposureStepCount++] = value;
    value *= kExposureStepRatio;
  }

  if (exposureStepCount == 0) {
    exposureSeconds[0] = 1.0f;
    exposureStepCount = 1;
  }
}

void applyExposureLightOutput() {
  if (!exposureRunning) {
    setPreviewColor(0, 0, 0, 0, 0, 0);
    return;
  }

  const ContrastRgbSetting& rgb = contrastRgbTable[contrastValue];
  const uint16_t red = pwmFromLogStep(rgb.redStep);
  const uint16_t green = pwmFromLogStep(rgb.greenStep);
  const uint16_t blue = pwmFromLogStep(rgb.blueStep);

  const uint8_t statusRed = static_cast<uint8_t>((red * kLedPreviewBrightness) / kLightHeadMax);
  const uint8_t statusGreen = static_cast<uint8_t>((green * kLedPreviewBrightness) / kLightHeadMax);
  const uint8_t statusBlue = static_cast<uint8_t>((blue * kLedPreviewBrightness) / kLightHeadMax);
  setPreviewColor(red, green, blue, statusRed, statusGreen, statusBlue);
}

void stopExposure(bool completed) {
  exposureRunning = false;
  exposureDurationMs = 0;
  applyExposureLightOutput();
  if (uiMode == UiMode::ExposureMode) {
    drawExposureModeUi();
    drawFooterLine(completed ? "Exposure complete" : "Exposure stopped", completed ? TFT_GREEN : TFT_YELLOW);
  }
}

void startExposure() {
  const float correctedExposure = exposureSeconds[exposureIndex] * contrastCorrectionTable[contrastValue];
  const float clampedExposure = (correctedExposure < 0.1f) ? 0.1f : correctedExposure;
  exposureDurationMs = static_cast<uint32_t>(clampedExposure * 1000.0f + 0.5f);
  exposureStartedAt = millis();
  lastExposureUiRefreshAt = 0;
  exposureRunning = true;
  applyExposureLightOutput();
  if (uiMode == UiMode::ExposureMode) {
    drawExposureModeUi();
  }
}

void drawWifiScanUi() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextFont(4);
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.drawString("Darkroom controler", 8, 10, 4);

  tft.setTextFont(2);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("WiFi setup", 8, 48, 2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Scanning WiFi, wait please", 8, 84, 2);
  drawFooterLine("Scanning...", TFT_YELLOW);
}

void runWifiScanDiagnostic() {
  networkCount = 0;
  selectedNetworkIndex = 0;
  uiMode = UiMode::WifiScan;
  drawWifiScanUi();
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, false);
  delay(50);
  wifiScanCount = WiFi.scanNetworks();

  if (wifiScanCount > 0) {
    const size_t count = static_cast<size_t>(wifiScanCount) > kMaxStoredNetworks ? kMaxStoredNetworks : static_cast<size_t>(wifiScanCount);
    for (size_t i = 0; i < count; ++i) {
      networks[i].ssid = WiFi.SSID(i);
      networks[i].rssi = WiFi.RSSI(i);
      networks[i].authMode = static_cast<wifi_auth_mode_t>(WiFi.encryptionType(i));
    }
    networkCount = count;
  }

  WiFi.scanDelete();
  uiMode = UiMode::WifiScanResult;
  drawWifiScanResultUi();
}

void beginPasswordEntry() {
  selectedSsid = networks[selectedNetworkIndex].ssid;
  passwordBuffer = "";
  passwordCursor = 0;
  selectedCharIndex = 0;
  uiMode = UiMode::WifiPasswordEntry;
  drawPasswordEntryUi();
}

void moveSelectedCharacter(int32_t delta) {
  const int32_t alphabetSize = static_cast<int32_t>(strlen(kPasswordAlphabet));
  int32_t index = static_cast<int32_t>(selectedCharIndex) + delta;
  if (index < 0) {
    index = alphabetSize - 1;
  } else if (index >= alphabetSize) {
    index = 0;
  }
  selectedCharIndex = static_cast<size_t>(index);
}

void insertSelectedCharacter() {
  const char selectedChar = kPasswordAlphabet[selectedCharIndex];
  passwordBuffer = passwordBuffer.substring(0, passwordCursor) + String(selectedChar) + passwordBuffer.substring(passwordCursor);
  ++passwordCursor;
}

void deleteCharacterAtCursor() {
  if (passwordBuffer.isEmpty()) {
    return;
  }

  if (passwordCursor >= passwordBuffer.length()) {
    passwordBuffer.remove(passwordBuffer.length() - 1, 1);
    if (passwordCursor > 0) {
      --passwordCursor;
    }
    return;
  }

  passwordBuffer.remove(passwordCursor, 1);
}

bool saveWifiCredentials(const String& ssid, const String& password) {
  if (ssid.isEmpty()) {
    return false;
  }

  if (!preferences.begin(kWifiPrefsNamespace, false)) {
    return false;
  }

  const bool ok = preferences.putString(kWifiSsidKey, ssid) > 0 &&
                  preferences.putString(kWifiPasswordKey, password) >= 0;
  preferences.end();
  return ok;
}

void ensureOtaStarted() {
  if (otaStarted || WiFi.status() != WL_CONNECTED) {
    return;
  }

  ArduinoOTA.setHostname("darkroom-controler");
  ArduinoOTA.onStart([]() {
    Serial.println("OTA start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("OTA end");
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA error %u\n", static_cast<unsigned>(error));
  });
  ArduinoOTA.begin();
  otaStarted = true;
  Serial.println("OTA ready");
}

void beginWifiConnectWithEnteredCredentials() {
  connectionStatusMessage = "Saving credentials";
  uiMode = UiMode::WifiConnecting;
  drawWifiConnectingUi();

  if (!saveWifiCredentials(selectedSsid, passwordBuffer)) {
    uiMode = UiMode::WifiPasswordEntry;
    drawPasswordEntryUi();
    drawFooterLine("Save failed", TFT_RED);
    return;
  }

  wifiSsid = selectedSsid;
  wifiPassword = passwordBuffer;
  wifiCredentialsValid = true;
  wifiConnectTimedOut = false;

  WiFi.disconnect(true, true);
  delay(100);
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());
  wifiConnectStartedAt = millis();

  while ((millis() - wifiConnectStartedAt) < kWifiConnectTimeoutMs) {
    DarkroomHw::updateBeep();
    connectionStatusMessage = "Trying to connect";
    drawWifiConnectingUi();

    if (WiFi.status() == WL_CONNECTED) {
      uiMode = UiMode::WifiConnected;
      wifiConnectedScreenShown = true;
      ensureOtaStarted();
      drawWifiConnectedUi();
      wifiConnectedShownAt = millis();
      lastWifiStatusRefreshAt = millis();
      return;
    }

    delay(120);
  }

  wifiConnectTimedOut = true;
  uiMode = UiMode::WifiPasswordEntry;
  drawPasswordEntryUi();
  drawFooterLine("Connect failed", TFT_RED);
}

void playStartupSequence() {
  const uint16_t rgb[3][3] = {
      {kLightHeadMax, 0, 0},
      {0, kLightHeadMax, 0},
      {0, 0, kLightHeadMax},
  };
  const uint8_t status[3][3] = {
      {kLedPreviewBrightness, 0, 0},
      {0, kLedPreviewBrightness, 0},
      {0, 0, kLedPreviewBrightness},
  };

  for (size_t i = 0; i < 3; ++i) {
    setPreviewColor(rgb[i][0], rgb[i][1], rgb[i][2], status[i][0], status[i][1], status[i][2]);
    DarkroomHw::beepTone(kStartupTonesHz[i], kStartupToneDurationMs);
    delay(kStartupGapMs);
  }

  setPreviewColor(0, 0, 0, 0, 0, 0);
}

void applyPressedColor(const DarkroomHw::ButtonState& buttons) {
  if (!exposureRunning) {
    setPreviewColor(0, 0, 0, 0, 0, 0);
  }
}

void handleButtonEdges(const DarkroomHw::ButtonState& buttons) {
  if (buttons.lightTimer && !prevLightTimerPressed) {
    DarkroomHw::startBeep(3520, kClickToneDurationMs);
    if (uiMode == UiMode::WifiPasswordEntry) {
      if (passwordCursor < passwordBuffer.length()) {
        ++passwordCursor;
      }
      drawPasswordEntryUi();
    } else if (uiMode == UiMode::ExposureMode && !exposureRunning) {
      startExposure();
    }
  }

  if (buttons.lightOff && !prevLightOffPressed) {
    DarkroomHw::startBeep(3520, kClickToneDurationMs);
    if (uiMode == UiMode::WifiPasswordEntry) {
      deleteCharacterAtCursor();
      drawPasswordEntryUi();
    } else if (uiMode == UiMode::ExposureMode && exposureRunning) {
      stopExposure(false);
    }
  }

  if (buttons.lightOn && !prevLightOnPressed) {
    DarkroomHw::startBeep(3520, kClickToneDurationMs);
    if (uiMode == UiMode::WifiPasswordEntry) {
      if (passwordCursor > 0) {
        --passwordCursor;
      }
      drawPasswordEntryUi();
    }
  }

  if (buttons.developerTimer && !prevDeveloperPressed) {
    DarkroomHw::startBeep(3520, kClickToneDurationMs);
    if (uiMode == UiMode::WifiScanResult) {
      runWifiScanDiagnostic();
    } else if (uiMode == UiMode::WifiPasswordEntry) {
      beginWifiConnectWithEnteredCredentials();
    }
  }

  prevLightTimerPressed = buttons.lightTimer;
  prevLightOffPressed = buttons.lightOff;
  prevLightOnPressed = buttons.lightOn;
  prevDeveloperPressed = buttons.developerTimer;
}

void handleEncoder() {
  const DarkroomHw::EncoderTurn turn = DarkroomHw::consumeEncoderTurn();
  if (turn == DarkroomHw::EncoderTurn::Clockwise) {
    DarkroomHw::startBeep(2794, kClickToneDurationMs);
    if (uiMode == UiMode::WifiScanResult && networkCount > 0) {
      selectedNetworkIndex = (selectedNetworkIndex + 1) % static_cast<int32_t>(networkCount);
      drawWifiScanResultUi();
    } else if (uiMode == UiMode::ConfigMode) {
      configMenuIndex = (configMenuIndex + 1) % static_cast<int32_t>(kConfigMenuEntryCount);
      drawConfigModeUi();
    } else if (uiMode == UiMode::ExposureMode) {
      if (exposureFocus == ExposureFocus::Exposure) {
        if (exposureIndex < static_cast<int32_t>(exposureStepCount) - 1) {
          ++exposureIndex;
        }
      } else if (exposureFocus == ExposureFocus::Contrast) {
        if (contrastValue < 10) {
          ++contrastValue;
        }
      } else if (exposureFocus == ExposureFocus::Aperture) {
        if (apertureValue < 6) {
          ++apertureValue;
        }
      }
      drawExposureModeUi();
    } else if (uiMode == UiMode::WifiPasswordEntry) {
      moveSelectedCharacter(1);
      drawPasswordEntryUi();
    }
  } else if (turn == DarkroomHw::EncoderTurn::CounterClockwise) {
    DarkroomHw::startBeep(2093, kClickToneDurationMs);
    if (uiMode == UiMode::WifiScanResult && networkCount > 0) {
      --selectedNetworkIndex;
      if (selectedNetworkIndex < 0) {
        selectedNetworkIndex = static_cast<int32_t>(networkCount) - 1;
      }
      drawWifiScanResultUi();
    } else if (uiMode == UiMode::ConfigMode) {
      --configMenuIndex;
      if (configMenuIndex < 0) {
        configMenuIndex = static_cast<int32_t>(kConfigMenuEntryCount) - 1;
      }
      drawConfigModeUi();
    } else if (uiMode == UiMode::ExposureMode) {
      if (exposureFocus == ExposureFocus::Exposure) {
        if (exposureIndex > 0) {
          --exposureIndex;
        }
      } else if (exposureFocus == ExposureFocus::Contrast) {
        if (contrastValue > 0) {
          --contrastValue;
        }
      } else if (exposureFocus == ExposureFocus::Aperture) {
        if (apertureValue > 0) {
          --apertureValue;
        }
      }
      drawExposureModeUi();
    } else if (uiMode == UiMode::WifiPasswordEntry) {
      moveSelectedCharacter(-1);
      drawPasswordEntryUi();
    }
  }

  const bool encoderButtonPressed = DarkroomHw::isEncoderButtonPressed();
  if (encoderButtonPressed && !prevEncoderButtonPressed) {
    if (uiMode == UiMode::WifiScanResult && networkCount > 0) {
      DarkroomHw::startBeep(3520, kClickToneDurationMs);
      beginPasswordEntry();
    } else if (uiMode == UiMode::WifiPasswordEntry) {
      DarkroomHw::startBeep(3520, kClickToneDurationMs);
      insertSelectedCharacter();
      drawPasswordEntryUi();
    } else if (uiMode == UiMode::ExposureMode || uiMode == UiMode::ConfigMode) {
      encoderButtonPressedAt = millis();
      encoderLongPressHandled = false;
    }
  }

  if (encoderButtonPressed &&
      !encoderLongPressHandled &&
      (uiMode == UiMode::ExposureMode || uiMode == UiMode::ConfigMode) &&
      (millis() - encoderButtonPressedAt) >= kEncoderLongPressMs) {
    DarkroomHw::startBeep(1760, kClickToneDurationMs);
    encoderLongPressHandled = true;
    if (uiMode == UiMode::ExposureMode) {
      enterConfigMode();
    } else {
      enterExposureMode();
    }
  }

  if (!encoderButtonPressed && prevEncoderButtonPressed) {
    if ((uiMode == UiMode::ExposureMode || uiMode == UiMode::ConfigMode) && !encoderLongPressHandled) {
      DarkroomHw::startBeep(3520, kClickToneDurationMs);
      if (uiMode == UiMode::ExposureMode) {
        if (exposureFocus == ExposureFocus::Exposure) {
          exposureFocus = ExposureFocus::Contrast;
        } else if (exposureFocus == ExposureFocus::Contrast) {
          exposureFocus = ExposureFocus::Aperture;
        } else {
          exposureFocus = ExposureFocus::Exposure;
        }
        drawExposureModeUi();
      } else if (uiMode == UiMode::ConfigMode) {
        drawFooterLine(String(kConfigMenuEntries[configMenuIndex].label) + " later", TFT_YELLOW);
      }
    }

    encoderLongPressHandled = false;
    encoderButtonPressedAt = 0;
  }

  prevEncoderButtonPressed = encoderButtonPressed;
}

}  // namespace

void setup() {
  Serial.begin(115200);

  initializeExposureSteps();
  DarkroomHw::initHardware();
  loadWifiCredentials();
  beginWifiConnectIfNeeded();
  DarkroomHw::setDarkroomRedLight(0);
  DarkroomHw::setButtonsBacklight(0);
  DarkroomHw::setDisplayBacklight(128);
  DarkroomHw::setLightHeadRgb(0, 0, 0);

  statusLed.begin();
  statusLed.setBrightness(DarkroomHw::kStatusLedBrightness);
  setStatusLed(0, 0, 0);

  playStartupSequence();

  hardResetDisplay();
  tft.init();
  tft.setRotation(1);

  drawBootUi();
  updateWifiStatusUi();

  DarkroomHw::updateEncoder();

  if (!wifiCredentialsValid) {
    runWifiScanDiagnostic();
  }
}

void loop() {
  DarkroomHw::updateEncoder();
  DarkroomHw::updateBeep();
  if (otaStarted) {
    ArduinoOTA.handle();
  }

  if (uiMode == UiMode::WifiConnected && wifiConnectedScreenShown && (millis() - wifiConnectedShownAt) >= 2000) {
    wifiConnectedScreenShown = false;
    enterExposureMode();
  }

  if (wifiCredentialsValid && !wifiConnectTimedOut && WiFi.status() != WL_CONNECTED) {
    if ((millis() - wifiConnectStartedAt) >= kWifiConnectTimeoutMs) {
      wifiConnectTimedOut = true;
      runWifiScanDiagnostic();
    } else if ((millis() - lastWifiStatusRefreshAt) >= 250) {
      lastWifiStatusRefreshAt = millis();
      updateWifiStatusUi();
    }
  } else if (wifiCredentialsValid && WiFi.status() == WL_CONNECTED && lastWifiStatusRefreshAt == 0) {
    updateWifiStatusUi();
    lastWifiStatusRefreshAt = millis();
  }

  if (exposureRunning) {
    const uint32_t now = millis();
    if ((now - exposureStartedAt) >= exposureDurationMs) {
      stopExposure(true);
    } else if (uiMode == UiMode::ExposureMode && (now - lastExposureUiRefreshAt) >= 100) {
      lastExposureUiRefreshAt = now;
      drawExposureFooter();
    }
  }

  const DarkroomHw::ButtonState buttons = DarkroomHw::readButtons();
  applyPressedColor(buttons);
  handleButtonEdges(buttons);
  handleEncoder();

  delay(5);
}
