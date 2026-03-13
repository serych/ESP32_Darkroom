#include <Arduino.h>
#include <SPI.h>

#include <TFT_eSPI.h>
#include "Free_Fonts.h" // Include the header file attached to this sketch

TFT_eSPI tft = TFT_eSPI();


#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define DISP_BRIGHTNESS 255
#define FONT1 FSB18
#define FONT2 FSB24


void setup() {
  Serial.begin(115200);
  pinMode(TFT_LED, OUTPUT);
  analogWrite(TFT_LED, DISP_BRIGHTNESS);
  // Start the tft display
  tft.init();
  
  // Set the TFT display rotation in landscape mode
  tft.setRotation(1);
  uint16_t textColor = tft.color24to16(0xFF0000); // Red color
  // Clear the screen before writing to it
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(textColor, TFT_BLACK);
  
  
  // Set X and Y coordinates for center of display
  int centerX = SCREEN_WIDTH / 2;
  int centerY = SCREEN_HEIGHT / 2;
  tft.setFreeFont(FONT1);
  tft.drawString("Čas", 20, 20, 1);
  tft.setFreeFont(FONT2);
  tft.drawString("1:25", 20,50, 1);
}

void loop() {

    delay(100);

}
