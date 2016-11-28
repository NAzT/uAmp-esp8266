#ifndef ESPERT_JUST_PRESSO_H
#define ESPERT_JUST_PRESSO_H

#include <Arduino.h>
#include "util.h"
#include <FS.h>
#include "ESPert_OLED.hpp"
#include "ESPert_JSON.hpp"
#include "Constants.h"
#include <CMMCEasy.h>

enum JUSTPRESSO_MODE{
  JUSTPRESSO_MODE_RUN = 0,
  JUSTPRESSO_MODE_CONFIG = 1,
};

#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>

class JustPresso {
  private:
    Config_t configuration;
    StaticJsonBuffer<1024> jsonBuffer;
    ESPert_JSON *jsonParser;
    JsonObject* root;
    String running_mode;
    CMMCEasy *easy;
    std::function<void(void)> web_server_mode_fn;
    bool initConfiguration();
    JUSTPRESSO_MODE _mode ;
  public:
    bool loadConfig();
    bool saveConfig(JsonObject& );
    ESPert_OLED oled;
    uint8_t mode();
    JustPresso();
    void init();
    void begin();
    JustPresso* add(CMMCEasy*);
    String getConfigService();
    void forceEnterConfigMode() { this->_mode = JUSTPRESSO_MODE_CONFIG; };
    ESPert_JSON *getJsonParser() { return this->jsonParser; };
    const Config_t* getConfiguration();
};

JustPresso* JustPresso::add(CMMCEasy *easy) {
  this->easy = easy;
  return this;
}

JustPresso::JustPresso() {
    jsonParser = new ESPert_JSON;
    this->easy = NULL;
    _mode = JUSTPRESSO_MODE_RUN;
    static JustPresso* that = this;
    web_server_mode_fn = [&]() {
      Serial.println(">>>> LONG PRESSED.");
      Serial.println("<<<< WEB SERVER MODE...");

      if (that->easy != NULL) {
        that->easy->blinker.blink(20);
        Serial.println("EASY BLINK 20");
      }

      that->oled.clear();
      that->oled.setTextSize(1);
      that->oled.setTextColor(WHITE);
      that->oled.setCursor(0, 0);

      that->oled.println("CONFIG Mode.");
      // that->oled.println("Release the button.");

      that->_mode = JUSTPRESSO_MODE_CONFIG;

      while (!digitalRead(13))
      {
        delay(0);
        yield();
      }

      oled.clear();
    };
}

uint8_t JustPresso::mode() {
  return this->_mode;
}

bool JustPresso::loadConfig() {
  Serial.println("=JUST PRESSO=");
  Serial.println("LOADING CONFIG");
  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("Failed to open config file");
    return false;
  }

  size_t size = configFile.size();
  if (size > 1024) {
    Serial.println("Config file size is too large");
    return false;
  }

  wdt_disable();
  wdt_enable(WDTO_8S);

  Serial.println("CONFIG OPENNED");

  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

  // We don't use String here because ArduinoJson library requires the input
  // buffer to be mutable. If you don't use ArduinoJson, you may as well
  // use configFile.readString instead.
  configFile.readBytes(buf.get(), size);

  // this->oled.println("Parssing Config.");
  if (!jsonParser->init(String(buf.get()))) {
    Serial.println("Failed to parse config file");
    return false;
  }

  // this->oled.println("Parsed OK.");
  Serial.println("JSON PARSED: RESULT OK");
  jsonParser->dumpSerial();
  Serial.println("/JSON PARSED: RESULT OK");

  this->configuration.WIFI_SSID         = jsonParser->get("ssid");
  this->configuration.WIFI_PASSWORD     = jsonParser->get("password");
  this->configuration.rest_params       = jsonParser->get("rest_params");
  this->configuration.rest_path         = jsonParser->get("rest_path");
  this->configuration.hour              = jsonParser->get("hour");
  this->configuration.minute            = jsonParser->get("minute");
  this->configuration.ifttt_key         = jsonParser->get("ifttt_key");
  this->configuration.ifttt_event       = jsonParser->get("ifttt_event");
  this->configuration.mqtt_client_id    = jsonParser->get("mqtt_client_id");
  this->configuration.mqtt_username     = jsonParser->get("mqtt_username");
  this->configuration.mqtt_password     = jsonParser->get("mqtt_password");
  this->configuration.mqtt_host         = jsonParser->get("mqtt_host");
  this->configuration.mqtt_port         = jsonParser->get("mqtt_port");
  this->configuration.service           = jsonParser->get("service");
  this->configuration.factory_upgrade   = jsonParser->get("factory_upgrade");
  this->configuration.battery_path      = jsonParser->get("battery_path");

  this->configuration.dumpSerial();

  if (this->configuration.service == "") {
    forceEnterConfigMode();
  }

  // ul_unixtime = strtol(unixtime.c_str(), NULL, 10);
  // printUnixTimeToUTCTime(ul_unixtime);

  return true;
}

String JustPresso::getConfigService() {
  return this->configuration.service;
}

bool JustPresso::saveConfig(JsonObject& json) {
  Serial.println(">>>>>> SAVE CONFIG");
  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return false;
  }

  json.printTo(Serial);
  json.printTo(configFile);
  Serial.println();

  Serial.println("<<<<<< SAVE CONFIG");
  return true;
}

bool JustPresso::initConfiguration() {
  Serial.println("SETTING UP FIRST CONFIG..");
  JsonObject& json = jsonBuffer.createObject();

  json["ssid"]              = "";
  json["password"]          = "";
  json["rest_params"]       = "";
  json["rest_path"]         = "";
  json["hour"]              = "";
  json["minute"]            = "";
  json["ifttt_key"]         = "";
  json["ifttt_event"]       = "";
  json["mqtt_client_id"]    = "";
  json["mqtt_username"]     = "";
  json["mqtt_password"]     = "";
  json["mqtt_host"]         = "";
  json["mqtt_port"]         = "";
  json["service"]           = "";
  json["factory_upgrade"]    = "NO";
  return saveConfig(json);
}

const Config_t* JustPresso::getConfiguration() {
  return &this->configuration;
}

void JustPresso::init() {
  oled.init();
}

void JustPresso::begin() {
  long_press_check(13, 300, web_server_mode_fn);
  long_press_check(0, 300, web_server_mode_fn);

  if (!loadConfig()) {
    Serial.println("First Open Config.");
    if (initConfiguration()) {
      Serial.println("Init Configuration OK.");
    }
    else {
      Serial.println("FAILED to initconfiguration.");
    };
  }
  else {
    Serial.println("LoadConfiguration: SUCCESS");
  }
}

#endif
