#ifndef ESPERT_JUSTPRESSO_CONSTANTS_H
#define ESPERT_JUSTPRESSO_CONSTANTS_H

  #include <Arduino.h>


  struct Config_t {
    String MQTT_HOST;
    String MQTT_USERNAME;
    String MQTT_PORT;
    String MQTT_PASSWORD;
    String MQTT_CLIENT_ID;
    String WIFI_SSID;
    String WIFI_PASSWORD;
    String rest_params;

    String rest_path;
    String hour;
    String minute;

    String ifttt_key;
    String ifttt_event;
    String mqtt_host;
    String mqtt_port;
    String mqtt_client_id;
    String mqtt_username;
    String mqtt_password;
    String service;

    String battery_path;

    String factory_upgrade;

    void dumpSerial() {
      Serial.printf("MQTT_HOST = %s, MQTT_PORT = %s \r\n", MQTT_HOST.c_str(), MQTT_PORT.c_str());
      Serial.printf("SSID = %s, PASS = %s \r\n", WIFI_SSID.c_str(), WIFI_PASSWORD.c_str());
    }

    void dumpMqttInfo() {
      Serial.printf("MQTT_HOST = %s, MQTT_PORT = %s \r\n", MQTT_HOST.c_str(), MQTT_PORT.c_str());
      Serial.printf("MQTT_PORT = %s, MQTT_CLIENT_ID = %s \r\n", MQTT_PORT.c_str(), MQTT_CLIENT_ID.c_str());
      Serial.printf("MQTT_USERNAME = %s, MQTT_PASSWORD = %s \r\n", MQTT_USERNAME.c_str(), MQTT_PASSWORD.c_str());
    }
  };

#endif
