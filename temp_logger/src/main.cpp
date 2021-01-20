
#include <DallasTemperature.h>
#include <ESP8266WiFi.h>
#include <OneWire.h>
#include <PubSubClient.h>

#include "Arduino.h"

// WiFi
const char* ssid = "telenet-E24A9";
const char* wifi_password = "If you could not show your password, that would be great";

const char* mqtt_server = "octopi.local";
const int mqtt_port = 1883;
const char* mqtt_client_id = "temp_logger";
const char* mqtt_topic = "house/roof/temperature";

WiFiClient wifiClient;
PubSubClient client(mqtt_server, mqtt_port, wifiClient);

const int oneWireBus = 2;

OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);

void setup() {
  Serial.begin(115200);
  while (!Serial) {
  }

  Serial.print("timestamp: ");
  Serial.println(millis());
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, wifi_password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (client.connect(mqtt_client_id)) {
    Serial.println("Connected to MQTT broker");
  } else {
    Serial.println("Connection to MQTT broker failed");
  }

  //   sensors.begin();
  //   sensors.requestTemperatures();
  //   float temperatureC = sensors.getTempCByIndex(0);
  //   Serial.print(temperatureC);
  //   Serial.println("ºC");

  //   ESP.deepSleep(20e6);  // 20e6 is 20 microseconds
}

void loop() {
  sensors.requestTemperatures();
  float temperatureC = sensors.getTempCByIndex(0);
  Serial.print(temperatureC);
  Serial.println("ºC");

  String tempString = String(temperatureC);
  client.connect(mqtt_client_id);
  client.publish(mqtt_topic, tempString.c_str());

  delay(5000);
}
