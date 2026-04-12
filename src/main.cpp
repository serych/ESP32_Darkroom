#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <TFT_eSPI.h>
#include <Preferences.h>
#include <WiFi.h>
#include <ArduinoOTA.h>

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
};

struct NetworkEntry {
  String ssid;
  int32_t rssi;
  wifi_auth_mode_t authMode;
};


TFT_eSPI tft = TFT_eSPI();
Adafruit_NeoPixel statusLed(DarkroomHw::kStatusLedCount, DarkroomHw::kStatusLedPin, NEO_GRB + NEO_KHZ800);
Preferences preferences;

bool prevLightOnPressed = false;
bool prevLightOffPressed = false;
bool prevLightTimerPressed = false;
bool prevDeveloperPressed = false;
bool prevEncoderButtonPressed = false;

UiMode uiMode = UiMode::BootConnect;
String wifiSsid;
String wifiPassword;
bool wifiCredentialsValid = false;
bool wifiConnectTimedOut = false;
uint32_t wifiConnectStartedAt = 0;
uint32_t lastWifiStatusRefreshAt = 0;
bool wifiConnectedScreenShown = false;
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

void drawWifiConnectedUi();
void ensureOtaStarted();

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
  setPreviewColor(0, 0, 0, 0, 0, 0);
}

void handleButtonEdges(const DarkroomHw::ButtonState& buttons) {
  if (buttons.lightTimer && !prevLightTimerPressed) {
    DarkroomHw::startBeep(3520, kClickToneDurationMs);
    if (uiMode == UiMode::WifiPasswordEntry) {
      if (passwordCursor < passwordBuffer.length()) {
        ++passwordCursor;
      }
      drawPasswordEntryUi();
    }
  }

  if (buttons.lightOff && !prevLightOffPressed) {
    DarkroomHw::startBeep(3520, kClickToneDurationMs);
    if (uiMode == UiMode::WifiPasswordEntry) {
      deleteCharacterAtCursor();
      drawPasswordEntryUi();
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
    } else if (uiMode == UiMode::WifiPasswordEntry) {
      moveSelectedCharacter(-1);
      drawPasswordEntryUi();
    }
  }

  const bool encoderButtonPressed = DarkroomHw::isEncoderButtonPressed();
  if (encoderButtonPressed && !prevEncoderButtonPressed) {
    DarkroomHw::startBeep(3520, kClickToneDurationMs);
    if (uiMode == UiMode::WifiScanResult && networkCount > 0) {
      beginPasswordEntry();
    } else if (uiMode == UiMode::WifiPasswordEntry) {
      insertSelectedCharacter();
      drawPasswordEntryUi();
    }
  } 

  prevEncoderButtonPressed = encoderButtonPressed;
}

}  // namespace

void setup() {
  Serial.begin(115200);

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

  const DarkroomHw::ButtonState buttons = DarkroomHw::readButtons();
  applyPressedColor(buttons);
  handleButtonEdges(buttons);
  handleEncoder();

  delay(5);
}
