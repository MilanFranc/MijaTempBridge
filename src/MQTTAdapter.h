#ifndef MQTT_ADAPTER_H
#define MQTT_ADAPTER_H

#include <Arduino.h>


void setupMqtt(const char* mqtt_broker, uint16_t mqtt_port);
bool connectToMqtt();

void sendSensorDataToMQTT(const String& devId, const String& jsonMessage);
void sendSensorsListToMQTT(const String& sensorsList);


#endif //MQTT_ADAPTER_H
