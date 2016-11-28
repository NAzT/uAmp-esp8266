#include <Arduino.h>
#include "Constants.h"
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include "CMMC_Interval.hpp"
#include "CMMC_NTP.hpp"
#include <CMMCEasy.h>
#include "FS.h"
#include <functional>
#include <ESP8266WebServer.h>
#include <CMMC_OTA.h>

CMMC_OTA ota;
float firmware_version = 0.70;
float spiffs_version = 0.01;
String host_name = "";
String system_name = "Justpresso";

CMMCEasy easy;
float full_batt = 3122;
CMMC_Interval interval;
ESP8266WebServer _server(80);


#define PUBLISH_EVERY     (20*1000)// every 10 seconds

String restPath;
String restParams;

#include "util.h"

void init_hardware();
void init_fs();
void init_ap_sta_wifi();
void init_sta_wifi();

#include "JustPresso.hpp"
JustPresso justPresso;
CMMC_NTP ntp;
unsigned long ntp_mark;
Config_t *justConfig;


#include "JustPresso_WebServer.h"
JustPresso_WebServer webserver;
bool is_wake_by_justpresso = false;

void setup()
{
  init_hardware();
  is_wake_by_justpresso = isWakeByJustPresso();

  ota.init();

  init_fs();



  easy.blinker.init(CMMC_Blink::TYPE_TICKER);
  easy.blinker.blink(100, LED_BUILTIN);

  justPresso.init();
  justPresso.add(&easy);
  delay(1500);

  Serial.println("STARTING JUSTPRESSO");
  justPresso.begin();

  justConfig = const_cast<Config_t*>(justPresso.getConfiguration());
  ntp.setMemoryDate(atol(justConfig->hour.c_str()), atol(justConfig->minute.c_str()));


  if (justPresso.mode() == JUSTPRESSO_MODE_CONFIG) {
    Serial.println("MODE_CONFIG");
    init_ap_sta_wifi();
    Serial.println("AP STA Initialized.");
    webserver.init(&_server);
    webserver.begin();
    Serial.println("WebServer started.");
  }
  else if (justPresso.mode() == JUSTPRESSO_MODE_RUN) {
    Serial.println("MODE_RUN");
    justPresso.oled.clear();
    justPresso.oled.println();
    // justPresso.oled.printf("Connecting to \r\n\r\n[%s:%s]\r\n", justConfig->WIFI_SSID.c_str(),
    // justConfig->WIFI_PASSWORD.c_str());
    justPresso.oled.println("Connecting WiFi..");
    justPresso.oled.println();
    init_sta_wifi();
    justPresso.oled.println("Loading Time..");
    ntp.init();
    ntp.getTimeFromNtp();
    ntp_mark = millis();
    easy.blinker.detach();
    digitalWrite(LED_BUILTIN, HIGH);
    // justPresso.oled.println("Loaded.");
  }

  Serial.println("JUSTPRESSO STARTED");

}


void logBattery(String batt_string) {
    String battreqPath = justConfig->battery_path;

    battreqPath.replace(":batt:", batt_string);
    battreqPath.replace(":battRaw:", String(getBatteryVoltage()));

    Serial.println(battreqPath);
    httpGet(battreqPath);
}

bool go_sleep_flag = false;
void loop() {
  if (justPresso.mode() == JUSTPRESSO_MODE_RUN && !go_sleep_flag) {

    u_long warp_s = ntp.getSleepTime() - (millis() - ntp_mark)/1000;
    const Config_t *config = justPresso.getConfiguration();
    Serial.printf("Service = %s \r\n", config->service.c_str());
    Serial.printf("WAKE UP IN %lu \r\n", warp_s);

    if (config->service == "IFTTT") {
        Serial.printf("IFTT Event = %s,  IFTTT KEY %s \r\n",
        config->ifttt_event.c_str(), config->ifttt_key.c_str());

        char reqPathC[80];
        snprintf (reqPathC, 80, "http://maker.ifttt.com/trigger/%s/with/key/%s", config->ifttt_event.c_str(), config->ifttt_key.c_str());

        // get voltage
        int Vbatt = getBatteryVoltage();
        Serial.println("VBatt: " + String(Vbatt) + " mV, GPIO16: " + String(digitalRead(16)));
        Serial.println("BATT: " + String(Vbatt) + " mV");

        httpGet(String(reqPathC));
        logBattery(String((float)Vbatt/MaxVoltage*100));

        goSleep(warp_s);
    }
    else if (config->service == "REST") {
        float remaining_batt = (float)getBatteryVoltage()/MaxVoltage*100;
        String batt_string = String(remaining_batt);
        if (remaining_batt > 100.00) {
          batt_string = "100";
        }

        String reqPath = justConfig->rest_path+ justConfig->rest_params;
        Serial.printf("BATT = %s\r\n", batt_string.c_str());

        reqPath.replace(":batt:", batt_string);
        reqPath.replace(":battRaw:", String(getBatteryVoltage()));

        httpGet(reqPath);
        logBattery(batt_string);

        goSleep(warp_s);
    }
    else if (config->service == "MQTT") {
      Serial.printf("MQTT_HOST = %s, MQTT_PORT = %s \r\n",
        config->MQTT_HOST.c_str(), config->MQTT_PORT.c_str());
      Serial.printf("MQTT_PORT = %s, MQTT_CLIENT_ID = %s \r\n",
        config->MQTT_PORT.c_str(), config->MQTT_CLIENT_ID.c_str());
      Serial.printf("MQTT_USERNAME = %s, MQTT_PASSWORD = %s \r\n",
      config->MQTT_USERNAME.c_str(), config->MQTT_PASSWORD.c_str());

      goSleep(warp_s);
    }

    delay(1000);

  }
  if (justPresso.mode() == JUSTPRESSO_MODE_CONFIG) {
    _server.handleClient();
    long_press_check(13, 3000, []() {
      WiFi.disconnect();
      justPresso.oled.clear();
      justPresso.oled.println("Release to turn off.");
      justPresso.oled.println();
      while(digitalRead(13) == LOW) {
        yield();
      }
      goSleep(0);
    });

    // long_press_check(13, 2000, [&]() {
    //   justPresso.oled.clear();
    //   justPresso.oled.println("UPGRADING FIRMWARE..");
    //   justPresso.oled.println();
    //   justPresso.oled.println("Release to go...");
    //   while(digitalRead(13) == LOW) {
    //     // delay(0);
    //     yield();
    //   }
    //   justPresso.oled.clear();
    //   justPresso.oled.println();
    //   justPresso.oled.printf("UPGRADING SPIFFS..\r\n\r\n");
    //   doOTAUpdate("http://cmmc.io/just.spiffs.php", true, &justPresso.oled);
    //   delay(1000);
    //   justPresso.oled.clear();;
    //   justPresso.oled.println("UPGRADING FIRMWARE..");
    //   doOTAUpdate("http://cmmc.io/just.firmware.bin", false, &justPresso.oled);
    //   delay(3000);
    //   ESP.reset();
    // });

  }
  interval.loop();
}

void init_hardware()
{
  Serial.begin(115200);
  delay(10);
  Serial.println();
  Serial.println("Serial port initialized.");

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(13, INPUT);
  pinMode(0, INPUT_PULLUP);
  Serial.println(">>>> RESET INFO: ");
  Serial.println(ESP.getResetInfo());
  Serial.println("<<<< RESET INFO: ");

  initBattery();

  // get voltage
  int Vbatt = getBatteryVoltage();
  Serial.println("VBatt: " + String(Vbatt) + " mV, GPIO16: " + String(digitalRead(16)));
  Serial.println("BATT: " + String(Vbatt) + " mV");
}


void init_ap_sta_wifi() {
  // WiFi.disconnect();
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(getId().c_str());

  justPresso.oled.clear();
  // justPresso.oled.println(system_get_chip_id());
  justPresso.oled.println(getId().c_str());
  justPresso.oled.println();
  delay(100);

  struct station_config conf;
  wifi_station_get_config(&conf);
  String ssid = String(reinterpret_cast<char*>(conf.ssid));
  String password = String(reinterpret_cast<char*>(conf.password));
  IPAddress myIP = WiFi.softAPIP();
  IPAddress localIP = WiFi.localIP();

  if (localIP[0] == 0) {
    WiFi.disconnect();
  }
  justPresso.oled.println(myIP);

  if (ssid == "" && password == "") {

  }
  else {
    justPresso.oled.println();
    justPresso.oled.println(localIP);
  }

  // justPresso.oled.print("AP IP address: ");


}

void init_sta_wifi() {
  // WiFi.disconnect();
  String WIFI_SSID = justConfig->WIFI_SSID;
  String WIFI_PASSWORD = justConfig->WIFI_PASSWORD;
  WiFi.mode(WIFI_AP_STA);
  Serial.println("LOAD CONFIG DONE..");
  WiFi.begin(WIFI_SSID.c_str(), WIFI_PASSWORD.c_str());
  bool ok = true;

  // easy.blinker.blink(20*(5*ok), LED_BUILTIN);
  easy.blinker.blink(500);
  // oled.println("Connecting WiFi..");
  while(WiFi.status() != WL_CONNECTED) {
    Serial.printf ("Connecting to %s:%s\r\n", WIFI_SSID.c_str(), WIFI_PASSWORD.c_str());
    ok != ok;
    delay(500);
    interval.loop();
    long_press_check(13, 3000, []() {
      justPresso.oled.clear();
      justPresso.oled.println("Release to turn off.");
      justPresso.oled.println();
      while(digitalRead(13) == LOW) {
        yield();
      }
      goSleep(0);
    });

  }
  // easy.blinker.blink(1500, LED_BUILTIN);
  Serial.print("WiFi Connected. => ");
  Serial.println(WiFi.localIP());
}

void init_fs() {
  Serial.println("Mounting FS...");
  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount file system");
    return;
  }

  Dir root = SPIFFS.openDir("/");
  Serial.println("CMMC READING ROOT..");
  while (root.next()) {
    String fileName = root.fileName();
    // File f = root.openFile("r");
    // Serial.printf("%s: %d\r\n", fileName.c_str(), f.size());
    Serial.printf("%s ", fileName.c_str());
  };

  Serial.println("");
  Serial.println("FS mounted.");
}
