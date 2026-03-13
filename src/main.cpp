#include <Arduino.h>
#include <SPI.h>

#include <TFT_eSPI.h>
#include "Free_Fonts.h"
#include "darkroom_hw.h"

namespace {

constexpr uint8_t kDisplayBrightness = 192;
constexpr uint8_t kButtonsBrightness = 64;
constexpr uint8_t kDarkroomRedBrightness = 0;
constexpr uint16_t kLightHeadIdleLevel = 0;

TFT_eSPI tft = TFT_eSPI();

}  // namespace

void setup() {
  Serial.begin(115200);

  DarkroomHw::initHardware();
  DarkroomHw::setDisplayBacklight(kDisplayBrightness);
  DarkroomHw::setButtonsBacklight(kButtonsBrightness);
  DarkroomHw::setDarkroomRedLight(kDarkroomRedBrightness);
  DarkroomHw::setLightHeadRgb(kLightHeadIdleLevel, kLightHeadIdleLevel, kLightHeadIdleLevel);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  const uint16_t titleColor = tft.color24to16(0xFF3030);
  const uint16_t bodyColor = tft.color24to16(0xE0E0E0);

  tft.setTextColor(titleColor, TFT_BLACK);
  tft.setFreeFont(FSB18);
  tft.drawString("Darkroom timer", 20, 24, 1);

  tft.setTextColor(bodyColor, TFT_BLACK);
  tft.setFreeFont(FSS12);
  tft.drawString("RGB head: GPIO 25 / 26 / 27", 20, 74, 1);
  tft.drawString("Darkroom red: GPIO 13", 20, 102, 1);
  tft.drawString("Buttons BL: GPIO 14", 20, 130, 1);
  tft.drawString("Beeper: GPIO 16", 20, 158, 1);
  tft.drawString("Display BL: GPIO 17", 20, 186, 1);

  DarkroomHw::updateEncoder();
}

void loop() {
  DarkroomHw::updateEncoder();
  delay(100);
}
