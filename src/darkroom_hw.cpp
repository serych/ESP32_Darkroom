#include "darkroom_hw.h"

#include <RotaryEncoder.h>

namespace DarkroomHw {

namespace {

constexpr uint16_t kPwmFrequency = 5000;
constexpr uint8_t kRgbResolutionBits = 10;
constexpr uint16_t kRgbMaxDuty = (1u << kRgbResolutionBits) - 1u;

constexpr uint16_t kAuxPwmFrequency = 5000;
constexpr uint8_t kAuxResolutionBits = 8;

constexpr uint8_t kLightRedChannel = 0;
constexpr uint8_t kLightGreenChannel = 1;
constexpr uint8_t kLightBlueChannel = 2;
constexpr uint8_t kDarkroomChannel = 3;
constexpr uint8_t kButtonsBacklightChannel = 4;
constexpr uint8_t kDisplayBacklightChannel = 5;
constexpr uint8_t kBeeperChannel = 6;

RotaryEncoder encoder(PinAssignment::kEncoderA, PinAssignment::kEncoderB, RotaryEncoder::LatchMode::FOUR3);
long lastEncoderPosition = 0;
int8_t pendingEncoderDelta = 0;

uint16_t clampRgb(uint16_t value) {
  return value > kRgbMaxDuty ? kRgbMaxDuty : value;
}

void attachPwmChannel(uint8_t pin, uint8_t channel, uint16_t frequency, uint8_t resolutionBits) {
  ledcSetup(channel, frequency, resolutionBits);
  ledcAttachPin(pin, channel);
}

void configureButtonPin(uint8_t pin) {
  if (pin == 34 || pin == 35 || pin == 36 || pin == 39) {
    pinMode(pin, INPUT);
  } else {
    pinMode(pin, INPUT_PULLUP);
  }
}

bool isPressed(uint8_t pin) {
  return digitalRead(pin) == LOW;
}

}  // namespace

void initHardware() {
  attachPwmChannel(PinAssignment::kLightHeadRed, kLightRedChannel, kPwmFrequency, kRgbResolutionBits);
  attachPwmChannel(PinAssignment::kLightHeadGreen, kLightGreenChannel, kPwmFrequency, kRgbResolutionBits);
  attachPwmChannel(PinAssignment::kLightHeadBlue, kLightBlueChannel, kPwmFrequency, kRgbResolutionBits);

  attachPwmChannel(PinAssignment::kDarkroomRed, kDarkroomChannel, kAuxPwmFrequency, kAuxResolutionBits);
  attachPwmChannel(PinAssignment::kButtonsBacklight, kButtonsBacklightChannel, kAuxPwmFrequency, kAuxResolutionBits);
  attachPwmChannel(PinAssignment::kDisplayBacklight, kDisplayBacklightChannel, kAuxPwmFrequency, kAuxResolutionBits);
  attachPwmChannel(PinAssignment::kBeeper, kBeeperChannel, 2000, kAuxResolutionBits);

  configureButtonPin(PinAssignment::kButtonLightOn);
  configureButtonPin(PinAssignment::kButtonLightOff);
  configureButtonPin(PinAssignment::kButtonLightTimer);
  configureButtonPin(PinAssignment::kButtonDeveloperTimer);
  configureButtonPin(PinAssignment::kEncoderA);
  configureButtonPin(PinAssignment::kEncoderB);
  configureButtonPin(PinAssignment::kEncoderButton);

  setLightHeadRgb(0, 0, 0);
  setDarkroomRedLight(0);
  setButtonsBacklight(0);
  setDisplayBacklight(0);
  stopBeep();

  encoder.tick();
  lastEncoderPosition = encoder.getPosition();
  pendingEncoderDelta = 0;
}

void updateEncoder() {
  encoder.tick();

  const long currentPosition = encoder.getPosition();
  const long step = currentPosition - lastEncoderPosition;

  if (step > 0) {
    pendingEncoderDelta = 1;
  } else if (step < 0) {
    pendingEncoderDelta = -1;
  }

  lastEncoderPosition = currentPosition;
}

void setLightHeadRgb(uint16_t red, uint16_t green, uint16_t blue) {
  ledcWrite(kLightRedChannel, clampRgb(red));
  ledcWrite(kLightGreenChannel, clampRgb(green));
  ledcWrite(kLightBlueChannel, clampRgb(blue));
}

void setDarkroomRedLight(uint8_t brightness) {
  ledcWrite(kDarkroomChannel, brightness);
}

void setButtonsBacklight(uint8_t brightness) {
  ledcWrite(kButtonsBacklightChannel, brightness);
}

void setDisplayBacklight(uint8_t brightness) {
  ledcWrite(kDisplayBacklightChannel, brightness);
}

void beepTone(uint16_t frequencyHz, uint32_t durationMs, uint8_t duty) {
  if (frequencyHz == 0 || durationMs == 0) {
    stopBeep();
    return;
  }

  ledcWriteTone(kBeeperChannel, frequencyHz);
  ledcWrite(kBeeperChannel, duty);
  delay(durationMs);
  stopBeep();
}

void stopBeep() {
  ledcWriteTone(kBeeperChannel, 0);
  ledcWrite(kBeeperChannel, 0);
}

bool isLightOnButtonPressed() {
  return isPressed(PinAssignment::kButtonLightOn);
}

bool isLightOffButtonPressed() {
  return isPressed(PinAssignment::kButtonLightOff);
}

bool isLightTimerButtonPressed() {
  return isPressed(PinAssignment::kButtonLightTimer);
}

bool isDeveloperTimerButtonPressed() {
  return isPressed(PinAssignment::kButtonDeveloperTimer);
}

ButtonState readButtons() {
  return ButtonState{
      isLightOnButtonPressed(),
      isLightOffButtonPressed(),
      isLightTimerButtonPressed(),
      isDeveloperTimerButtonPressed(),
  };
}

EncoderTurn consumeEncoderTurn() {
  const int8_t delta = pendingEncoderDelta;
  pendingEncoderDelta = 0;

  if (delta > 0) {
    return EncoderTurn::Clockwise;
  }

  if (delta < 0) {
    return EncoderTurn::CounterClockwise;
  }

  return EncoderTurn::None;
}

int32_t getEncoderPosition() {
  return static_cast<int32_t>(lastEncoderPosition);
}

bool isEncoderButtonPressed() {
  return isPressed(PinAssignment::kEncoderButton);
}

}  // namespace DarkroomHw
