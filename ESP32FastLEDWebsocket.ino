ignore changes on single file
#include <WiFi.h>
#include "WifiCredentials.h"
#include <WiFiMulti.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <WiFiClient.h>
#include <WiFiMulti.h>
#include "FS.h"
#include "SPIFFS.h"
#define FASTLED_INTERNAL
#include <FastLED.h>            //https://github.com/FastLED/FastLED
#include <ArduinoJson.h>


WiFiMulti wifiMulti;
WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

#define NUM_LEDS 116
#define LED_PIN 4

CRGB leds[NUM_LEDS];
uint8_t ledBrightness = 96;
int currentLedIndex = 0;


/**************************************************************************/
/*  Arduino main funtions                                                 */
/**************************************************************************/
void setup() {
  Serial.begin(115200);
  delay(10);
  
  if(!SPIFFS.begin()){
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
  }

  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(ledBrightness);

  Serial.println("Connecting Wifi...");
  for(int i = 0; i < NB_WIFI_NETWORKS; i++) {
    Serial.printf("Add WiFi network (SSID : %s)\n", WIFI_SSIDS[i].c_str());
    WiFiMulti.addAP(WIFI_SSIDS[i].c_str(), WIFI_PWDS[i].c_str());
  }
  int i = 0;
  while (WiFiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
    i++;
    if (i >= 20) {
      ESP.restart();
    }
  }
  Serial.printf("WiFi connected, IP address: %s\n", WiFi.localIP().toString().c_str());


  server.onNotFound([](){
    if(!handleFileRead(server.uri()))
      server.send(404, "text/plain", "FileNotFound");
  });
  server.begin();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

void loop() {
  webSocket.loop();
  server.handleClient();
  FastLED.show();
}



/**************************************************************************/
/*  server funtions (needed here, it depends on global variables)         */
/**************************************************************************/
String getContentType(String filename){
  if(server.hasArg("download")) return "application/octet-stream";
  else if(filename.endsWith(".htm")) return "text/html";
  else if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".png")) return "image/png";
  else if(filename.endsWith(".gif")) return "image/gif";
  else if(filename.endsWith(".jpg")) return "image/jpeg";
  else if(filename.endsWith(".ico")) return "image/x-icon";
  else if(filename.endsWith(".xml")) return "text/xml";
  else if(filename.endsWith(".pdf")) return "application/x-pdf";
  else if(filename.endsWith(".zip")) return "application/x-zip";
  else if(filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}


bool handleFileRead(String path){
  if(path == "/") path += "config.html";
  Serial.println("handleFileRead: " + path);
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if(SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)){
    if(SPIFFS.exists(pathWithGz))
      path += ".gz";
    File file = SPIFFS.open(path, "r");
    size_t sent = server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}



/**************************************************************************/
/*  WebSocket callbacks                                                   */
/**************************************************************************/
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[%u] Disconnected!\n", num);
            break;
        case WStype_CONNECTED:
            {
                IPAddress ip = webSocket.remoteIP(num);
                Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
                // send message to client
                String str = getJSONCurrentConfig();
                webSocket.sendTXT(num, str);
            }
            break;
        case WStype_TEXT:
            {
              Serial.printf("[%u] get Text: %s\n", num, payload);   
              //String str = "";
              //for(int i = 0; i < length; i++) str += char(payload[i]);
              StaticJsonDocument<100> doc;
              DeserializationError err = deserializeJson(doc, payload);
              if(err) {
                Serial.printf("deserializeJson failed with code %i\n", err.c_str());
              }
              int brightness = doc["brightness"];
              if(brightness) {
                ledBrightness = brightness;
                FastLED.setBrightness(ledBrightness);
              }
              const char* ledColor = doc["led"];
              int ledIndex = doc["index"];
              if(ledColor != nullptr) {
                leds[ledIndex] = (int) strtol(ledColor + 1, NULL, 16);
              }
            }
            break;
      default:  
        break;
    }
}



String getJSONCurrentConfig() {
  StaticJsonDocument<400> doc;
  doc["brightness"] = ledBrightness;
  JsonArray ledsArray = doc.createNestedArray("leds");
  for(int i = 0; i < NUM_LEDS; i++) {
    ledsArray.add(crgbToHtmlString(leds[i]));
  }
  String jsonStr;
  serializeJson(doc, jsonStr);
  return jsonStr;
}


String crgbToHtmlString(CRGB color) {
  char buf[8];
  sprintf(buf, "#%02x%02x%02x", color.r, color.g, color.b);
  return String(buf);
}
