#include <AirGradient.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiClient.h>

#include <Wire.h>
#include "SSD1306Wire.h"

AirGradient ag = AirGradient();

const char* deviceId = ""; // optional

const bool hasPM = true;
const bool hasCO2 = true;
const bool hasSHT = true;

const char* ssid = "TODO";
const char* password = "TODO";

const int port = 9926;

long lastUpdate;
const int updateFrequency = 5000;

// Config End
SSD1306Wire display(0x3c, SDA, SCL);
ESP8266WebServer server(port);

void setup() {
  Serial.begin(9600);

  // Initialize display.
  display.init();
  //display.flipScreenVertically();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  Serial.println("Initializing " + String(ESP.getChipId(), HEX));
  showLines("Initializing", String(ESP.getChipId(), HEX), "", "");

  // Enable sensors.
  if (hasPM) ag.PMS_Init();
  if (hasCO2) ag.CO2_Init();
  if (hasSHT) ag.TMP_RH_Init(0x44);

  // Enable wifi.
  WiFi.begin(ssid, password);
  Serial.println("Connecting to " + String(ssid)  + "...");
  showLines("Connecting to ", String(ssid), "", "");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  Serial.println("IP address: " + WiFi.localIP().toString());
  Serial.println("MAC address: " + WiFi.macAddress());
  showLines("IP address:", WiFi.localIP().toString(), "MAC address: ", WiFi.macAddress());

  // Enable server.
  server.on("/", handleRequest);
  server.on("/metrics", handleRequest);
  server.onNotFound(handleNotFound);
  server.begin();

  delay(updateFrequency);
}

void handleRequest() {
  server.send(200, "text/plain", generateMetrics());
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/html", message);
}

void loop() {
  long t = millis();
  server.handleClient();
  updateScreen(t);
}

String generateMetrics() {
  String message = "";
  String idString = "{id=\"" + String(deviceId) + "\",mac=\"" + WiFi.macAddress() + "\"}";

  if (hasSHT) {
    TMP_RH stat = ag.periodicFetchData();
    if (stat.error == SHT3XD_NO_ERROR) {
      message += "# HELP atmp Temperature value, in degrees celsius\n";
      message += "# TYPE atmp gauge\n";
      message += "atmp";
      message += idString;
      message += String(stat.t);
      message += "\n";

      message += "# HELP rhum Relative humidity value, in %\n";
      message += "# TYPE rhum gauge\n";
      message += "rhum";
      message += idString;
      message += String(stat.rh);
      message += "\n";
    }
  }

  if (hasPM) {
    int stat = ag.getPM2_Raw();
    if (stat > 0) {
      message += "# HELP pm02 Particulate matter PM2.5 value, in μg/m3\n";
      message += "# TYPE pm02 gauge\n";
      message += "pm02";
      message += idString;
      message += String(stat);
      message += "\n";
    }
  }

  if (hasCO2) {
    int stat = ag.getCO2_Raw();
    if (stat > 0) {
      message += "# HELP rco2 CO2 value, in ppm\n";
      message += "# TYPE rco2 gauge\n";
      message += "rco2";
      message += idString;
      message += String(stat);
      message += "\n";
    }
  }

  return message;
}

void showLines(String line1, String line2, String line3, String line4) {
  display.clear();
  display.drawString(32, 0, line1);
  display.drawString(32, 12, line2);
  display.drawString(32, 24, line3);
  display.drawString(32, 36, line4);
  display.display();
}

void updateScreen(long now) {
  if ((now - lastUpdate) > updateFrequency) {
    String tmp, hum, pm2, co2 = "";

    if (hasSHT) {
      TMP_RH stat = ag.periodicFetchData();
      if (stat.error == SHT3XD_NO_ERROR) {
        tmp = String(stat.t) + " °C";
        hum = String(stat.rh) + " %RH";
      }
    }

    if (hasPM) {
      int stat = ag.getPM2_Raw();
      if (stat > 0) {
        pm2 = String(stat) + " μg/m3";
      }
    }

    if (hasCO2) {
      int stat = ag.getCO2_Raw();
      if (stat > 0) {
        co2 = String(stat) +  " ppm";
      }
    }

    showLines(tmp, hum, pm2, co2);
    lastUpdate = millis();
  }
}
