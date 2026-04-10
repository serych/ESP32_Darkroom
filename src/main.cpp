#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <TFT_eSPI.h>

#include "darkroom_hw.h"

namespace {

constexpr uint8_t kLedPreviewBrightness = 64;
constexpr uint16_t kLightHeadMax = 64;
constexpr uint16_t kTftResetPulseMs = 30;
constexpr uint16_t kTftResetSettlingMs = 180;

constexpr uint8_t kBrightnessSteps[] = {255, 128, 64, 32, 16, 8, 4, 2, 1, 0, 1, 2, 4, 8, 16, 32, 64, 128};
constexpr size_t kBrightnessStepCount = sizeof(kBrightnessSteps) / sizeof(kBrightnessSteps[0]);

constexpr uint16_t kStartupTonesHz[] = {523, 659, 784};
constexpr uint32_t kStartupToneDurationMs = 120;
constexpr uint32_t kStartupGapMs = 80;
constexpr uint32_t kClickToneDurationMs = 15;


TFT_eSPI tft = TFT_eSPI();
Adafruit_NeoPixel statusLed(DarkroomHw::kStatusLedCount, DarkroomHw::kStatusLedPin, NEO_GRB + NEO_KHZ800);

int32_t encoderValue = 0;
size_t buttonsBacklightIndex = 0;
size_t displayBacklightIndex = 0;

bool prevLightOnPressed = false;
bool prevLightOffPressed = false;
bool prevLightTimerPressed = false;
bool prevDeveloperPressed = false;
bool prevEncoderButtonPressed = false;

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

void drawStaticUi() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextFont(4);
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.drawString("Testing hardware", 8, 10, 4);

  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Buttons:", 8, 56, 2);
  tft.drawString("Light timer  -> green", 8, 78, 2);
  tft.drawString("Light off    -> red", 8, 94, 2);
  tft.drawString("Light on     -> yellow", 8, 110, 2);
  tft.drawString("Developer    -> blue", 8, 126, 2);

  tft.drawString("Btn backlight:", 8, 158, 2);
  tft.drawString("Disp backlight:", 8, 176, 2);
  tft.drawString("Encoder value:", 8, 194, 2);
  tft.drawString("Last event:", 8, 212, 2);
}

void drawValue(const char* label, int32_t value, int16_t x, int16_t y, uint16_t color = TFT_CYAN) {
  tft.fillRect(x, y, 120, 16, TFT_BLACK);
  tft.setTextFont(2);
  tft.setTextColor(color, TFT_BLACK);
  tft.drawString(String(value), x, y, 2);
}

void drawTextValue(const char* text, int16_t x, int16_t y, uint16_t color = TFT_GREEN) {
  tft.fillRect(x, y, 150, 16, TFT_BLACK);
  tft.setTextFont(2);
  tft.setTextColor(color, TFT_BLACK);
  tft.drawString(text, x, y, 2);
}

void refreshDynamicUi(const char* eventText = nullptr) {
  drawValue("buttons", kBrightnessSteps[buttonsBacklightIndex], 150, 158);
  drawValue("display", kBrightnessSteps[displayBacklightIndex], 158, 176);
  drawValue("encoder", encoderValue, 136, 194, TFT_YELLOW);
  if (eventText != nullptr) {
    drawTextValue(eventText, 92, 212);
  }
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

void advanceButtonsBacklight() {
  buttonsBacklightIndex = (buttonsBacklightIndex + 1) % kBrightnessStepCount;
  DarkroomHw::setButtonsBacklight(kBrightnessSteps[buttonsBacklightIndex]);
}

void advanceDisplayBacklight() {
  displayBacklightIndex = (displayBacklightIndex + 1) % kBrightnessStepCount;
  DarkroomHw::setDisplayBacklight(kBrightnessSteps[displayBacklightIndex]);
}

void applyPressedColor(const DarkroomHw::ButtonState& buttons) {
  if (buttons.lightTimer) {
    setPreviewColor(0, kLightHeadMax, 0, 0, kLedPreviewBrightness, 0);
    return;
  }

  if (buttons.lightOff) {
    setPreviewColor(kLightHeadMax, 0, 0, kLedPreviewBrightness, 0, 0);
    return;
  }

  if (buttons.lightOn) {
    setPreviewColor(kLightHeadMax, kLightHeadMax, 0, kLedPreviewBrightness, kLedPreviewBrightness, 0);
    return;
  }

  if (buttons.developerTimer) {
    setPreviewColor(0, 0, kLightHeadMax, 0, 0, kLedPreviewBrightness);
    return;
  }

  setPreviewColor(0, 0, 0, 0, 0, 0);
}

void handleButtonEdges(const DarkroomHw::ButtonState& buttons) {
  if (buttons.lightTimer && !prevLightTimerPressed) {
    DarkroomHw::startBeep(3520, kClickToneDurationMs);
    refreshDynamicUi("Light timer");
  }

  if (buttons.lightOff && !prevLightOffPressed) {
    DarkroomHw::startBeep(3520, kClickToneDurationMs);
    refreshDynamicUi("Light off");
  }

  if (buttons.lightOn && !prevLightOnPressed) {
    advanceButtonsBacklight();
    DarkroomHw::startBeep(3520, kClickToneDurationMs);
    refreshDynamicUi("Light on");
  }

  if (buttons.developerTimer && !prevDeveloperPressed) {
    DarkroomHw::startBeep(3520, kClickToneDurationMs);
    refreshDynamicUi("Developer");
  }

  prevLightTimerPressed = buttons.lightTimer;
  prevLightOffPressed = buttons.lightOff;
  prevLightOnPressed = buttons.lightOn;
  prevDeveloperPressed = buttons.developerTimer;
}

void handleEncoder() {
  const DarkroomHw::EncoderTurn turn = DarkroomHw::consumeEncoderTurn();
  if (turn == DarkroomHw::EncoderTurn::Clockwise) {
    ++encoderValue;
    DarkroomHw::startBeep(2794, kClickToneDurationMs);
    refreshDynamicUi("Encoder +");
  } else if (turn == DarkroomHw::EncoderTurn::CounterClockwise) {
    --encoderValue;
    DarkroomHw::startBeep(2093, kClickToneDurationMs);
    refreshDynamicUi("Encoder -");
  }

  const bool encoderButtonPressed = DarkroomHw::isEncoderButtonPressed();
  if (encoderButtonPressed && !prevEncoderButtonPressed) {
    advanceDisplayBacklight();
    DarkroomHw::startBeep(3520, kClickToneDurationMs);
    refreshDynamicUi("Encoder btn");
  }

  prevEncoderButtonPressed = encoderButtonPressed;
}

}  // namespace

void setup() {
  Serial.begin(115200);

  DarkroomHw::initHardware();
  DarkroomHw::setDarkroomRedLight(0);
  DarkroomHw::setButtonsBacklight(kBrightnessSteps[buttonsBacklightIndex]);
  DarkroomHw::setDisplayBacklight(kBrightnessSteps[displayBacklightIndex]);
  DarkroomHw::setLightHeadRgb(0, 0, 0);

  statusLed.begin();
  statusLed.setBrightness(DarkroomHw::kStatusLedBrightness);
  setStatusLed(0, 0, 0);

  playStartupSequence();

  hardResetDisplay();
  tft.init();
  tft.setRotation(1);

  drawStaticUi();
  refreshDynamicUi("Ready");

  DarkroomHw::updateEncoder();
}

void loop() {
  DarkroomHw::updateEncoder();
  DarkroomHw::updateBeep();

  const DarkroomHw::ButtonState buttons = DarkroomHw::readButtons();
  applyPressedColor(buttons);
  handleButtonEdges(buttons);
  handleEncoder();

  delay(5);
}
