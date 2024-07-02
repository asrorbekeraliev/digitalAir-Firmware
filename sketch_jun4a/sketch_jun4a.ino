#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "DHT.h"
#include <PMS.h>
#include <SoftwareSerial.h>

#define zoneId 1

// Serial communication for PMS5003 sensor
SoftwareSerial pmsSerial(13, 15); // D7-Rx  D8-Tx

// PMS configuration
PMS pms(pmsSerial);
PMS::DATA data;

// WiFi credentials
const char* ssid = "Home-WiFi";
const char* password = "19701974";

// DHT sensor configurations
#define DHTPIN 4  // D2 pin of NodeMCU
#define DHTTYPE DHT11   // DHT 11
DHT dht(DHTPIN, DHTTYPE);

void connectToWiFi() {
    WiFi.begin(ssid, password);
    Serial.println("");
    Serial.print("Connecting");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
}

// Function for HTTP POST request
int postAPI(String JSONMessage) {
    String url = "https://backend.helptech.uz/api/v1/parameter/add";
    WiFiClientSecure client;
    HTTPClient http;
    client.setInsecure();
    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");
    int httpCode = http.POST(JSONMessage);   // Send the request
    String payload = http.getString();       // Get the response payload

    Serial.println(httpCode);   // Print HTTP return code
    Serial.println(payload);    // Print request response payload

    http.end();  // Close connection
    return httpCode;
}

String makeJSON(double temperature, double humidity, double heatIndex, int pm1_0, int pm2_5, int pm10) {
    String JSONMessage;
    StaticJsonDocument<128> doc;

    doc["temperature"] = temperature;
    doc["humidity"] = humidity;
    doc["heatIndex"] = heatIndex;
    doc["pm1_0"] = pm1_0;
    doc["pm2_5"] = pm2_5;
    doc["pm10"] = pm10;
    doc["zoneId"] = zoneId;

    serializeJson(doc, JSONMessage);
    return JSONMessage;
}

void setup() {
    // Initialize Serial Monitor
    Serial.begin(9600);

    // PMS5003 sensor baud rate is 9600
    pmsSerial.begin(9600);

    connectToWiFi();
    delay(1000);
    dht.begin();
    delay(1000);
}

void loop() {

  int attempts = 0;
  
  
  int pm1_0, pm2_5, pm10;    

  A:  if (pms.read(data)) {
        pm1_0 = data.PM_AE_UG_1_0;
        pm2_5 = data.PM_AE_UG_2_5;
        pm10 = data.PM_AE_UG_10_0;
    } else {
        goto A;
    }

    float h = dht.readHumidity();
    float t = dht.readTemperature();

    // Check if any reads failed and exit early (to try again).
    if (isnan(h) || isnan(t)) {
        Serial.println(F("Failed to read from DHT sensor!"));
        return;
    }

    // Compute heat index in Celsius (isFahreheit = false)
    float hic = dht.computeHeatIndex(t, h, false);

    Serial.print(F("Humidity: "));
    Serial.print(h);
    Serial.print(F("%  Temperature: "));
    Serial.print(t);
    Serial.print(F("°C "));
    Serial.print(F("Heat index: "));
    Serial.print(hic);
    Serial.print(F("°C "));

    String JSONMessage = makeJSON(t, h, hic, pm1_0, pm2_5, pm10);
    Serial.println(JSONMessage);

    // HTTP POST the JSON message
    int result = postAPI(JSONMessage);


    // Check the response
    if(result == -1){
      attempts++;
      if(attempts >= 5){
        ESP.deepSleep(3600e6);
      }
      return;
    }

    delay(1000);

    // Go to deep sleep for 1 hour (3600 seconds, 3600 * 1,000,000 microseconds)
    ESP.deepSleep(3600e6);  // 1 hour in microseconds
}
