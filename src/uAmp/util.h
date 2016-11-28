#ifndef CMMC_UTIL_H
#define CMMC_UTIL_H

#include <Arduino.h>
#include <ESP8266HTTPClient.h>

#include <Ticker.h>
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include "FS.h"
#include <functional>
#include "ESPert_JSON.hpp"
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include "ESPert_OLED.hpp"

typedef std::function<void(void)> void_callback_t;

void replaceString(String input) {
  input.replace("+", " ");
  input.replace("%40", "@");
}

String getId() {
  char textID[16] = {'\0'};
  sprintf(textID, "JP-%lu", ESP.getChipId());
  return String(textID);
}

String getAPIP() {
  IPAddress ip =  WiFi.softAPIP();
  char textID[32] = {'\0'};
  sprintf(textID, "%i.%i.%i.%i", ip[0], ip[1], ip[2], ip[3]);
  return String(textID);
}

void long_press_check(uint8_t pin, unsigned long threshold, void_callback_t callback) {
    long memo = millis();
    if (digitalRead(pin) == LOW) {
      while (digitalRead(pin) == LOW) {
        if (millis() - memo > threshold) {
            Serial.println("LONG PRESSED > 1ms");
            callback();
            break;
        }
        delay(0);
        yield();
      }
    }
}

void doOTAUpdate(String url, bool is_spiffs, ESPert_OLED* oled) {
  t_httpUpdate_return  ret;

  if (is_spiffs) {
    ret = ESPhttpUpdate.updateSpiffs(url);
  }
  else {
      Serial.println("Update sketch...");
      ret = ESPhttpUpdate.update(url);
  }
  oled->println();
  switch(ret) {
      case HTTP_UPDATE_FAILED:
          Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\r\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
          oled->printf("HTTP_UPDATE_FAILD Error (%d): %s\r\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
          break;

      case HTTP_UPDATE_NO_UPDATES:
          Serial.println("HTTP_UPDATE_NO_UPDATES");
          oled->println("HTTP_UPDATE_NO_UPDATES");
          break;

      case HTTP_UPDATE_OK:
          oled->println("HTTP_UPDATE_OK");
          Serial.println("HTTP_UPDATE_OK");
          break;
  }

}

void httpGet(String url) {
  HTTPClient http;

  Serial.print("[HTTP] begin...\n");

  http.begin(url); //HTTP
  Serial.print("[HTTP] GET...\n");
  // start connection and send HTTP header
  int httpCode = http.GET();

  // httpCode will be negative on error
  if(httpCode > 0) {
    // HTTP header has been send and Server response header has been handled
    Serial.printf("[HTTP] GET... code: %d\n", httpCode);
    // file found at server
    if(httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        Serial.println(payload);
    }
  } else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();
}

// Battery >>>
// max values in storange
#define MaxValues 21

// default
//#define MaxVoltage 6200
// adjusted
#define MaxVoltage 6080

// use ticker for get/store walue (value: @milli-sec, if 0 = disalble ticker)
#define TickerBattery 20

Ticker tickerBattery;
void swap(int &a, int &b)
{
  int t = a;
  a = b;
  b = t;
}

int partition(int *arr, const int left, const int right)
{
  const int mid = left + (right - left) / 2;
  const int pivot = arr[mid];
  // move the mid point value to the front.
  swap(arr[mid], arr[left]);
  int i = left + 1;
  int j = right;
  while (i <= j)
  {
    while (i <= j && arr[i] <= pivot)
    {
      i++;
    }

    while (i <= j && arr[j] > pivot)
    {
      j--;
    }

    if (i < j)
    {
      swap(arr[i], arr[j]);
    }
  }

  swap(arr[i - 1], arr[left]);
  return i - 1;
}

void quickSort(int *arr, const int left, const int right)
{
  if (left >= right)
  {
    return;
  }

  int part = partition(arr, left, right);

  quickSort(arr, left, part - 1);
  quickSort(arr, part + 1, right);
}

int median(int arr[], int maxValues)
{
  quickSort(arr, 0, maxValues - 1);
  return arr[maxValues / 2];
}

int getBatteryVoltage()
{
  static int filters[MaxValues] = {0};
  static int lastIndex = 0;

  int val = analogRead(A0);

  filters[lastIndex++ % MaxValues] = val;
  val = median(filters, MaxValues);

  // for AVG >>>
  //static float filterAvg = 0;
  //filterAvg = (filterAvg + val) / 2.0;
  //return map(filterAvg, 0, 1023, 0, MaxVoltage);
  // for AVG <<<

  return map(val, 0, 1023, 0, MaxVoltage);
}

void storeBatteryValue()
{
  getBatteryVoltage();
}

void initBattery()
{
  // Store values to buffer
  for (int i = 0; i < MaxValues; ++i)
  {
    storeBatteryValue();
  }

  if (TickerBattery > 0)
  {
    tickerBattery.attach_ms(TickerBattery, storeBatteryValue);
  }
}

// Battery <<<

// Check Wake from? >>>
// 1. out LOW at GPIO13
// 2. save timer counter
// 3. wait for GPIO13 is LOW ( HIGH --> LOW )
// 4. check lap time
// 5. if lap time < 810 = wake by GPIO13
// 5. if lap time > 810 = wake by other
// return true when wake by GPIO13 (on JustPresso or ESPresso Lite)
// return false when wake by reset (on ESPresso Lite), deepsleep timeout, software reset-restart, etc.
bool isWakeByJustPresso(unsigned long nTimeOutMicrosec = 810)
{
  digitalWrite(13, LOW);
  pinMode(13, OUTPUT);

  unsigned long prev = micros();
  while (digitalRead(13) == HIGH)
  {
    if (micros() - prev > nTimeOutMicrosec)
    {
      break;
    }
  }

  pinMode(13, INPUT);

  return (micros() - prev) < nTimeOutMicrosec;
}
// Check Wake from? <<<


void goSleep(u_long s) {
    // send command: Tern-off OLED
    Wire.beginTransmission(0x3c);     // begin transmitting
    Wire.write(0x80);                 // command mode
    Wire.write(0xAE);                 // Tern-off OLED
    Wire.endTransmission();           // stop transmitting

    ESP.deepSleep(s * 1000000);
}



#endif
