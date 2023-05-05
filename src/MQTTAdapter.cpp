
#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoMqttClient.h>
#include <ArduinoJson.h>

#include "arduino_secrets.h"

#include "MiTempDev.h"
#include "utils.h"


WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

const char broker[] = MQTT_BROKER_ADDR;
int        port     = 1883;
const char topicPrefix[]  = "MiTemp/";
const char topicType[]  = "data";

static String g_wifiMacAddrString;

namespace internal {

String getMqttDeviceId()
{
    return "blebridge_" + g_wifiMacAddrString.substring(0, 6);   //Short name
}

} //namespace internal



bool connectToMQTT()
{
    // You can provide a unique client ID, if not set the library uses Arduino-millis()
    // Each client must have a unique client ID
    // mqttClient.setId("clientId");

    // You can provide a username and password for authentication
    // mqttClient.setUsernamePassword("username", "password");

    if (!mqttClient.connected()) {

        g_wifiMacAddrString = utils::getMacAddrString();        

        Serial.print("Connect to the MQTT broker: ");
        Serial.println(broker);

        if (!mqttClient.connect(broker, port)) {
            Serial.print("MQTT connection failed! Error code = ");
            Serial.println(mqttClient.connectError());
            return false;
        }

        Serial.println("Connected to the MQTT broker!");
        Serial.println();
    }
    else {
        Serial.println("Still connected to the MQTT broker!");
    }

    return true;
}

void sendStatusOnlineMsg()
{
    String topic = "tele/" + internal::getMqttDeviceId() + "/STATUS";
    String payload = "Online";

    // send message, the Print interface can be used to set the message contents
    mqttClient.beginMessage(topic, static_cast<unsigned long>(payload.length()));
    mqttClient.print(payload);
    mqttClient.endMessage();
}

void sendMiTempDataToMQTT(MiTempDev* pDev)
{
    StaticJsonDocument<200> doc;
    doc["temp"] = pDev->getTemp();
    doc["humidity"] = pDev->getHumidity();
    doc["batt"] = pDev->getBatteryLevel();
    doc["rssi"] = pDev->getRSSILevel();

    String jsonData;
    serializeJson(doc, jsonData);

    String topic = topicPrefix;
    topic += pDev->devId();
    topic += "/";
    topic += topicType;

    Serial.print("MQTT topic: ");
    Serial.println(topic);        
    Serial.print("MQTT data:");
    Serial.println(jsonData);

    // send message, the Print interface can be used to set the message contents
    mqttClient.beginMessage(topic, static_cast<unsigned long>(jsonData.length()));

    mqttClient.print(jsonData);
    mqttClient.endMessage();
}

void pollMQTT()
{
    mqttClient.poll();
}

void flushMQTT()
{
    mqttClient.flush();
}

