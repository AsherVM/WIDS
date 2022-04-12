// 1152000 Baud

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>

#include <set>
#include <string>
#include "./functions.h"
#include "./mqtt.h"

#define MAXDEVICES 100
#define JBUFFER 80 + (MAXDEVICES * 60) // Times two for the NearbyAPs array, + 80 for general properties

#define mySSID "WINS-Net" // RPi AP
#define myPASSWORD "UpdateThis!"


uint32_t ESPSensorID = ESP.getChipId();

DynamicJsonDocument jsonDoc(JBUFFER);
char jsonString[JBUFFER];

unsigned int channel = 1;
int usedChannels[15];

void setup() { // Configure serial outspeed and put wifi chip in monitor mode
  Serial.begin(115200);
  wifi_set_opmode(STATION_MODE);
  wifi_set_channel(channel);
  wifi_promiscuous_enable(0);
  wifi_set_promiscuous_rx_cb(promisc_cb_old);   // On 
  wifi_promiscuous_enable(1);
}

void connectToWiFi() {
  delay(10);
  
  WiFi.mode(WIFI_STA);
  
  // scan nearby nets for geo 
  jsonDoc.clear();
  int nearbyAPsCount = WiFi.scanNetworks();
  JsonArray nearbyAPs = jsonDoc.createNestedArray("nearbyAPs");
  for (int i = 0; i < nearbyAPsCount; ++i) {
        JsonObject nearbyAP = nearbyAPs.createNestedObject();
        nearbyAP["MAC"] =  WiFi.BSSIDstr(i);
        nearbyAP["RSSI"] = WiFi.RSSI(i);
        nearbyAP["channel"] = WiFi.channel(i);
  }
  
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(mySSID);

  WiFi.begin(mySSID, myPASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void MQTTConnect() {
  // Connect to WiFi and publish to MQTT broker
  wifi_promiscuous_enable(0);
  connectToWiFi();
  client.setServer(mqttServer, 1883);
  while (!client.connected()) {
    Serial.println("Connecting to MQTT...");

    if (client.connect("ESP32Client", "", "" )) {
      Serial.println("connected");
    } else {
      Serial.print("failed with state ");
      Serial.println(client.state());
    }
    yield();
  }
}

void sendDevices() {
  MQTTConnect();

  jsonDoc["SensorID"] = ESPSensorID;
  jsonDoc["time"] = millis();
  JsonArray nearbyDevices = jsonDoc.createNestedArray("devices");

  // Add Clients
  for(clientinfo nearbyDevice : clients_known) {
    if (nearbyDevice.channel != 0) {
      JsonObject detectedDevice = nearbyDevices.createNestedObject();
      detectedDevice["MAC"] = formatMac1(nearbyDevice.station);
      detectedDevice["RSSI"] = nearbyDevice.rssi;
      detectedDevice["channel"] = nearbyDevice.channel;
    }
  }

  jsonDoc["nDevices"] = nearbyDevices.size();
  Serial.println();
  Serial.printf("Devices seen: %02d\n", nearbyDevices.size());

  serializeJson(jsonDoc, jsonString);

  //if (client.publish((char*)MQTT_TOPIC.c_str(), jsonString) == 1) Serial.println("\nSuccessfully published");
  if (client.publish("Sniffer/", jsonString) == 1) Serial.println("\nSuccessfully published");
  else {
    Serial.println();
    Serial.println("!!!!! Not published. Please add #define MQTT_MAX_PACKET_SIZE 2048 at the beginning of PubSubClient.h file");
    Serial.println();
  }

  client.loop();
  client.disconnect ();
  
  delay(100);

  clients_known_count = 0;

  wifi_promiscuous_enable(1);
}

void loop() {
  channel = 1;
  wifi_set_channel(channel);

  while (true) {
    nothing_new++;
    if (nothing_new > 500) { // After 500 iterations, switch to next channel   
      nothing_new = 0;
      channel++;
      if (channel == 15) break; // After scanning all 14 channels for 500ms, escape while loop and purgeDevices()
      wifi_set_channel(channel);
    }
    
    delay(1); //  Processing timeslice for NONOS SDK! No delay(0) yield()
  }
  sendDevices();
}
