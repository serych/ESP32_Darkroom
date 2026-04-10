#pragma once

#include <Arduino.h>

namespace DarkroomHw {

static constexpr uint8_t kStatusLedPin = 2;
static constexpr uint8_t kStatusLedCount = 1;
static constexpr uint8_t kStatusLedBrightness = 64;

struct PinAssignment {
  static constexpr uint8_t kTftMiso = 19;
  static constexpr uint8_t kTftMosi = 23;
  static constexpr uint8_t kTftSclk = 18;
  static constexpr uint8_t kTftCs = 15;
  static constexpr uint8_t kTftDc = 16;
  static constexpr uint8_t kTftRst = 4;
  static constexpr uint8_t kDisplayBacklight = 17;

  static constexpr uint8_t kLightHeadRed = 25;
  static constexpr uint8_t kLightHeadGreen = 26;
  static constexpr uint8_t kLightHeadBlue = 27;

  static constexpr uint8_t kDarkroomRed = 13;
  static constexpr uint8_t kButtonsBacklight = 14;
  static constexpr uint8_t kBeeper = 12;

  static constexpr uint8_t kButtonLightOn = 32;
  static constexpr uint8_t kButtonLightOff = 33;
  static constexpr uint8_t kButtonLightTimer = 34;
  static constexpr uint8_t kButtonDeveloperTimer = 35;

  static constexpr uint8_t kEncoderA = 36;
  static constexpr uint8_t kEncoderB = 39;
  static constexpr uint8_t kEncoderButton = 5;

  static constexpr uint8_t kI2cSda = 21;
  static constexpr uint8_t kI2cScl = 22;
};

struct ButtonState {
  bool lightOn;
  bool lightOff;
  bool lightTimer;
  bool developerTimer;
};

enum class EncoderTurn : int8_t {
  None = 0,
  Clockwise = 1,
  CounterClockwise = -1,
};

void initHardware();
void updateEncoder();

void setLightHeadRgb(uint16_t red, uint16_t green, uint16_t blue);
void setDarkroomRedLight(uint8_t brightness);
void setButtonsBacklight(uint8_t brightness);
void setDisplayBacklight(uint8_t brightness);

void beepTone(uint16_t frequencyHz, uint32_t durationMs, uint8_t duty = 128);
void stopBeep();

bool isLightOnButtonPressed();
bool isLightOffButtonPressed();
bool isLightTimerButtonPressed();
bool isDeveloperTimerButtonPressed();
ButtonState readButtons();

EncoderTurn consumeEncoderTurn();
int32_t getEncoderPosition();
bool isEncoderButtonPressed();

}  // namespace DarkroomHw
