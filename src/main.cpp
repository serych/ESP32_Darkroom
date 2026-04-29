#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_VEML7700.h>
#include <TFT_eSPI.h>
#include <Preferences.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <Wire.h>
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
constexpr uint32_t kDeveloperTimerFinalShortBeepMs = 60;
constexpr uint32_t kDeveloperTimerFinalLongBeepMs = 500;
constexpr uint16_t kDeveloperTimerBeepHz = 2093;
constexpr uint32_t kExposureEndToneDurationMs = 100;
constexpr uint32_t kExposureEndToneGapMs = 50;
constexpr uint32_t kWifiConnectTimeoutMs = 30000;
constexpr uint32_t kI2cClockHz = 50000;
constexpr char kWifiPrefsNamespace[] = "wifi";
constexpr char kWifiSsidKey[] = "ssid";
constexpr char kWifiPasswordKey[] = "password";
constexpr char kConfigPrefsNamespace[] = "config";
constexpr char kConfigRedLightKey[] = "red_light";
constexpr char kConfigButtonsLightKey[] = "btn_light";
constexpr char kConfigDisplayLightKey[] = "disp_light";
constexpr char kConfigExposureKey[] = "exposure";
constexpr char kConfigContrastKey[] = "contrast";
constexpr char kConfigApertureKey[] = "aperture";
constexpr char kConfigCorrectionTableKey[] = "corr_tbl";
constexpr char kConfigContrastRgbKey[] = "rgb_tbl";
constexpr char kConfigWhiteLightKey[] = "white_rgb";
constexpr char kConfigRedHeadKey[] = "red_rgb";
constexpr char kConfigNetworkEnabledKey[] = "net_en";
constexpr char kConfigDevTimerKey[] = "dev_time";
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

struct ContrastRgbSetting {
  uint8_t redStep;
  uint8_t greenStep;
  uint8_t blueStep;
};

enum class LightOutputMode : uint8_t {
  Off,
  White,
  Red,
  Exposure,
};

enum class ConfigMenuScreen : uint8_t {
  Root,
  Lighting,
  Network,
  ContrastExposure,
  ContrastTableEditor,
  WhiteLightEditor,
  RedLightEditor,
};

enum class ContrastEditorField : uint8_t {
  ContrastStep,
  Correction,
  Red,
  Green,
  Blue,
  Back,
};

enum class ColorEditorField : uint8_t {
  Red,
  Green,
  Blue,
  Back,
};


TFT_eSPI tft = TFT_eSPI();
Adafruit_NeoPixel statusLed(DarkroomHw::kStatusLedCount, DarkroomHw::kStatusLedPin, NEO_GRB + NEO_KHZ800);
Preferences preferences;
Adafruit_VEML7700 lightSensor;

bool prevLightOnPressed = false;
bool prevLightOffPressed = false;
bool prevLightTimerPressed = false;
bool prevDeveloperPressed = false;
bool prevEncoderButtonPressed = false;
uint32_t encoderButtonPressedAt = 0;
bool encoderLongPressHandled = false;
bool developerTimerAdjustUsed = false;

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
ConfigMenuScreen configMenuScreen = ConfigMenuScreen::Root;
int32_t configRootIndex = 0;
int32_t configLightingIndex = 0;
bool configEditingValue = false;
constexpr const char* kConfigRootItems[] = {
    "Osvetleni",
    "Kontrast/Expozice",
    "Sit",
    "Zpet",
};
constexpr size_t kConfigRootItemCount = sizeof(kConfigRootItems) / sizeof(kConfigRootItems[0]);
constexpr const char* kLightingItems[] = {
    "Cervene svetlo",
    "Osvetleni tlacitek",
    "Osvetleni displeje",
    "Zpet",
};
constexpr size_t kLightingItemCount = sizeof(kLightingItems) / sizeof(kLightingItems[0]);
constexpr const char* kContrastExposureItems[] = {
    "Barvy + korekce",
    "Bile svetlo",
    "Cervene svetlo",
    "Zpet",
};
constexpr size_t kContrastExposureItemCount = sizeof(kContrastExposureItems) / sizeof(kContrastExposureItems[0]);
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
ContrastRgbSetting whiteLightSetting = {10, 10, 10};
ContrastRgbSetting redLightSetting = {10, 0, 0};
bool exposureRunning = false;
uint32_t exposureStartedAt = 0;
uint32_t exposureDurationMs = 0;
uint32_t lastExposureUiRefreshAt = 0;
uint32_t lastExposureLuxReadAt = 0;
constexpr uint32_t kExposureLuxRefreshMs = 1000;
LightOutputMode lightOutputMode = LightOutputMode::Off;
bool redChordArmed = false;
uint16_t developerTimerSettingSeconds = 90;
bool developerTimerRunning = false;
uint32_t developerTimerStartedAt = 0;
uint32_t developerTimerDurationMs = 0;
int32_t developerTimerLastAnnouncedSecond = -1;
uint8_t darkroomRedLevel = 0;
uint8_t buttonsBacklightLevel = 0;
uint8_t displayBacklightLevel = 6;
bool lightSensorAvailable = false;
bool exposureLuxValid = false;
float exposureLux = 0.0f;
int32_t configContrastExposureIndex = 0;
int32_t configContrastStepIndex = 0;
ContrastEditorField contrastEditorField = ContrastEditorField::ContrastStep;
ColorEditorField colorEditorField = ColorEditorField::Red;
bool networkEnabled = false;
int32_t configNetworkIndex = 0;
constexpr const char* kNetworkItems[] = {
    "Sit povolena",
    "Pripojit ted",
    "Nastavit WiFi",
    "Zpet",
};
constexpr size_t kNetworkItemCount = sizeof(kNetworkItems) / sizeof(kNetworkItems[0]);
bool wifiUiReturnToConfig = false;

void drawWifiConnectedUi();
void drawNetworkDisabledStartupUi();
void drawWifiConnectingUi();
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
void drawDeveloperTimerStatus(uint16_t color);
void playExposureEndTone();
void applyLightOutputMode();
uint8_t pwmFromAuxLevel(uint8_t level);
void applyAuxOutputs();
void drawExposureHeader();
void drawConfigFooter(const String& text, uint16_t color);
void loadConfigValues();
bool saveConfigValues();
void applyConfigPreview();
String formatCorrectionValue(float value);
String formatLuxValue(float lux);
String formatDeveloperTimerSeconds(uint32_t seconds);
void initializeLightSensor();
void refreshExposureLux(bool force);
String formatNetworkStatus();
void startWifiSetupFlow(bool returnToConfig);
void returnFromWifiUi();
void cancelWifiUiToConfig();
void connectStoredWifiFromConfig();
void shutdownWifi();
void runWifiScanDiagnostic();
void startDeveloperTimer();
void stopDeveloperTimer();
void updateDeveloperTimer();

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
  tft.drawString("Darkroom controller", 8, 10, 4);
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

void loadConfigValues() {
  if (!preferences.begin(kConfigPrefsNamespace, true)) {
    return;
  }

  darkroomRedLevel = preferences.getUChar(kConfigRedLightKey, darkroomRedLevel);
  buttonsBacklightLevel = preferences.getUChar(kConfigButtonsLightKey, buttonsBacklightLevel);
  displayBacklightLevel = preferences.getUChar(kConfigDisplayLightKey, displayBacklightLevel);
  exposureIndex = preferences.getInt(kConfigExposureKey, exposureIndex);
  contrastValue = preferences.getInt(kConfigContrastKey, contrastValue);
  apertureValue = preferences.getInt(kConfigApertureKey, apertureValue);
  preferences.getBytes(kConfigCorrectionTableKey, contrastCorrectionTable, sizeof(contrastCorrectionTable));
  preferences.getBytes(kConfigContrastRgbKey, contrastRgbTable, sizeof(contrastRgbTable));
  preferences.getBytes(kConfigWhiteLightKey, &whiteLightSetting, sizeof(whiteLightSetting));
  preferences.getBytes(kConfigRedHeadKey, &redLightSetting, sizeof(redLightSetting));
  networkEnabled = preferences.getBool(kConfigNetworkEnabledKey, networkEnabled);
  developerTimerSettingSeconds = preferences.getUShort(kConfigDevTimerKey, developerTimerSettingSeconds);
  preferences.end();

  if (darkroomRedLevel > 7) {
    darkroomRedLevel = 7;
  }
  if (buttonsBacklightLevel > 7) {
    buttonsBacklightLevel = 7;
  }
  if (displayBacklightLevel > 7) {
    displayBacklightLevel = 7;
  }
  if (exposureIndex < 0) {
    exposureIndex = 0;
  } else if (exposureIndex >= static_cast<int32_t>(exposureStepCount)) {
    exposureIndex = static_cast<int32_t>(exposureStepCount) - 1;
  }
  if (contrastValue < 0) {
    contrastValue = 0;
  } else if (contrastValue > 10) {
    contrastValue = 10;
  }
  if (apertureValue < 0) {
    apertureValue = 0;
  } else if (apertureValue > 6) {
    apertureValue = 6;
  }
  if (developerTimerSettingSeconds < 10) {
    developerTimerSettingSeconds = 10;
  } else if (developerTimerSettingSeconds > 300) {
    developerTimerSettingSeconds = 300;
  }

  for (size_t i = 0; i < 11; ++i) {
    if (contrastCorrectionTable[i] < 0.0f) {
      contrastCorrectionTable[i] = 0.0f;
    } else if (contrastCorrectionTable[i] > 4.0f) {
      contrastCorrectionTable[i] = 4.0f;
    }

    if (contrastRgbTable[i].redStep > 10) {
      contrastRgbTable[i].redStep = 10;
    }
    if (contrastRgbTable[i].greenStep > 10) {
      contrastRgbTable[i].greenStep = 10;
    }
    if (contrastRgbTable[i].blueStep > 10) {
      contrastRgbTable[i].blueStep = 10;
    }
  }

  if (whiteLightSetting.redStep > 10) {
    whiteLightSetting.redStep = 10;
  }
  if (whiteLightSetting.greenStep > 10) {
    whiteLightSetting.greenStep = 10;
  }
  if (whiteLightSetting.blueStep > 10) {
    whiteLightSetting.blueStep = 10;
  }
  if (redLightSetting.redStep > 10) {
    redLightSetting.redStep = 10;
  }
  if (redLightSetting.greenStep > 10) {
    redLightSetting.greenStep = 10;
  }
  if (redLightSetting.blueStep > 10) {
    redLightSetting.blueStep = 10;
  }
}

bool saveConfigValues() {
  if (!preferences.begin(kConfigPrefsNamespace, false)) {
    return false;
  }

  const bool ok = preferences.putUChar(kConfigRedLightKey, darkroomRedLevel) > 0 &&
                  preferences.putUChar(kConfigButtonsLightKey, buttonsBacklightLevel) > 0 &&
                  preferences.putUChar(kConfigDisplayLightKey, displayBacklightLevel) > 0 &&
                  preferences.putInt(kConfigExposureKey, exposureIndex) > 0 &&
                  preferences.putInt(kConfigContrastKey, contrastValue) > 0 &&
                  preferences.putInt(kConfigApertureKey, apertureValue) > 0 &&
                  preferences.putBytes(kConfigCorrectionTableKey, contrastCorrectionTable, sizeof(contrastCorrectionTable)) == sizeof(contrastCorrectionTable) &&
                  preferences.putBytes(kConfigContrastRgbKey, contrastRgbTable, sizeof(contrastRgbTable)) == sizeof(contrastRgbTable) &&
                  preferences.putBytes(kConfigWhiteLightKey, &whiteLightSetting, sizeof(whiteLightSetting)) == sizeof(whiteLightSetting) &&
                  preferences.putBytes(kConfigRedHeadKey, &redLightSetting, sizeof(redLightSetting)) == sizeof(redLightSetting) &&
                  preferences.putBool(kConfigNetworkEnabledKey, networkEnabled) &&
                  preferences.putUShort(kConfigDevTimerKey, developerTimerSettingSeconds) > 0;
  preferences.end();
  return ok;
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
  if (!networkEnabled || !wifiCredentialsValid) {
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

void shutdownWifi() {
  wifiConnectTimedOut = false;
  wifiConnectedScreenShown = false;
  lastWifiStatusRefreshAt = 0;
  WiFi.disconnect(true, true);
  delay(100);
  WiFi.mode(WIFI_OFF);
}

void updateWifiStatusUi() {
  if (!networkEnabled) {
    wifiConnectedScreenShown = false;
    drawWifiStatusLine("WiFi vypnuta", TFT_DARKGREY);
    return;
  }

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
  tft.drawString("Darkroom controller", 8, 10, 4);

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

  drawFooterLine(wifiUiReturnToConfig ? "Rotary=SSID  Long=zpet" : "Rotary = next/prev SSID", TFT_GREEN);
}

void drawPasswordEntryUi() {
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextFont(4);
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.drawString("Darkroom controller", 8, 10, 4);

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
  drawFooterLine(wifiUiReturnToConfig ? "Password entry  Long=zpet" : "Password entry", TFT_GREEN);
}

void drawWifiConnectingUi() {
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextFont(4);
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.drawString("Darkroom controller", 8, 10, 4);

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
  tft.drawString("Darkroom controller", 8, 10, 4);

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

void drawNetworkDisabledStartupUi() {
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextFont(4);
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.drawString("Darkroom controller", 8, 10, 4);

  tft.setTextFont(4);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("WiFi disabled", 8, 48, 4);

  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Sit vypnuta", 8, 120, 2);
  drawFooterLine("Starting exposure mode", TFT_DARKGREY);
}

void drawExposureModeUi() {
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextFont(4);
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.drawString("Darkroom controller", 8, 10, 4);

  drawExposureHeader();

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

void drawExposureHeader() {
  tft.setRotation(1);
  tft.fillRect(0, 44, tft.width(), 28, TFT_BLACK);
  tft.setTextFont(4);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString("Expozice", 8, 48, 4);

  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  if (lightSensorAvailable) {
    tft.drawRightString(exposureLuxValid ? formatLuxValue(exposureLux) : "...", tft.width() - 8, 54, 2);
  }
}

void drawExposureFooter() {
  tft.setRotation(1);
  tft.fillRect(0, 208, tft.width(), 32, TFT_BLACK);
  tft.setTextFont(4);
  if (exposureRunning) {
    const uint32_t elapsedMs = millis() - exposureStartedAt;
    const uint32_t remainingMs = (elapsedMs >= exposureDurationMs) ? 0 : (exposureDurationMs - elapsedMs);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString("Zbyva " + formatExposureSeconds(static_cast<float>(remainingMs) / 1000.0f), 8, 212, 4);
  } else {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString("Timer start", 8, 212, 4);
  }

  drawDeveloperTimerStatus(TFT_CYAN);
}

void drawDeveloperTimerStatus(uint16_t color) {
  uint32_t seconds = developerTimerSettingSeconds;
  bool blinkInvert = false;
  if (developerTimerRunning) {
    const uint32_t elapsedMs = millis() - developerTimerStartedAt;
    if (elapsedMs >= developerTimerDurationMs) {
      seconds = 0;
    } else {
      seconds = (developerTimerDurationMs - elapsedMs + 999U) / 1000U;
      blinkInvert = seconds <= 10 && ((millis() / 250U) % 2U == 0U);
    }
  }

  tft.setTextFont(4);
  const uint16_t background = blinkInvert ? color : TFT_BLACK;
  const uint16_t foreground = blinkInvert ? TFT_BLACK : color;
  tft.fillRect(118, 208, tft.width() - 118, 32, background);
  tft.setTextColor(foreground, background);
  tft.drawRightString(formatDeveloperTimerSeconds(seconds), tft.width() - 8, 212, 4);
}

void playExposureEndTone() {
  DarkroomHw::beepTone(3520, kExposureEndToneDurationMs);
  delay(kExposureEndToneGapMs);
  DarkroomHw::beepTone(3520, kExposureEndToneDurationMs);
}

void drawConfigFooter(const String& text, uint16_t color) {
  tft.setRotation(1);
  tft.fillRect(0, 208, tft.width(), 32, TFT_BLACK);
  tft.setTextFont(2);
  tft.setTextColor(color, TFT_BLACK);
  tft.drawString(text, 8, 216, 2);
}

void drawConfigModeUi() {
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextFont(4);
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.drawString("Darkroom controller", 8, 10, 4);

  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("Konfigurace", 8, 48, 4);

  tft.setTextFont(2);
  if (configMenuScreen == ConfigMenuScreen::Root) {
    for (size_t i = 0; i < kConfigRootItemCount; ++i) {
      const uint16_t background = (static_cast<int32_t>(i) == configRootIndex) ? TFT_DARKGREY : TFT_BLACK;
      const uint16_t color = (static_cast<int32_t>(i) == configRootIndex) ? TFT_YELLOW : TFT_WHITE;
      const int16_t y = 96 + static_cast<int16_t>(i) * 28;
      tft.fillRect(0, y - 2, tft.width(), 24, background);
      tft.setTextColor(color, background);
      tft.drawString(kConfigRootItems[i], 8, y, 2);
    }
    drawConfigFooter("Rotary=vyber  Short=otevri  Long=expozice", TFT_GREEN);
  } else if (configMenuScreen == ConfigMenuScreen::Lighting) {
    for (size_t i = 0; i < kLightingItemCount; ++i) {
      const uint16_t background = (static_cast<int32_t>(i) == configLightingIndex) ? TFT_DARKGREY : TFT_BLACK;
      const uint16_t color = (static_cast<int32_t>(i) == configLightingIndex) ? TFT_YELLOW : TFT_WHITE;
      const int16_t y = 96 + static_cast<int16_t>(i) * 28;
      tft.fillRect(0, y - 2, tft.width(), 24, background);
      tft.setTextColor(color, background);
      tft.drawString(kLightingItems[i], 8, y, 2);

      if (i == 0) {
        tft.drawRightString(String(darkroomRedLevel), tft.width() - 8, y, 2);
      } else if (i == 1) {
        tft.drawRightString(String(buttonsBacklightLevel), tft.width() - 8, y, 2);
      } else if (i == 2) {
        tft.drawRightString(String(displayBacklightLevel), tft.width() - 8, y, 2);
      }
    }

    if (configEditingValue) {
      drawConfigFooter("Rotary=hodnota  Short=uloz", TFT_YELLOW);
    } else {
      drawConfigFooter("Rotary=vyber  Short=uprav/zpet", TFT_GREEN);
    }
  } else if (configMenuScreen == ConfigMenuScreen::Network) {
    for (size_t i = 0; i < kNetworkItemCount; ++i) {
      const uint16_t background = (static_cast<int32_t>(i) == configNetworkIndex) ? TFT_DARKGREY : TFT_BLACK;
      const uint16_t color = (static_cast<int32_t>(i) == configNetworkIndex) ? TFT_YELLOW : TFT_WHITE;
      const int16_t y = 96 + static_cast<int16_t>(i) * 24;
      tft.fillRect(0, y - 2, tft.width(), 22, background);
      tft.setTextColor(color, background);
      tft.drawString(kNetworkItems[i], 8, y, 2);

      if (i == 0) {
        tft.drawRightString(networkEnabled ? "[x]" : "[ ]", tft.width() - 8, y, 2);
      }
    }

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Stav: " + formatNetworkStatus(), 8, 74, 2);
    String ssidText = wifiSsid.isEmpty() ? "-" : wifiSsid.substring(0, 20);
    if (WiFi.status() == WL_CONNECTED && !WiFi.SSID().isEmpty()) {
      ssidText = WiFi.SSID().substring(0, 20);
    }
    tft.drawString("SSID: " + ssidText, 8, 86, 2);
    drawConfigFooter("Short=akce  Long=expozice", TFT_GREEN);
  } else if (configMenuScreen == ConfigMenuScreen::ContrastExposure) {
    for (size_t i = 0; i < kContrastExposureItemCount; ++i) {
      const uint16_t background = (static_cast<int32_t>(i) == configContrastExposureIndex) ? TFT_DARKGREY : TFT_BLACK;
      const uint16_t color = (static_cast<int32_t>(i) == configContrastExposureIndex) ? TFT_YELLOW : TFT_WHITE;
      const int16_t y = 96 + static_cast<int16_t>(i) * 28;
      tft.fillRect(0, y - 2, tft.width(), 24, background);
      tft.setTextColor(color, background);
      tft.drawString(kContrastExposureItems[i], 8, y, 2);
    }
    drawConfigFooter("Rotary=vyber  Short=otevri/zpet", TFT_GREEN);
  } else if (configMenuScreen == ConfigMenuScreen::ContrastTableEditor) {
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Barvy + korekce", 8, 86, 2);

    const uint16_t contrastBackground = (contrastEditorField == ContrastEditorField::ContrastStep) ? TFT_DARKGREY : TFT_BLACK;
    const uint16_t correctionBackground = (contrastEditorField == ContrastEditorField::Correction) ? TFT_DARKGREY : TFT_BLACK;
    const uint16_t redBackground = (contrastEditorField == ContrastEditorField::Red) ? TFT_DARKGREY : TFT_BLACK;
    const uint16_t greenBackground = (contrastEditorField == ContrastEditorField::Green) ? TFT_DARKGREY : TFT_BLACK;
    const uint16_t blueBackground = (contrastEditorField == ContrastEditorField::Blue) ? TFT_DARKGREY : TFT_BLACK;
    const uint16_t backBackground = (contrastEditorField == ContrastEditorField::Back) ? TFT_DARKGREY : TFT_BLACK;

    tft.fillRect(0, 108, tft.width(), 24, contrastBackground);
    tft.setTextColor((contrastEditorField == ContrastEditorField::ContrastStep) ? TFT_YELLOW : TFT_WHITE, contrastBackground);
    tft.drawString("Kontrast:", 8, 112, 2);
    tft.drawRightString(String(configContrastStepIndex), tft.width() - 8, 112, 2);

    tft.fillRect(0, 134, tft.width(), 24, correctionBackground);
    tft.setTextColor((contrastEditorField == ContrastEditorField::Correction) ? TFT_YELLOW : TFT_WHITE, correctionBackground);
    tft.drawString("Korekce:", 8, 138, 2);
    tft.drawRightString(formatCorrectionValue(contrastCorrectionTable[configContrastStepIndex]), tft.width() - 8, 138, 2);

    tft.fillRect(0, 160, tft.width(), 24, redBackground);
    tft.setTextColor((contrastEditorField == ContrastEditorField::Red) ? TFT_YELLOW : TFT_WHITE, redBackground);
    tft.drawString("R:", 8, 164, 2);
    tft.drawRightString(String(contrastRgbTable[configContrastStepIndex].redStep), 74, 164, 2);

    tft.fillRect(78, 160, 74, 24, greenBackground);
    tft.setTextColor((contrastEditorField == ContrastEditorField::Green) ? TFT_YELLOW : TFT_WHITE, greenBackground);
    tft.drawString("G:", 82, 164, 2);
    tft.drawRightString(String(contrastRgbTable[configContrastStepIndex].greenStep), 148, 164, 2);

    tft.fillRect(156, 160, 84, 24, blueBackground);
    tft.setTextColor((contrastEditorField == ContrastEditorField::Blue) ? TFT_YELLOW : TFT_WHITE, blueBackground);
    tft.drawString("B:", 160, 164, 2);
    tft.drawRightString(String(contrastRgbTable[configContrastStepIndex].blueStep), 236, 164, 2);

    tft.fillRect(0, 188, tft.width(), 24, backBackground);
    tft.setTextColor((contrastEditorField == ContrastEditorField::Back) ? TFT_YELLOW : TFT_WHITE, backBackground);
    tft.drawString("Zpet", 8, 192, 2);
    drawConfigFooter(configEditingValue ? "Rotary=hodnota  Short=uloz" : "Rotary=vyber  Short=uprav/zpet", configEditingValue ? TFT_YELLOW : TFT_GREEN);
  } else if (configMenuScreen == ConfigMenuScreen::WhiteLightEditor || configMenuScreen == ConfigMenuScreen::RedLightEditor) {
    ContrastRgbSetting preview = (configMenuScreen == ConfigMenuScreen::WhiteLightEditor) ? whiteLightSetting : redLightSetting;
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString((configMenuScreen == ConfigMenuScreen::WhiteLightEditor) ? "Bile svetlo" : "Cervene svetlo", 8, 86, 2);

    const uint16_t redBackground = (colorEditorField == ColorEditorField::Red) ? TFT_DARKGREY : TFT_BLACK;
    const uint16_t greenBackground = (colorEditorField == ColorEditorField::Green) ? TFT_DARKGREY : TFT_BLACK;
    const uint16_t blueBackground = (colorEditorField == ColorEditorField::Blue) ? TFT_DARKGREY : TFT_BLACK;
    const uint16_t backBackground = (colorEditorField == ColorEditorField::Back) ? TFT_DARKGREY : TFT_BLACK;

    tft.fillRect(0, 132, 74, 24, redBackground);
    tft.setTextColor((colorEditorField == ColorEditorField::Red) ? TFT_YELLOW : TFT_WHITE, redBackground);
    tft.drawString("R:", 8, 136, 2);
    tft.drawRightString(String(preview.redStep), 70, 136, 2);

    tft.fillRect(78, 132, 74, 24, greenBackground);
    tft.setTextColor((colorEditorField == ColorEditorField::Green) ? TFT_YELLOW : TFT_WHITE, greenBackground);
    tft.drawString("G:", 82, 136, 2);
    tft.drawRightString(String(preview.greenStep), 148, 136, 2);

    tft.fillRect(156, 132, 84, 24, blueBackground);
    tft.setTextColor((colorEditorField == ColorEditorField::Blue) ? TFT_YELLOW : TFT_WHITE, blueBackground);
    tft.drawString("B:", 160, 136, 2);
    tft.drawRightString(String(preview.blueStep), 236, 136, 2);

    tft.fillRect(0, 168, tft.width(), 24, backBackground);
    tft.setTextColor((colorEditorField == ColorEditorField::Back) ? TFT_YELLOW : TFT_WHITE, backBackground);
    tft.drawString("Zpet", 8, 172, 2);
    drawConfigFooter(configEditingValue ? "Rotary=hodnota  Short=uloz" : "Rotary=vyber  Short=uprav/zpet", configEditingValue ? TFT_YELLOW : TFT_GREEN);
  }

  applyConfigPreview();
}

void enterExposureMode() {
  uiMode = UiMode::ExposureMode;
  lastExposureLuxReadAt = 0;
  refreshExposureLux(true);
  drawExposureModeUi();
}

void enterConfigMode() {
  uiMode = UiMode::ConfigMode;
  configMenuScreen = ConfigMenuScreen::Root;
  configEditingValue = false;
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

  if (step >= 10) {
    return kLightHeadMax;
  }

  return static_cast<uint16_t>(1U << step);
}

uint8_t pwmFromAuxLevel(uint8_t level) {
  if (level >= 7) {
    return 255;
  }

  return static_cast<uint8_t>((1U << (level + 1)) - 1U);
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

void applyAuxOutputs() {
  DarkroomHw::setDarkroomRedLight(pwmFromAuxLevel(darkroomRedLevel));
  DarkroomHw::setButtonsBacklight(pwmFromAuxLevel(buttonsBacklightLevel));
  DarkroomHw::setDisplayBacklight(pwmFromAuxLevel(displayBacklightLevel));
}

String formatCorrectionValue(float value) {
  return String(value, 2);
}

String formatLuxValue(float lux) {
  if (lux < 10.0f) {
    return String(lux, 2) + " lx";
  }

  if (lux < 100.0f) {
    return String(lux, 1) + " lx";
  }

  return String(static_cast<uint32_t>(lux + 0.5f)) + " lx";
}

String formatDeveloperTimerSeconds(uint32_t seconds) {
  return "DEV: " + String(seconds) + " s";
}

void startDeveloperTimer() {
  developerTimerDurationMs = static_cast<uint32_t>(developerTimerSettingSeconds) * 1000U;
  developerTimerStartedAt = millis();
  developerTimerRunning = true;
  developerTimerLastAnnouncedSecond = -1;
  drawExposureFooter();
}

void stopDeveloperTimer() {
  developerTimerRunning = false;
  developerTimerDurationMs = 0;
  developerTimerLastAnnouncedSecond = -1;
  drawExposureFooter();
}

void updateDeveloperTimer() {
  if (!developerTimerRunning) {
    return;
  }

  const uint32_t elapsedMs = millis() - developerTimerStartedAt;
  if (elapsedMs >= developerTimerDurationMs) {
    developerTimerRunning = false;
    developerTimerDurationMs = 0;
    developerTimerLastAnnouncedSecond = -1;
    drawExposureFooter();
    return;
  }

  const uint32_t remainingMs = developerTimerDurationMs - elapsedMs;
  const int32_t remainingSeconds = static_cast<int32_t>((remainingMs + 999U) / 1000U);
  if (remainingSeconds <= 10 && remainingSeconds >= 1 && remainingSeconds != developerTimerLastAnnouncedSecond) {
    developerTimerLastAnnouncedSecond = remainingSeconds;
    const uint32_t durationMs = (remainingSeconds == 1) ? kDeveloperTimerFinalLongBeepMs : kDeveloperTimerFinalShortBeepMs;
    DarkroomHw::startBeep(kDeveloperTimerBeepHz, durationMs);
  }
}

void initializeLightSensor() {
  Wire.begin(DarkroomHw::PinAssignment::kI2cSda, DarkroomHw::PinAssignment::kI2cScl);
  Wire.setClock(kI2cClockHz);
  lightSensorAvailable = lightSensor.begin(&Wire);
  if (!lightSensorAvailable) {
    Serial.println("VEML7700 init failed");
    return;
  }

  lightSensor.setIntegrationTime(VEML7700_IT_100MS);
  lightSensor.setGain(VEML7700_GAIN_1);
  Serial.println("VEML7700 ready");
}

void refreshExposureLux(bool force) {
  if (uiMode != UiMode::ExposureMode) {
    return;
  }

  const uint32_t now = millis();
  if (!force && (now - lastExposureLuxReadAt) < kExposureLuxRefreshMs) {
    return;
  }

  lastExposureLuxReadAt = now;
  if (!lightSensorAvailable) {
    exposureLuxValid = false;
  } else {
    exposureLux = lightSensor.readLux(VEML_LUX_AUTO);
    exposureLuxValid = isfinite(exposureLux);
  }

  drawExposureHeader();
}

String formatNetworkStatus() {
  if (!networkEnabled) {
    return "vypnuto";
  }

  if (WiFi.status() == WL_CONNECTED) {
    return "pripojeno";
  }

  if (!wifiCredentialsValid) {
    return "bez udaju";
  }

  if (wifiConnectTimedOut) {
    return "chyba spojeni";
  }

  return "pripraveno";
}

void startWifiSetupFlow(bool returnToConfig) {
  wifiUiReturnToConfig = returnToConfig;
  runWifiScanDiagnostic();
}

void returnFromWifiUi() {
  wifiConnectedScreenShown = false;
  if (wifiUiReturnToConfig) {
    wifiUiReturnToConfig = false;
    uiMode = UiMode::ConfigMode;
    configMenuScreen = ConfigMenuScreen::Network;
    configEditingValue = false;
    drawConfigModeUi();
    return;
  }

  enterExposureMode();
}

void cancelWifiUiToConfig() {
  wifiUiReturnToConfig = false;
  uiMode = UiMode::ConfigMode;
  configMenuScreen = ConfigMenuScreen::Network;
  configEditingValue = false;
  drawConfigModeUi();
}

void connectStoredWifiFromConfig() {
  if (!networkEnabled) {
    uiMode = UiMode::ConfigMode;
    configMenuScreen = ConfigMenuScreen::Network;
    drawConfigModeUi();
    drawConfigFooter("Sit je vypnuta", TFT_YELLOW);
    return;
  }

  if (!wifiCredentialsValid) {
    uiMode = UiMode::ConfigMode;
    configMenuScreen = ConfigMenuScreen::Network;
    drawConfigModeUi();
    drawConfigFooter("Neni ulozene SSID", TFT_YELLOW);
    return;
  }

  wifiUiReturnToConfig = true;
  connectionStatusMessage = "Trying to connect";
  uiMode = UiMode::WifiConnecting;
  drawWifiConnectingUi();

  WiFi.disconnect(true, true);
  delay(100);
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());
  wifiConnectStartedAt = millis();
  wifiConnectTimedOut = false;

  while ((millis() - wifiConnectStartedAt) < kWifiConnectTimeoutMs) {
    DarkroomHw::updateBeep();
    drawWifiConnectingUi();

    if (WiFi.status() == WL_CONNECTED) {
      uiMode = UiMode::WifiConnected;
      wifiConnectedScreenShown = true;
      selectedSsid = WiFi.SSID();
      ensureOtaStarted();
      drawWifiConnectedUi();
      wifiConnectedShownAt = millis();
      lastWifiStatusRefreshAt = millis();
      return;
    }

    delay(120);
  }

  wifiConnectTimedOut = true;
  cancelWifiUiToConfig();
  drawConfigFooter("Connect failed", TFT_RED);
}

void applyConfigPreview() {
  if (uiMode != UiMode::ConfigMode) {
    return;
  }

  if (configMenuScreen == ConfigMenuScreen::ContrastTableEditor) {
    const ContrastRgbSetting& rgb = contrastRgbTable[configContrastStepIndex];
    const uint16_t red = pwmFromLogStep(rgb.redStep);
    const uint16_t green = pwmFromLogStep(rgb.greenStep);
    const uint16_t blue = pwmFromLogStep(rgb.blueStep);
    const uint8_t statusRed = static_cast<uint8_t>((red * kLedPreviewBrightness) / kLightHeadMax);
    const uint8_t statusGreen = static_cast<uint8_t>((green * kLedPreviewBrightness) / kLightHeadMax);
    const uint8_t statusBlue = static_cast<uint8_t>((blue * kLedPreviewBrightness) / kLightHeadMax);
    setPreviewColor(red, green, blue, statusRed, statusGreen, statusBlue);
    return;
  }

  if (configMenuScreen == ConfigMenuScreen::WhiteLightEditor) {
    const uint16_t red = pwmFromLogStep(whiteLightSetting.redStep);
    const uint16_t green = pwmFromLogStep(whiteLightSetting.greenStep);
    const uint16_t blue = pwmFromLogStep(whiteLightSetting.blueStep);
    const uint8_t statusRed = static_cast<uint8_t>((red * kLedPreviewBrightness) / kLightHeadMax);
    const uint8_t statusGreen = static_cast<uint8_t>((green * kLedPreviewBrightness) / kLightHeadMax);
    const uint8_t statusBlue = static_cast<uint8_t>((blue * kLedPreviewBrightness) / kLightHeadMax);
    setPreviewColor(red, green, blue, statusRed, statusGreen, statusBlue);
    return;
  }

  if (configMenuScreen == ConfigMenuScreen::RedLightEditor) {
    const uint16_t red = pwmFromLogStep(redLightSetting.redStep);
    const uint16_t green = pwmFromLogStep(redLightSetting.greenStep);
    const uint16_t blue = pwmFromLogStep(redLightSetting.blueStep);
    const uint8_t statusRed = static_cast<uint8_t>((red * kLedPreviewBrightness) / kLightHeadMax);
    const uint8_t statusGreen = static_cast<uint8_t>((green * kLedPreviewBrightness) / kLightHeadMax);
    const uint8_t statusBlue = static_cast<uint8_t>((blue * kLedPreviewBrightness) / kLightHeadMax);
    setPreviewColor(red, green, blue, statusRed, statusGreen, statusBlue);
    return;
  }

  applyLightOutputMode();
}

void applyExposureLightOutput() {
  if (lightOutputMode != LightOutputMode::Exposure || !exposureRunning) {
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

void applyLightOutputMode() {
  if (lightOutputMode == LightOutputMode::Exposure) {
    applyExposureLightOutput();
    return;
  }

  if (lightOutputMode == LightOutputMode::White) {
    const uint16_t red = pwmFromLogStep(whiteLightSetting.redStep);
    const uint16_t green = pwmFromLogStep(whiteLightSetting.greenStep);
    const uint16_t blue = pwmFromLogStep(whiteLightSetting.blueStep);
    const uint8_t statusRed = static_cast<uint8_t>((red * kLedPreviewBrightness) / kLightHeadMax);
    const uint8_t statusGreen = static_cast<uint8_t>((green * kLedPreviewBrightness) / kLightHeadMax);
    const uint8_t statusBlue = static_cast<uint8_t>((blue * kLedPreviewBrightness) / kLightHeadMax);
    setPreviewColor(red, green, blue, statusRed, statusGreen, statusBlue);
    return;
  }

  if (lightOutputMode == LightOutputMode::Red) {
    const uint16_t red = pwmFromLogStep(redLightSetting.redStep);
    const uint16_t green = pwmFromLogStep(redLightSetting.greenStep);
    const uint16_t blue = pwmFromLogStep(redLightSetting.blueStep);
    const uint8_t statusRed = static_cast<uint8_t>((red * kLedPreviewBrightness) / kLightHeadMax);
    const uint8_t statusGreen = static_cast<uint8_t>((green * kLedPreviewBrightness) / kLightHeadMax);
    const uint8_t statusBlue = static_cast<uint8_t>((blue * kLedPreviewBrightness) / kLightHeadMax);
    setPreviewColor(red, green, blue, statusRed, statusGreen, statusBlue);
    return;
  }

  setPreviewColor(0, 0, 0, 0, 0, 0);
}

void stopExposure(bool completed) {
  exposureRunning = false;
  exposureDurationMs = 0;
  exposureLuxValid = false;
  lightOutputMode = LightOutputMode::Off;
  applyLightOutputMode();
  playExposureEndTone();
  if (uiMode == UiMode::ExposureMode) {
    drawExposureModeUi();
    tft.fillRect(0, 208, tft.width(), 32, TFT_BLACK);
    tft.setTextFont(4);
    tft.setTextColor(completed ? TFT_GREEN : TFT_YELLOW, TFT_BLACK);
    tft.drawString(completed ? "Hotovo" : "Zastaveno", 8, 212, 4);
    drawDeveloperTimerStatus(TFT_CYAN);
  }
}

void startExposure() {
  const float correctedExposure = exposureSeconds[exposureIndex] * contrastCorrectionTable[contrastValue];
  const float clampedExposure = (correctedExposure < 0.1f) ? 0.1f : correctedExposure;
  exposureDurationMs = static_cast<uint32_t>(clampedExposure * 1000.0f + 0.5f);
  exposureStartedAt = millis();
  lastExposureUiRefreshAt = 0;
  lastExposureLuxReadAt = 0;
  exposureRunning = true;
  lightOutputMode = LightOutputMode::Exposure;
  refreshExposureLux(true);
  applyLightOutputMode();
  if (uiMode == UiMode::ExposureMode) {
    drawExposureModeUi();
  }
}

void drawWifiScanUi() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextFont(4);
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.drawString("Darkroom controller", 8, 10, 4);

  tft.setTextFont(2);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("WiFi setup", 8, 48, 2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Scanning WiFi, wait please", 8, 84, 2);
  drawFooterLine(wifiUiReturnToConfig ? "Scanning...  Long=zpet" : "Scanning...", TFT_YELLOW);
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
  (void)buttons;
  applyConfigPreview();
}

void handleButtonEdges(const DarkroomHw::ButtonState& buttons) {
  const bool lightOnReleased = !buttons.lightOn && prevLightOnPressed;
  const bool developerReleased = !buttons.developerTimer && prevDeveloperPressed;

  if (buttons.lightTimer && !prevLightTimerPressed) {
    DarkroomHw::startBeep(3520, kClickToneDurationMs);
    if (uiMode == UiMode::WifiPasswordEntry) {
      if (passwordCursor < passwordBuffer.length()) {
        ++passwordCursor;
      }
      drawPasswordEntryUi();
    } else if (uiMode == UiMode::ExposureMode && !exposureRunning) {
      redChordArmed = false;
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
    } else if (uiMode == UiMode::ExposureMode && lightOutputMode == LightOutputMode::Red) {
      lightOutputMode = LightOutputMode::Off;
      redChordArmed = false;
      applyLightOutputMode();
      drawExposureFooter();
    } else if (uiMode == UiMode::ExposureMode) {
      lightOutputMode = LightOutputMode::Off;
      redChordArmed = false;
      applyLightOutputMode();
      drawExposureFooter();
    }
  }

  if (buttons.lightOn && !prevLightOnPressed) {
    DarkroomHw::startBeep(3520, kClickToneDurationMs);
    if (uiMode == UiMode::WifiPasswordEntry) {
      if (passwordCursor > 0) {
        --passwordCursor;
      }
      drawPasswordEntryUi();
    } else if (uiMode == UiMode::ExposureMode && !exposureRunning && buttons.lightOff) {
      redChordArmed = true;
    } else if (uiMode == UiMode::ExposureMode && !exposureRunning) {
      lightOutputMode = LightOutputMode::White;
      redChordArmed = false;
      applyLightOutputMode();
      drawExposureFooter();
    }
  }

  if (buttons.developerTimer && !prevDeveloperPressed) {
    if (uiMode == UiMode::ExposureMode) {
      developerTimerAdjustUsed = false;
    } else {
      DarkroomHw::startBeep(3520, kClickToneDurationMs);
    }

    if (uiMode == UiMode::WifiScanResult) {
      runWifiScanDiagnostic();
    } else if (uiMode == UiMode::WifiPasswordEntry) {
      beginWifiConnectWithEnteredCredentials();
    }
  }

  if (developerReleased && uiMode == UiMode::ExposureMode) {
    if (!developerTimerAdjustUsed) {
      DarkroomHw::startBeep(3520, kClickToneDurationMs);
      startDeveloperTimer();
    }
    developerTimerAdjustUsed = false;
  }

  if (lightOnReleased && uiMode == UiMode::ExposureMode && !exposureRunning && redChordArmed) {
    lightOutputMode = LightOutputMode::Red;
    redChordArmed = false;
    applyLightOutputMode();
    drawExposureFooter();
  }

  if (!buttons.lightOff && !buttons.lightOn && lightOutputMode != LightOutputMode::Red && redChordArmed) {
    redChordArmed = false;
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
    } else if (uiMode == UiMode::ExposureMode && DarkroomHw::isDeveloperTimerButtonPressed()) {
      developerTimerAdjustUsed = true;
      if (!developerTimerRunning && developerTimerSettingSeconds < 300) {
        ++developerTimerSettingSeconds;
        saveConfigValues();
      }
      drawExposureFooter();
    } else if (uiMode == UiMode::ConfigMode) {
      if (configMenuScreen == ConfigMenuScreen::Root) {
        configRootIndex = (configRootIndex + 1) % static_cast<int32_t>(kConfigRootItemCount);
      } else if (configMenuScreen == ConfigMenuScreen::Lighting && configEditingValue) {
        if (configLightingIndex == 0 && darkroomRedLevel < 7) {
          ++darkroomRedLevel;
          applyAuxOutputs();
          saveConfigValues();
        } else if (configLightingIndex == 1 && buttonsBacklightLevel < 7) {
          ++buttonsBacklightLevel;
          applyAuxOutputs();
          saveConfigValues();
        } else if (configLightingIndex == 2 && displayBacklightLevel < 7) {
          ++displayBacklightLevel;
          applyAuxOutputs();
          saveConfigValues();
        }
      } else if (configMenuScreen == ConfigMenuScreen::Lighting) {
        configLightingIndex = (configLightingIndex + 1) % static_cast<int32_t>(kLightingItemCount);
      } else if (configMenuScreen == ConfigMenuScreen::Network) {
        configNetworkIndex = (configNetworkIndex + 1) % static_cast<int32_t>(kNetworkItemCount);
      } else if (configMenuScreen == ConfigMenuScreen::ContrastExposure) {
        configContrastExposureIndex = (configContrastExposureIndex + 1) % static_cast<int32_t>(kContrastExposureItemCount);
      } else if (configMenuScreen == ConfigMenuScreen::ContrastTableEditor) {
        if (!configEditingValue) {
          contrastEditorField = static_cast<ContrastEditorField>((static_cast<uint8_t>(contrastEditorField) + 1) % 6);
        } else if (contrastEditorField == ContrastEditorField::ContrastStep) {
          configContrastStepIndex = (configContrastStepIndex + 1) % 11;
        } else if (contrastEditorField == ContrastEditorField::Correction) {
          if (contrastCorrectionTable[configContrastStepIndex] < 4.0f) {
            contrastCorrectionTable[configContrastStepIndex] += 0.01f;
            if (contrastCorrectionTable[configContrastStepIndex] > 4.0f) {
              contrastCorrectionTable[configContrastStepIndex] = 4.0f;
            }
          }
        } else if (contrastEditorField == ContrastEditorField::Red) {
          if (contrastRgbTable[configContrastStepIndex].redStep < 10) {
            ++contrastRgbTable[configContrastStepIndex].redStep;
          }
        } else if (contrastEditorField == ContrastEditorField::Green) {
          if (contrastRgbTable[configContrastStepIndex].greenStep < 10) {
            ++contrastRgbTable[configContrastStepIndex].greenStep;
          }
        } else if (contrastEditorField == ContrastEditorField::Blue) {
          if (contrastRgbTable[configContrastStepIndex].blueStep < 10) {
            ++contrastRgbTable[configContrastStepIndex].blueStep;
          }
        }
      } else if (configMenuScreen == ConfigMenuScreen::WhiteLightEditor) {
        if (!configEditingValue) {
          colorEditorField = static_cast<ColorEditorField>((static_cast<uint8_t>(colorEditorField) + 1) % 4);
        } else if (colorEditorField == ColorEditorField::Red && whiteLightSetting.redStep < 10) {
          ++whiteLightSetting.redStep;
        } else if (colorEditorField == ColorEditorField::Green && whiteLightSetting.greenStep < 10) {
          ++whiteLightSetting.greenStep;
        } else if (colorEditorField == ColorEditorField::Blue && whiteLightSetting.blueStep < 10) {
          ++whiteLightSetting.blueStep;
        }
      } else if (configMenuScreen == ConfigMenuScreen::RedLightEditor) {
        if (!configEditingValue) {
          colorEditorField = static_cast<ColorEditorField>((static_cast<uint8_t>(colorEditorField) + 1) % 4);
        } else if (colorEditorField == ColorEditorField::Red && redLightSetting.redStep < 10) {
          ++redLightSetting.redStep;
        } else if (colorEditorField == ColorEditorField::Green && redLightSetting.greenStep < 10) {
          ++redLightSetting.greenStep;
        } else if (colorEditorField == ColorEditorField::Blue && redLightSetting.blueStep < 10) {
          ++redLightSetting.blueStep;
        }
      }
      drawConfigModeUi();
    } else if (uiMode == UiMode::ExposureMode) {
      if (exposureFocus == ExposureFocus::Exposure) {
        if (exposureIndex < static_cast<int32_t>(exposureStepCount) - 1) {
          ++exposureIndex;
          saveConfigValues();
        }
      } else if (exposureFocus == ExposureFocus::Contrast) {
        if (contrastValue < 10) {
          ++contrastValue;
          saveConfigValues();
        }
      } else if (exposureFocus == ExposureFocus::Aperture) {
        if (apertureValue < 6) {
          ++apertureValue;
          saveConfigValues();
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
    } else if (uiMode == UiMode::ExposureMode && DarkroomHw::isDeveloperTimerButtonPressed()) {
      developerTimerAdjustUsed = true;
      if (!developerTimerRunning && developerTimerSettingSeconds > 10) {
        --developerTimerSettingSeconds;
        saveConfigValues();
      }
      drawExposureFooter();
    } else if (uiMode == UiMode::ConfigMode) {
      if (configMenuScreen == ConfigMenuScreen::Root) {
        --configRootIndex;
        if (configRootIndex < 0) {
          configRootIndex = static_cast<int32_t>(kConfigRootItemCount) - 1;
        }
      } else if (configMenuScreen == ConfigMenuScreen::Lighting && configEditingValue) {
        if (configLightingIndex == 0 && darkroomRedLevel > 0) {
          --darkroomRedLevel;
          applyAuxOutputs();
          saveConfigValues();
        } else if (configLightingIndex == 1 && buttonsBacklightLevel > 0) {
          --buttonsBacklightLevel;
          applyAuxOutputs();
          saveConfigValues();
        } else if (configLightingIndex == 2 && displayBacklightLevel > 0) {
          --displayBacklightLevel;
          applyAuxOutputs();
          saveConfigValues();
        }
      } else if (configMenuScreen == ConfigMenuScreen::Lighting) {
        --configLightingIndex;
        if (configLightingIndex < 0) {
          configLightingIndex = static_cast<int32_t>(kLightingItemCount) - 1;
        }
      } else if (configMenuScreen == ConfigMenuScreen::Network) {
        --configNetworkIndex;
        if (configNetworkIndex < 0) {
          configNetworkIndex = static_cast<int32_t>(kNetworkItemCount) - 1;
        }
      } else if (configMenuScreen == ConfigMenuScreen::ContrastExposure) {
        --configContrastExposureIndex;
        if (configContrastExposureIndex < 0) {
          configContrastExposureIndex = static_cast<int32_t>(kContrastExposureItemCount) - 1;
        }
      } else if (configMenuScreen == ConfigMenuScreen::ContrastTableEditor) {
        if (!configEditingValue) {
          int32_t index = static_cast<int32_t>(contrastEditorField) - 1;
          if (index < 0) {
            index = 5;
          }
          contrastEditorField = static_cast<ContrastEditorField>(index);
        } else if (contrastEditorField == ContrastEditorField::ContrastStep) {
          --configContrastStepIndex;
          if (configContrastStepIndex < 0) {
            configContrastStepIndex = 10;
          }
        } else if (contrastEditorField == ContrastEditorField::Correction) {
          if (contrastCorrectionTable[configContrastStepIndex] > 0.0f) {
            contrastCorrectionTable[configContrastStepIndex] -= 0.01f;
            if (contrastCorrectionTable[configContrastStepIndex] < 0.0f) {
              contrastCorrectionTable[configContrastStepIndex] = 0.0f;
            }
          }
        } else if (contrastEditorField == ContrastEditorField::Red) {
          if (contrastRgbTable[configContrastStepIndex].redStep > 0) {
            --contrastRgbTable[configContrastStepIndex].redStep;
          }
        } else if (contrastEditorField == ContrastEditorField::Green) {
          if (contrastRgbTable[configContrastStepIndex].greenStep > 0) {
            --contrastRgbTable[configContrastStepIndex].greenStep;
          }
        } else if (contrastEditorField == ContrastEditorField::Blue) {
          if (contrastRgbTable[configContrastStepIndex].blueStep > 0) {
            --contrastRgbTable[configContrastStepIndex].blueStep;
          }
        }
      } else if (configMenuScreen == ConfigMenuScreen::WhiteLightEditor) {
        if (!configEditingValue) {
          int32_t index = static_cast<int32_t>(colorEditorField) - 1;
          if (index < 0) {
            index = 3;
          }
          colorEditorField = static_cast<ColorEditorField>(index);
        } else if (colorEditorField == ColorEditorField::Red && whiteLightSetting.redStep > 0) {
          --whiteLightSetting.redStep;
        } else if (colorEditorField == ColorEditorField::Green && whiteLightSetting.greenStep > 0) {
          --whiteLightSetting.greenStep;
        } else if (colorEditorField == ColorEditorField::Blue && whiteLightSetting.blueStep > 0) {
          --whiteLightSetting.blueStep;
        }
      } else if (configMenuScreen == ConfigMenuScreen::RedLightEditor) {
        if (!configEditingValue) {
          int32_t index = static_cast<int32_t>(colorEditorField) - 1;
          if (index < 0) {
            index = 3;
          }
          colorEditorField = static_cast<ColorEditorField>(index);
        } else if (colorEditorField == ColorEditorField::Red && redLightSetting.redStep > 0) {
          --redLightSetting.redStep;
        } else if (colorEditorField == ColorEditorField::Green && redLightSetting.greenStep > 0) {
          --redLightSetting.greenStep;
        } else if (colorEditorField == ColorEditorField::Blue && redLightSetting.blueStep > 0) {
          --redLightSetting.blueStep;
        }
      }
      drawConfigModeUi();
    } else if (uiMode == UiMode::ExposureMode) {
      if (exposureFocus == ExposureFocus::Exposure) {
        if (exposureIndex > 0) {
          --exposureIndex;
          saveConfigValues();
        }
      } else if (exposureFocus == ExposureFocus::Contrast) {
        if (contrastValue > 0) {
          --contrastValue;
          saveConfigValues();
        }
      } else if (exposureFocus == ExposureFocus::Aperture) {
        if (apertureValue > 0) {
          --apertureValue;
          saveConfigValues();
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
    } else if (uiMode == UiMode::ExposureMode || uiMode == UiMode::ConfigMode ||
               (wifiUiReturnToConfig &&
                (uiMode == UiMode::WifiScan || uiMode == UiMode::WifiScanResult || uiMode == UiMode::WifiPasswordEntry))) {
      encoderButtonPressedAt = millis();
      encoderLongPressHandled = false;
    }
  }

  if (encoderButtonPressed &&
      !encoderLongPressHandled &&
      (uiMode == UiMode::ExposureMode || uiMode == UiMode::ConfigMode ||
       (wifiUiReturnToConfig &&
        (uiMode == UiMode::WifiScan || uiMode == UiMode::WifiScanResult || uiMode == UiMode::WifiPasswordEntry))) &&
      (millis() - encoderButtonPressedAt) >= kEncoderLongPressMs) {
    DarkroomHw::startBeep(1760, kClickToneDurationMs);
    encoderLongPressHandled = true;
    if (uiMode == UiMode::ExposureMode) {
      enterConfigMode();
    } else if (uiMode == UiMode::ConfigMode) {
      enterExposureMode();
    } else {
      cancelWifiUiToConfig();
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
        if (configMenuScreen == ConfigMenuScreen::Root) {
          if (configRootIndex == 0) {
            configMenuScreen = ConfigMenuScreen::Lighting;
            configEditingValue = false;
            drawConfigModeUi();
          } else if (configRootIndex == 1) {
            configMenuScreen = ConfigMenuScreen::ContrastExposure;
            configEditingValue = false;
            drawConfigModeUi();
          } else if (configRootIndex == 2) {
            configMenuScreen = ConfigMenuScreen::Network;
            configEditingValue = false;
            drawConfigModeUi();
          } else if (configRootIndex == 3) {
            enterExposureMode();
          } else {
            drawConfigFooter(String(kConfigRootItems[configRootIndex]) + " later", TFT_YELLOW);
          }
        } else if (configMenuScreen == ConfigMenuScreen::Lighting) {
          if (configLightingIndex == 3) {
            if (configEditingValue) {
              configEditingValue = false;
            } else {
              configMenuScreen = ConfigMenuScreen::Root;
            }
          } else {
            configEditingValue = !configEditingValue;
          }
          drawConfigModeUi();
        } else if (configMenuScreen == ConfigMenuScreen::Network) {
          if (configNetworkIndex == 0) {
            networkEnabled = !networkEnabled;
            saveConfigValues();
            if (!networkEnabled) {
              shutdownWifi();
            }
            drawConfigModeUi();
          } else if (configNetworkIndex == 1) {
            connectStoredWifiFromConfig();
          } else if (configNetworkIndex == 2) {
            if (!networkEnabled) {
              drawConfigModeUi();
              drawConfigFooter("Nejdriv povolte sit", TFT_YELLOW);
            } else {
              startWifiSetupFlow(true);
            }
          } else {
            configMenuScreen = ConfigMenuScreen::Root;
            drawConfigModeUi();
          }
        } else if (configMenuScreen == ConfigMenuScreen::ContrastExposure) {
          if (configContrastExposureIndex == 0) {
            configMenuScreen = ConfigMenuScreen::ContrastTableEditor;
            contrastEditorField = ContrastEditorField::ContrastStep;
            configEditingValue = false;
          } else if (configContrastExposureIndex == 1) {
            configMenuScreen = ConfigMenuScreen::WhiteLightEditor;
            colorEditorField = ColorEditorField::Red;
            configEditingValue = false;
          } else if (configContrastExposureIndex == 2) {
            configMenuScreen = ConfigMenuScreen::RedLightEditor;
            colorEditorField = ColorEditorField::Red;
            configEditingValue = false;
          } else {
            configMenuScreen = ConfigMenuScreen::Root;
            configEditingValue = false;
          }
          drawConfigModeUi();
        } else if (configMenuScreen == ConfigMenuScreen::ContrastTableEditor) {
          if (contrastEditorField == ContrastEditorField::Back && !configEditingValue) {
            configMenuScreen = ConfigMenuScreen::ContrastExposure;
            configEditingValue = false;
          } else {
            if (configEditingValue) {
              saveConfigValues();
            }
            configEditingValue = !configEditingValue;
          }
          drawConfigModeUi();
        } else if (configMenuScreen == ConfigMenuScreen::WhiteLightEditor || configMenuScreen == ConfigMenuScreen::RedLightEditor) {
          if (colorEditorField == ColorEditorField::Back && !configEditingValue) {
            configMenuScreen = ConfigMenuScreen::ContrastExposure;
            configEditingValue = false;
          } else {
            if (configEditingValue) {
              saveConfigValues();
            }
            configEditingValue = !configEditingValue;
          }
          drawConfigModeUi();
        }
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
  initializeLightSensor();
  loadConfigValues();
  loadWifiCredentials();
  beginWifiConnectIfNeeded();
  applyAuxOutputs();
  DarkroomHw::setLightHeadRgb(0, 0, 0);

  statusLed.begin();
  statusLed.setBrightness(DarkroomHw::kStatusLedBrightness);
  setStatusLed(0, 0, 0);

  playStartupSequence();

  hardResetDisplay();
  tft.init();
  tft.setRotation(1);

  drawBootUi();

  DarkroomHw::updateEncoder();

  if (networkEnabled) {
    updateWifiStatusUi();
    if (!wifiCredentialsValid) {
      startWifiSetupFlow(false);
    }
  } else {
    uiMode = UiMode::WifiConnected;
    wifiConnectedScreenShown = true;
    wifiConnectedShownAt = millis();
    drawNetworkDisabledStartupUi();
  }
}

void loop() {
  DarkroomHw::updateEncoder();
  DarkroomHw::updateBeep();
  if (otaStarted) {
    ArduinoOTA.handle();
  }

  if (uiMode == UiMode::WifiConnected && wifiConnectedScreenShown && (millis() - wifiConnectedShownAt) >= 2000) {
    returnFromWifiUi();
  }

  if (uiMode == UiMode::BootConnect && networkEnabled && wifiCredentialsValid && !wifiConnectTimedOut && WiFi.status() != WL_CONNECTED) {
    if ((millis() - wifiConnectStartedAt) >= kWifiConnectTimeoutMs) {
      wifiConnectTimedOut = true;
      startWifiSetupFlow(false);
    } else if ((millis() - lastWifiStatusRefreshAt) >= 250) {
      lastWifiStatusRefreshAt = millis();
      updateWifiStatusUi();
    }
  } else if (uiMode == UiMode::BootConnect && networkEnabled && wifiCredentialsValid && WiFi.status() == WL_CONNECTED &&
             lastWifiStatusRefreshAt == 0) {
    updateWifiStatusUi();
    lastWifiStatusRefreshAt = millis();
  }

  if (exposureRunning || developerTimerRunning) {
    const uint32_t now = millis();
    if (exposureRunning && (now - exposureStartedAt) >= exposureDurationMs) {
      stopExposure(true);
    } else if (uiMode == UiMode::ExposureMode && (now - lastExposureUiRefreshAt) >= 100) {
      lastExposureUiRefreshAt = now;
      drawExposureFooter();
    }
  }

  if (uiMode == UiMode::ExposureMode) {
    refreshExposureLux(false);
  }

  updateDeveloperTimer();

  const DarkroomHw::ButtonState buttons = DarkroomHw::readButtons();
  applyPressedColor(buttons);
  handleButtonEdges(buttons);
  handleEncoder();

  delay(5);
}
