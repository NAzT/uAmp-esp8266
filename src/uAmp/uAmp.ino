#include <Arduino.h>
#include "ESPert_OLED.hpp"
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include "font.h"
SoftwareSerial swSer(14, 12, false, 256);

ESPert_OLED oled;
byte r;
uint8_t idx = 0;
byte arr[3];


void init_hardware()
{
  Serial.begin(115200);
  swSer.begin(115200);
  delay(10);
  Serial.println();
  Serial.println("Serial port initialized.");
}

SSD1306* display;

void init_oled() {
  oled.init();
  display = oled.getDisplay();
  // display->setFont(Dialog_plain_16);
}

void setup()
{
  init_hardware();
  WiFi.mode(WIFI_OFF);
  init_oled();
}

void loop() {
  if (swSer.available() > 0) {
    r = swSer.read();
    if (r == '\n') {
      uint16_t number;
      number = (arr[1] << 8) | (arr[2]);
      Serial.println((uint16_t) number);
      idx = 0;
      oled.clear();
      // display->setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
      oled.setCursor(128/2 - 5, 64/2);
      oled.print(number);
      oled.update();
    }
    else {
      arr[idx++] = r;
    }
  }
}
