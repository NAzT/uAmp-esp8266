#ifndef CMMC_ESPERT_JSON_H
#define CMMC_ESPERT_JSON_H

#include <ArduinoJson.h>
#include <Arduino.h>


class ESPert_JSON
{
  private:
    char* json;
    JsonObject* root;
    DynamicJsonBuffer jsonBuffer;

  public:
    ESPert_JSON();
    bool init(String payload);
    bool containsKey(String key);
    String get(String key);
    void release();
    void dumpSerial();
    JsonObject* getRoot(JsonObject*root = NULL);
};

// ****************************************
// JSON class
// ****************************************
ESPert_JSON::ESPert_JSON() {
  json = NULL;
  root = NULL;
}

bool ESPert_JSON::init(String payload) {
  release();

  bool success = false;
  unsigned int length = payload.length();

  if (json = (char*)malloc(length+1)) {
    memset(json, '\0', length);
    memcpy(json, payload.c_str(), length);
    root = &jsonBuffer.parseObject(json);

    if (!(success = root->success())) {
      release();
    }
  }

  return success;
}

bool ESPert_JSON::containsKey(String key) {
  return (json && root->containsKey(key.c_str()));
}

String ESPert_JSON::get(String key) {
  return (json ? (const char*)(*root)[key.c_str()] : "");
}

void ESPert_JSON::release() {
  free(json);
  json = NULL;
  root = NULL;
}

void ESPert_JSON::dumpSerial() {
  root->printTo(Serial);
}


JsonObject* ESPert_JSON::getRoot(JsonObject* root) {
  if (root != NULL) {
    root = this->root;
  }
  return this->root;
}


#endif
