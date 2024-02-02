
#include <Arduino.h>
#include <WiFi.h>
#include <AsyncMqttClient.h>
#include <ArduinoJson.h>

#include "utils.h"
#include "version.h"

static const uint16_t g_mqttKeepAliveTimeout = 15 * 60;   //15 minutes
static const int      g_mqttNumberOfReconnectAttempts = 20;

AsyncMqttClient mqttClient;

void onMqttConnect(bool sessionPresent);
void onMqttDisconnect(AsyncMqttClientDisconnectReason reason);
//void onMqttSubscribe(uint16_t packetId, uint8_t qos);
//void onMqttUnsubscribe(uint16_t packetId);
void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);
//void onMqttPublish(uint16_t packetId);
void handleMqttCmnd(const String& cmnd, const String& message);
void sendStatusToMQTT();

void onCmndGetList();
void onCmndScanDev();


const char topicPrefix[]  = "MiTemp/";
const char topicType[]  = "data";

static String g_wifiMacAddrString;
static String g_willTopic;


namespace internal {

String getMqttDeviceId()
{
    return "blebridge_" + g_wifiMacAddrString.substring(0, 6);   //Short name
}

} //namespace internal


void setupMqtt(const char* mqtt_broker, uint16_t mqtt_port)
{
    mqttClient.onConnect(onMqttConnect);
    mqttClient.onDisconnect(onMqttDisconnect);
    //mqttClient.onSubscribe(onMqttSubscribe);
    //mqttClient.onUnsubscribe(onMqttUnsubscribe);
    mqttClient.onMessage(onMqttMessage);
    //mqttClient.onPublish(onMqttPublish);
    mqttClient.setKeepAlive(g_mqttKeepAliveTimeout);
    mqttClient.setServer(mqtt_broker, mqtt_port);
}

bool connectToMqtt()
{
    if (!mqttClient.connected()) {

        if (g_wifiMacAddrString.isEmpty()) {
            g_wifiMacAddrString = utils::getMacAddrString();

            //Setup LWT topic
            g_willTopic = "tele/" + internal::getMqttDeviceId() + "/LWT";
            mqttClient.setWill(g_willTopic.c_str(), 1, true, "Offline");
        }

        Serial.println("Connecting to MQTT...");
        mqttClient.connect();

        for(int i = 0; i < g_mqttNumberOfReconnectAttempts && !mqttClient.connected(); i++) {
            delay(100);
        }
    }

    return mqttClient.connected();
}


void sendStatusOnlineMsg()
{
    String topic = "tele/" + internal::getMqttDeviceId() + "/STATUS";
    String payload = "Online";

    // send message, the Print interface can be used to set the message contents
    mqttClient.publish(topic.c_str(), 0, true, payload.c_str(), payload.length());
}


void sendDiscoveryToMQTT()
{
    JsonDocument doc;

    doc["ip"]  = WiFi.localIP().toString();
    doc["dn"]  = "BLE-Bridge";
    doc["mac"] = g_wifiMacAddrString;
    doc["sw"]  = String(APP_VERSION);
    doc["bd"]  = String(APP_BUILD_DATE);
    doc["n"]   = internal::getMqttDeviceId();

    String jsonData;
    serializeJson(doc, jsonData);

    String topic = "blebridge/discovery/";
    topic += g_wifiMacAddrString;
    topic += "/config";

    Serial.print("MQTT message: ");
    Serial.println(topic);  

    Serial.print("data:");
    Serial.println(jsonData);

    mqttClient.publish(topic.c_str(), 0, true, jsonData.c_str(), jsonData.length());
}

void sendSensorsListToMQTT(const String& sensorsList)
{
    String topic = "blebridge/discovery/";
    topic += g_wifiMacAddrString;
    topic += "/sensors";

    Serial.print("MQTT message: ");
    Serial.println(topic);  

    Serial.print("data:");
    Serial.println(sensorsList);

    mqttClient.publish(topic.c_str(), 0, true, sensorsList.c_str(), sensorsList.length());
}


// Tasmota sensor MQTT topic
//  tele/%topic%/SENSOR = {"Time":"2020-03-24T12:47:51",
//                          "LYWSD03-52680f":{"Temperature":21.1,"Humidity":58.0,"DewPoint":12.5,"Battery":100},
//                          "LYWSD02-a2fd09":{"Temperature":21.4,"Humidity":57.0,"DewPoint":12.5,"Battery":2},
//                          "MJ_HT_V1-d8799d":{"Temperature":21.4,"Humidity":54.6,"DewPoint":11.9},
//                          "TempUnit":"C"}


void sendSensorDataToMQTT(const String& devId, const String& jsonMessage)
{
    String topic = topicPrefix;
    topic += devId;
    topic += "/";
    topic += topicType;

    Serial.print("Sending message to topic: ");
    Serial.println(topic);        

    Serial.print("data:");
    Serial.println(jsonMessage);

    mqttClient.publish(topic.c_str(), 0, false, jsonMessage.c_str(), jsonMessage.length());
}

void onMqttConnect(bool sessionPresent) 
{
    Serial.println("Connected to MQTT.");
    Serial.print("Session present: ");
    Serial.println(sessionPresent);


    //TODO: subscribe on discovery topic...
    //      read config (maybe not good idea)
    //      apply config and start sensors scan

    String topic = "cmnd/";
    topic += internal::getMqttDeviceId();
    topic += "/#";
    mqttClient.subscribe(topic.c_str(), 2);  //TODO: why 2?

    mqttClient.publish(g_willTopic.c_str(), 0, true, "Online");

    sendDiscoveryToMQTT();
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
    Serial.println("Disconnected from MQTT.");

    //TODO:

}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)
{
    Serial.println("Publish received.");
    Serial.print("  topic: ");
    Serial.println(topic);
    Serial.print("  len: ");
    Serial.println(len);
    Serial.print("  index: ");
    Serial.println(index);
    Serial.print("  total: ");
    Serial.println(total);

    String front = utils::getStringFrontItem(topic, '/');
    if (front == "cmnd") {
        String cmnd = utils::getStringBackItem(topic, '/');
        if (cmnd == "getList") {
            onCmndGetList();
        }
        else if (cmnd == "scanDev") {
            onCmndScanDev();
        }
    }
}

void onCmndGetList()
{
    Serial.println("Cmnd: GetList..");




}

void onCmndScanDev()
{
    Serial.println("Cmnd: ScanDev..");





}
