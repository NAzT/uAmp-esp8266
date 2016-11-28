#ifndef __JUST_PRESSO_WEBSERVER_H
#define __JUST_PRESSO_WEBSERVER_H

#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include "FS.h"
#include "util.h"
#include <ArduinoJson.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>

#ifdef ESP8266
extern "C" {
#include "user_interface.h"

}
#endif
StaticJsonBuffer<1024> jsonBuffer;

extern String system_name;
extern String host_name;
extern float firmware_version;
extern float spiffs_version;

int currentWiFiStatus = -1;
void WiFiStatus( bool force );
void WiFiStatus(bool bForse=false) {
  static struct station_config conf;
  wifi_station_get_config(&conf);

  int n = WiFi.status();
  if( bForse || n != currentWiFiStatus )  {
    Serial.printf( "WiFi status: %i\n", n );
    currentWiFiStatus = n;
    if( n == WL_CONNECTED ) {
      Serial.printf( "SSID: %s\n", WiFi.SSID().c_str() );
      const char* passphrase = reinterpret_cast<const char*>(conf.password);
      Serial.printf( "Password: (%i) %s\n", strlen( passphrase ), passphrase );
      Serial.print("Local IP: ");
      Serial.println(WiFi.localIP());
    }
  }
}

class JustPresso_WebServer {
  private:
    ESP8266WebServer *server;
  public:

    void init(ESP8266WebServer *server_pointer) {
      this->server = server_pointer;
    }

    void init_webserver() {
      static JustPresso_WebServer *that = this;
      server->on("/factory_reset", [&]() {
        SPIFFS.remove("/config.json");
        String out = String("FACTORY RESET.. BEING REBOOTED.");
        server->send(200, "text/plain", out.c_str());
        WiFi.disconnect();
        delay(1000);
        ESP.reset();
      });

      server->on("/update", HTTP_POST, [](){
        that->server->sendHeader("Connection", "close");
        that->server->sendHeader("Access-Control-Allow-Origin", "*");
        that->server->send(200, "text/plain", (Update.hasError())?"FAIL":"OK");
        ESP.restart();
      },[](){
        HTTPUpload& upload = that->server->upload();
        if(upload.status == UPLOAD_FILE_START){
          Serial.setDebugOutput(true);
          WiFiUDP::stopAll();
          Serial.printf("Update: %s\n", upload.filename.c_str());
          uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
          if(!Update.begin(maxSketchSpace)){//start with max available size
            Update.printError(Serial);
          }
        } else if(upload.status == UPLOAD_FILE_WRITE){
          if(Update.write(upload.buf, upload.currentSize) != upload.currentSize){
            Update.printError(Serial);
          }
        } else if(upload.status == UPLOAD_FILE_END){
          if(Update.end(true)){ //true to set the size to the current progress
            Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
          } else {
            Update.printError(Serial);
          }
          Serial.setDebugOutput(false);
        }
        yield();
      });

      server->on("/save", HTTP_POST, []() {
        struct station_config conf;
        wifi_station_get_config(&conf);

        String ssid = String(reinterpret_cast<char*>(conf.ssid));
        String password = String(reinterpret_cast<char*>(conf.password));

        if (ssid == "" && password == "") {
          String out = String("Config WiFi First..");
          that->server->send(403, "text/plain", out.c_str());
          return;
        }

        JsonObject& json = jsonBuffer.createObject();
        json["ssid"]              = ssid;
        json["password"]          = password;
        json["rest_params"]       = that->server->arg("rest_params");
        json["rest_path"]         = that->server->arg("rest_path");
        json["hour"]              = that->server->arg("hour");
        json["minute"]            = that->server->arg("minute");
        json["ifttt_key"]         = that->server->arg("ifttt_key");
        json["ifttt_event"]       = that->server->arg("ifttt_event");
        json["mqtt_client_id"]    = that->server->arg("mqtt_client_id");
        json["mqtt_username"]     = that->server->arg("mqtt_username");
        json["mqtt_password"]     = that->server->arg("mqtt_password");
        json["mqtt_host"]         = that->server->arg("mqtt_host");
        json["mqtt_port"]         = that->server->arg("mqtt_port");
        json["service"]           = that->server->arg("service");
        json["battery_path"]      = that->server->arg("battery_path");
        json["factory_upgrade"]   = "NO";

        bool config = justPresso.saveConfig(json);

        yield();
        // String out = String(config) + String(".... BEING REBOOTED.");
        String out = String(".... BEING REBOOTED.");
        that->server->send(200, "text/plain", out.c_str());
        // WiFi.disconnect();
        // WiFi.mode(WIFI_STA);
        // WiFi.begin(ssid.c_str(), password.c_str());
        // delay(1000);
        // ESP.reset();
      });

      server->on("/millis", []() {
        char buff[100];
        String ms = String(millis());
        sprintf(buff, "{\"millis\": %s }", ms.c_str());
        that->server->send (200, "text/plain", buff );
      });

      server->on("/scan", HTTP_GET, []() {
        char myIpString[24];
        IPAddress myIp = WiFi.localIP();
        sprintf(myIpString, "%d.%d.%d.%d", myIp[0], myIp[1], myIp[2], myIp[3]);

        int n = WiFi.scanNetworks();
        String currentSSID = WiFi.SSID();
        yield();
        String output = "{\"list\":[";
        for( int i=0; i<n; i++ ) {
          if (output != "{\"list\":[") output += ',';
          output += "\"";
          //if( currentSSID == WiFi.SSID(i) )
          //  output += "***>";
          output += WiFi.SSID(i);
          output += "\"";
          yield();
        }
        output += "]";
        output += ",\"current\":\""+currentSSID+"\"";
        output += ", \"ip\":\""+String( myIpString )+"\"";
        output += ", \"host\":\""+host_name+"\"";
        output += ", \"name\":\""+system_name+"\"";
        output += ", \"version\":\""+String(firmware_version)+"\"";
        output += ", \"spiffs\":\""+String(spiffs_version)+"\"";

        output += "}";
        Serial.println( output );
        that->server->send(200, "text/json", output);
      });

      server->on("/update", HTTP_POST, [](){
        that->server->sendHeader("Connection", "close");
        that->server->sendHeader("Access-Control-Allow-Origin", "*");
        that->server->send(200, "text/plain", (Update.hasError())?"FAIL":"OK");
        ESP.restart();
      },[](){
        HTTPUpload& upload = that->server->upload();
        if(upload.status == UPLOAD_FILE_START){
          Serial.setDebugOutput(true);
          WiFiUDP::stopAll();
          Serial.printf("Update: %s\n", upload.filename.c_str());
          uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
          if(!Update.begin(maxSketchSpace)){//start with max available size
            Update.printError(Serial);
          }
        } else if(upload.status == UPLOAD_FILE_WRITE){
          if(Update.write(upload.buf, upload.currentSize) != upload.currentSize){
            Update.printError(Serial);
          }
        } else if(upload.status == UPLOAD_FILE_END){
          if(Update.end(true)){ //true to set the size to the current progress
            Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
          } else {
            Update.printError(Serial);
          }
          Serial.setDebugOutput(false);
        }
        yield();
      });

      server->on("/disconnect", HTTP_GET, [&](){
        WiFi.disconnect();
        server->send(200, "text/plain", "Disconnected.");
      });

      server->on( "/batt", []() {
        char buff[100];
        sprintf(buff, "{\"batt\": %s }", String(getBatteryVoltage()).c_str());
        that->server->send (200, "text/plain", buff );
      });

      server->on( "/storage", []() {
        char buff[600];
        justPresso.loadConfig();
        justPresso.getJsonParser()->getRoot()->printTo(buff, sizeof(buff));
        that->server->send (200, "text/plain", buff );
      });

      server->on("/ota", HTTP_POST, []() {
        String is_spiffs = that->server->arg("is_spiff");
        String url = that->server->arg("url");
        Serial.printf("url = %s, is spiffs = %s", url.c_str(), is_spiffs.c_str());
        that->server->send (200, "text/plain", "OF");

        static char verString[15];
        dtostrf(firmware_version,5,2,verString);

        String verSPIFFS = "0.01";

        File f = SPIFFS.open("/version", "r");
        if (f) {
          Serial.println("Reading SPIFFS version..." );
          while(f.available()) {
            //Lets read line by line from the file
            verSPIFFS = f.readStringUntil('\n');
          }
        }
        else {
          Serial.println( "No version info" );
        }
        f.close();
        Serial.println("Update SPIFFS from "+verSPIFFS+"...");
        t_httpUpdate_return ret;
        if (is_spiffs == "on") {
          ret = ESPhttpUpdate.updateSpiffs(url);
        }
        else {
          ret = ESPhttpUpdate.update(url);
        }

        switch(ret) {
            case HTTP_UPDATE_FAILED:
                Serial.printf("SPIFFS HTTP_UPDATE_FAILD Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
                Serial.println();
                break;

            case HTTP_UPDATE_NO_UPDATES:
                Serial.println("SPIFFS HTTP_UPDATE_NO_UPDATES");
                break;

            case HTTP_UPDATE_OK:
                Serial.println("SPIFFS HTTP_UPDATE_OK");
                break;
        }
      });

      server->on("/reset", HTTP_GET, [&](){
        server->send(200, "text/plain", "Device Restarted.");
        delay(100);
        ESP.reset();
      });


      server->on("/reboot", HTTP_GET, [&](){
        server->send(200, "text/plain", "Device Restarted.");
        delay(100);
        ESP.reset();
      });

      server->on("/factory_upgrade", HTTP_POST, [&](){
        WiFi.disconnect();
        WiFi.mode(WIFI_STA);
        JsonObject *json= justPresso.getJsonParser()->getRoot();
        (*json)["factory_upgrade"] = "YES";
        justPresso.saveConfig(*json);
        server->send(200, "text/plain", "TO BE UPGRADED.");
        delay(200);
        ESP.reset();
      });

      server->on("/ssid", HTTP_POST, [&]() {
        String ssid = that->server->arg("ssid");
        String password = that->server->arg("password");

        justPresso.loadConfig();
        JsonObject *jsonConfig= justPresso.getJsonParser()->getRoot();
        (*jsonConfig)["ssid"] = ssid;
        (*jsonConfig)["password"] = password;
        justPresso.saveConfig(*jsonConfig);

        ssid.replace("+", " ");
        ssid.replace("%40", "@");

        Serial.println( "SSID: " + ssid );
        Serial.println( "Password: " + password );

        WiFi.begin(ssid.c_str(), password.c_str());
        Serial.println("Waiting for WiFi to connect!");

        int c = 0;
        while (c < 50*5) {
          int n = WiFi.status();
          Serial.printf( "WiFi status: %i\n", n );
          if( n == WL_CONNECTED) {
            Serial.println();
            Serial.print("WiFi connected OK, local IP " );
            Serial.println( WiFi.localIP() );
            delay(50);
            break;
          }
          if( n == WL_IDLE_STATUS ) {
            Serial.println("No auto connect available!");
            break;
          }
          c++;
          yield();
          delay(45);
        }

        String json = "{";
        if (WiFi.status() == WL_CONNECTED) {
          char myIpString[24];
          IPAddress myIp = WiFi.localIP();
          sprintf(myIpString, "%d.%d.%d.%d", myIp[0], myIp[1], myIp[2], myIp[3]);
          json += "\"result\":\"success\"";
          json += ",\"current\":\""+ssid+"\"";
          json += ", \"ip\":\""+String( myIpString )+"\"";
          json += ", \"host\":\""+host_name+"\"";
          Serial.println("WiFi: Success!");
          WiFiStatus(true);
        } else {
          json += "\"result\":\"failed\"";
          Serial.println("WiFi: Failed!");
        }
        json += "}";
        Serial.println( json );
        that->server->send(200, "text/json", json);
      });

      server->serveStatic("/", SPIFFS, "/");
      server->begin();
    }

    void begin() {
      init_webserver();
    }


};


#endif
