#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_NeoPixel.h>

#include <TFT_eSPI.h>
#include "Free_Fonts.h"
#include "darkroom_hw.h"

namespace {

constexpr uint8_t kDisplayBrightness = 192;
constexpr uint8_t kButtonsBrightness = 64;
constexpr uint8_t kDarkroomRedBrightness = 0;
constexpr uint16_t kLightHeadIdleLevel = 0;
constexpr uint32_t kTestStepIntervalMs = 500;
constexpr uint32_t kTextStepIntervalMs = 40;
constexpr uint16_t kTftResetPulseMs = 30;
constexpr uint16_t kTftResetSettlingMs = 180;
constexpr uint8_t kTextFont = 4;
constexpr int16_t kTextXStep = 2;
constexpr int16_t kTextMarginX = 4;
constexpr int16_t kTextStartY = 56;
constexpr char kScrollText[] = "Darkroom display test";

TFT_eSPI tft = TFT_eSPI();
Adafruit_NeoPixel statusLed(DarkroomHw::kStatusLedCount, DarkroomHw::kStatusLedPin, NEO_GRB + NEO_KHZ800);

void setStatusLed(uint8_t red, uint8_t green, uint8_t blue) {
  statusLed.setPixelColor(0, statusLed.Color(red, green, blue));
  statusLed.show();
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

void drawRollingText() {
  static bool initialized = false;
  static uint32_t lastTextStepAt = 0;
  static int16_t currentX = kTextMarginX;
  static int16_t currentRow = 0;
  static int16_t lineHeight = 0;
  static int16_t maxRows = 0;
  static int16_t textWidth = 0;
  static uint16_t textColor = 0;

  if (!initialized) {
    tft.setTextFont(kTextFont);
    tft.setTextSize(1);
    textColor = TFT_RED;
    lineHeight = tft.fontHeight(kTextFont) + 4;
    maxRows = (tft.height() - kTextStartY - 2) / lineHeight;
    textWidth = tft.textWidth(kScrollText, kTextFont);
    initialized = true;
  }

  const uint32_t now = millis();
  if (now - lastTextStepAt < kTextStepIntervalMs) {
    return;
  }
  lastTextStepAt = now;

  const int16_t y = kTextStartY + (currentRow * lineHeight);
  tft.fillRect(1, y, tft.width() - 2, lineHeight, TFT_BLACK);
  tft.setTextFont(kTextFont);
  tft.setTextSize(1);
  tft.setTextColor(textColor, TFT_BLACK);
  tft.drawString(kScrollText, currentX, y, kTextFont);

  currentX += kTextXStep;
  if (currentX > (tft.width() - textWidth - kTextMarginX)) {
    currentX = kTextMarginX;
    currentRow = (currentRow + 1) % maxRows;
    const int16_t nextY = kTextStartY + (currentRow * lineHeight);
    tft.fillRect(1, nextY, tft.width() - 2, lineHeight, TFT_BLACK);
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);

  DarkroomHw::initHardware();
  DarkroomHw::setDisplayBacklight(kDisplayBrightness);
  DarkroomHw::setButtonsBacklight(kButtonsBrightness);
  DarkroomHw::setDarkroomRedLight(kDarkroomRedBrightness);
  DarkroomHw::setLightHeadRgb(kLightHeadIdleLevel, kLightHeadIdleLevel, kLightHeadIdleLevel);

  statusLed.begin();
  statusLed.setBrightness(DarkroomHw::kStatusLedBrightness);
  setStatusLed(0, 0, 0);

  hardResetDisplay();
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  const uint16_t titleColor = TFT_RED;
  const uint16_t borderColor = TFT_YELLOW;
  const int16_t leftBorderX = 1;
  const int16_t rightBorderX = tft.width() - 1;
  const int16_t borderWidth = rightBorderX - leftBorderX + 1;

  tft.drawFastHLine(leftBorderX, 0, borderWidth, borderColor);
  tft.drawFastHLine(leftBorderX, tft.height() - 1, borderWidth, borderColor);
  tft.drawFastVLine(leftBorderX, 0, tft.height(), borderColor);
  tft.drawFastVLine(rightBorderX, 0, tft.height(), borderColor);

  tft.setTextFont(4);
  tft.setTextSize(1);
  tft.setTextColor(titleColor, TFT_BLACK);
  tft.drawString("Darkroom timer", 8, 12, 4);

  DarkroomHw::updateEncoder();
}

void loop() {
  static uint32_t lastStepAt = 0;
  static uint8_t testStep = 0;
  static bool beepEnabled = true;

  DarkroomHw::updateEncoder();
  drawRollingText();

  const uint32_t now = millis();
  if (now - lastStepAt < kTestStepIntervalMs) {
    delay(10);
    return;
  }

  lastStepAt = now;

  switch (testStep) {
    case 0:
      setStatusLed(DarkroomHw::kStatusLedBrightness, 0, 0);
      if (beepEnabled) {
        DarkroomHw::beepTone(880, 120);
      }
      break;
    case 1:
      setStatusLed(0, DarkroomHw::kStatusLedBrightness, 0);
      if (beepEnabled) {
        DarkroomHw::beepTone(1175, 120);
      }
      break;
    case 2:
      setStatusLed(0, 0, DarkroomHw::kStatusLedBrightness);
      if (beepEnabled) {
        DarkroomHw::beepTone(1568, 120);
      }
      break;
    default:
      setStatusLed(0, 0, 0);
      DarkroomHw::stopBeep();
      beepEnabled = false;
      break;
  }

  testStep = (testStep + 1) % 4;
}
