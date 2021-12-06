/*
This is the code for the AirGradient DIY Air Quality Sensor with an ESP8266 Microcontroller.

It is a high quality sensor showing PM2.5, CO2, Temperature and Humidity on a small display and can send data over Wifi.

For build instructions please visit https://www.airgradient.com/diy/

Compatible with the following sensors:
Plantower PMS5003 (Fine Particle Sensor)
SenseAir S8 (CO2 Sensor)
SHT30/31 (Temperature/Humidity Sensor)

Please install ESP8266 board manager (tested with version 3.0.0)

The codes needs the following libraries installed:
"WifiManager by tzapu, tablatronix" tested with Version 2.0.3-alpha
"ESP8266 and ESP32 OLED driver for SSD1306 displays by ThingPulse, Fabrice Weinberg" tested with Version 4.1.0

Configuration:
Please set in the code below which sensor you are using and if you want to connect it to WiFi.

If you are a school or university contact us for a free trial on the AirGradient platform.
https://www.airgradient.com/schools/

MIT License
*/

// comment sensors that you do not use
#define hasPM
#define hasCO2
#define hasSHT
// #define hasTVOC

// uncomment to flip screen
// #define flipScreen

// uncomment if you want to connect to wifi. The display will show values only when the sensor has wifi connection
#define connectWIFI

// uncomment if you want to upload to AirGradient
#define airgradient

// uncomment to enable zabbix support
#define zabbix

#include <AirGradient.h>
#include <WiFiManager.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

#include <Wire.h>
#include "SSD1306Wire.h"

#ifdef hasTVOC
  #include "SGP30.h"
#endif // ifdef hasTVOC

#ifdef zabbix
  void createZabbixData(JsonArray *data, String key, String value);
#endif

AirGradient ag = AirGradient();

SSD1306Wire display(0x3c, SDA, SCL);

// change if you want to send the data to another server
String AGAPIROOT = "http://hw.airgradient.com/";

// Zabbix config

// Comma seperated IP address
IPAddress zabbixHost(192, 168, 1, 2); 
// Default 10051
uint16_t zabbixPort=10051;

void setup(){
  Serial.begin(9600);
  Serial.println("init");
  
  display.init();
  
  #ifdef flipScreen
    display.flipScreenVertically();
  #endif // ifdef flipScreen
  
  showTextRectangle("Init", String(ESP.getChipId(),HEX),true);

  #ifdef hasTVOC
    SGP30 SGP;
    SGP.begin();
  #endif // ifdef hasTVOC

  #ifdef hasPM
    ag.PMS_Init();
  #endif // ifdef hasPM

  #ifdef hasCO2
    ag.CO2_Init();
  #endif // ifdef hasCO2
  
  #ifdef hasSHT
    ag.TMP_RH_Init(0x44);
  #endif // ifdef hasSHT

  #ifdef connectWIFI
    connectToWifi();
  #endif  // ifdef connectWIFI
  delay(2000);
}

void loop(){
  uint16_t delaytime=60000;
  
  #ifdef hasTVOC
    SGP.measure(false);
  #endif // ifdef hasTVOC

  // create payload
  #ifdef airgradient
    DynamicJsonDocument payload(1024);
    payload["wifi"] = String(WiFi.RSSI());
  #endif // ifdef airgradient

  #ifdef zabbix
    DynamicJsonDocument zabbixpayload(1024);
    zabbixpayload["request"] = "sender data";
    JsonArray data = zabbixpayload.createNestedArray("data");
    createZabbixData(&data, "wifi", String(WiFi.RSSI()));
  #endif

  #ifdef hasPM
    int PM2 = ag.getPM2_Raw();
    #ifdef airgradient
      payload["pm02"] = String(PM2);
    #endif // ifdef airgradient
    #ifdef zabbix
      createZabbixData(&data, "pm02", String(PM2));
    #endif // ifdef zabbix
    showTextRectangle("PM2",String(PM2),false);
    delay(3000);
    delaytime = delaytime-3000;
  #endif // ifdef hasPM

  #ifdef hasTVOC
    int TVOC = SGP.getTVOC();
    #ifdef airgradient
      payload["tvoc"] = String(TVOC);
    #endif // ifdef airgradient
    #ifdef zabbix
      createZabbixData(&data, "tvoc", String(TVOC));
    #endif // ifdef zabbix
    showTextRectangle("TVOC",String(TVOC),false);
    delay(3000);
    delaytime = delaytime-3000;
  #endif // ifdef hasTVOC

  #ifdef hasCO2
    int CO2 = ag.getCO2_Raw();
    #ifdef airgradient
      payload["rco2"] = String(CO2);
    #endif // ifdef airgradient
    #ifdef zabbix
      createZabbixData(&data, "rco2", String(CO2));
    #endif // ifdef zabbix
    showTextRectangle("CO2",String(CO2),false);
    delay(3000);
    delaytime = delaytime-3000;
  #endif // ifdef hasCO2

  #ifdef hasSHT
    TMP_RH result = ag.periodicFetchData();
    #ifdef airgradient
      payload["atmp"] = String(result.t);
      payload["rhum"] = String(result.rh);
    #endif // ifdef airgradient
    #ifdef zabbix
      createZabbixData(&data, "atmp", String(result.t));
      createZabbixData(&data, "rhum", String(result.rh));
    #endif // ifdef zabbix
    showTextRectangle(String(result.t),String(result.rh)+"%",false);
    delay(3000);
    delaytime = delaytime-3000;
  #endif // ifdef hasSHT

  // send payload
  #ifdef connectWIFI
    #ifdef airgradient
      WiFiClient client;
      String output;
      serializeJson(payload, output);
      Serial.println(output);
      String POSTURL = AGAPIROOT + "sensors/airgradient:" + String(ESP.getChipId(),HEX) + "/measures";
      Serial.println(POSTURL);
      HTTPClient http;
      http.begin(client, POSTURL);
      http.addHeader("content-type", "application/json");
      int httpCode = http.POST(output);
      String response = http.getString();
      Serial.println(httpCode);
      Serial.println(response);
      http.end();
    #endif // ifdef airgradient
    #ifdef zabbix
      WiFiClient zabbixclient;
      String zabbixoutput;
      serializeJson(zabbixpayload, zabbixoutput);
      Serial.println(zabbixoutput);
      bool connected = (zabbixclient.connect(zabbixHost, zabbixPort) == 1);
      Serial.println(String(connected));
      if (connected) {
        char packet_header[] = "ZBXD\1";
        uint64_t payload_len = zabbixoutput.length();
        Serial.println(String("ZBX: ") + zabbixoutput);
        zabbixclient.write(packet_header, sizeof(packet_header) - 1);
        zabbixclient.write(reinterpret_cast<const char *>(&payload_len), sizeof(payload_len));
        zabbixclient.write(zabbixoutput.c_str(), payload_len);
        delay(10);
        delaytime = delaytime-10;
        while(zabbixclient.available()){
          String line = zabbixclient.readStringUntil('\n'); // <---- look for LF here instead of CR
          Serial.print(line);
        }
      }
      zabbixclient.stop();      
      
    #endif // ifdef zabbix
  #endif  // ifdef connectWIFI
  delay(delaytime);
}

// DISPLAY
void showTextRectangle(String ln1, String ln2, boolean small) {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  if (small) {
    display.setFont(ArialMT_Plain_16);
  } else {
    display.setFont(ArialMT_Plain_24);
  }
  #ifdef flipScreen
    display.drawString(32, 16, ln1);
    display.drawString(32, 36, ln2);
  #else // ifdef flipScreen
    display.drawString(32, 4, ln1);
    display.drawString(32, 24, ln2);
  #endif // ifdef flipScreen
  display.display();
}

// Wifi Manager
void connectToWifi(){
  WiFiManager wifiManager;
  //WiFi.disconnect(); //to delete previous saved hotspot
  String HOTSPOT = "AIRGRADIENT-"+String(ESP.getChipId(),HEX);
  wifiManager.setTimeout(120);
  if(!wifiManager.autoConnect((const char*)HOTSPOT.c_str())) {
      Serial.println("failed to connect and hit timeout");
      delay(3000);
      ESP.restart();
      delay(5000);
  }
}

// Zabbix Helpers
#ifdef zabbix
  void createZabbixData(JsonArray *data, String key, String value) {
    JsonObject block = data->createNestedObject();
    block["host"]  = "AIRGRADIENT-"+String(ESP.getChipId(),HEX);
    block["key"]   = key;
    block["value"] = value;
  }
#endif // ifdef zabbix
