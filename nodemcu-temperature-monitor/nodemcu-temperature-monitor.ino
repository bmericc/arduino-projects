/**
   BasicHTTPClient.ino

    Created on: 24.05.2015

*/

#include <Arduino.h>

#include <ESP8266WiFi.h>
//#include <ESP8266WiFiMulti.h>

#include <ESP8266HTTPClient.h>

#include <OneWire.h>
#include <DallasTemperature.h>

#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include "WiFiManager.h" //https://github.com/tzapu/WiFiManager

#define USE_SERIAL Serial

// Pin use
#define ONEWIRE D1 //pin to use for One Wire interface

// User defined variables for Exosite reporting period and averaging samples
#define REPORT_TIMEOUT 30000 //milliseconds period for reporting to Exosite.com
#define SENSOR_READ_TIMEOUT 5000 //milliseconds period for reading sensors in loop

// Set up which Arduino pin will be used for the 1-wire interface to the sensor
OneWire oneWire(ONEWIRE);
DallasTemperature sensors(&oneWire);

//ESP8266WiFiMulti WiFiMulti;
WiFiManager wifiManager;

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
}

void setup() {

  USE_SERIAL.begin(115200);
  // USE_SERIAL.setDebugOutput(true);

  USE_SERIAL.println();
  USE_SERIAL.println();
  USE_SERIAL.println();

  for (uint8_t t = 4; t > 0; t--) {
    USE_SERIAL.printf("[SETUP] WAIT %d...\n", t);
    USE_SERIAL.flush();
    delay(1000);
  }

  sensors.begin();

  wifiManager.setAPCallback(configModeCallback);

  if(!wifiManager.autoConnect("AutoConnectAP")) {
      Serial.println("failed to connect and hit timeout");
      //reset and try again, or maybe put it to deep sleep
      ESP.reset();
      delay(1000);
  } 

 // WiFi.mode(WIFI_STA);
  //WiFiMulti.addAP("SSID", "PASSWD");

}

void loop() {
  static float tempF;
  static float tempC;
  static unsigned long sendPrevTime = 0;
  static unsigned long sensorPrevTime = 0; 
  char buffer[7];

  
 // Read sensor every defined timeout period
  if (millis() - sensorPrevTime > SENSOR_READ_TIMEOUT) {
    Serial.println();
    Serial.println("Requesting temperature...");
    sensors.requestTemperatures(); // Send the command to get temperatures
    tempC = sensors.getTempCByIndex(0);
    Serial.print("Celsius:    ");
    Serial.print(tempC);
    Serial.println(" C ..........DONE");
    tempF = DallasTemperature::toFahrenheit(tempC);
    Serial.print("Fahrenheit: ");
    Serial.print(tempF);
    Serial.println(" F ..........DONE");
     
    sensorPrevTime = millis();
  }

  
  // wait for WiFi connection
  if ((WiFi.status() == WL_CONNECTED)) {

    HTTPClient http;

    if (millis() - sendPrevTime > REPORT_TIMEOUT) {

      
      USE_SERIAL.print("[HTTP] begin...\n");
      // configure traged server and url
      //http.begin("https://192.168.1.12/test.html", "7a 9c f4 db 40 d3 62 5a 6e 21 bc 5c cc 66 c8 3e a1 45 59 38"); //HTTPS
      //http.begin("http://192.168.0.34/test.php"); //HTTP
      http.begin("http://secure.prj.be/temperature-monitor/nodemcu-data.php"); //HTTP

      String tempValueC = dtostrf(tempC, 1, 2, buffer);
      String tempValueF = dtostrf(tempF, 1, 2, buffer);
      String macString = WiFi.macAddress();

      USE_SERIAL.print("[HTTP] POST...\n");
      
      http.addHeader("Content-Type", "application/x-www-form-urlencoded");
      int httpCode = http.POST("tempD=*D"+tempValueC+"*&tempA=*A"+tempValueF+"*&macString="+macString+"&current="+tempValueC);
      http.writeToStream(&Serial);
      http.end();     
  
      // httpCode will be negative on error
      if (httpCode > 0) {
        // HTTP header has been send and Server response header has been handled
        USE_SERIAL.printf("[HTTP] POST... code: %d\n", httpCode);
  
        // file found at server
        if (httpCode == HTTP_CODE_OK) {
          String payload = http.getString();
          USE_SERIAL.println(payload);
        }
      } else {
        USE_SERIAL.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
      }
  
      http.end();
  
   
      sendPrevTime = millis(); //reset report period timer
      Serial.println("done sending.");
    }
    
  }

  delay(300000);
}

