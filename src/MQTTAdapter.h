#ifndef MQTT_ADAPTER_H
#define MQTT_ADAPTER_H

class MyBLEDevice;

bool connectToMQTT();

void sendStatusOnlineMsg();
void sendMiTempDataToMQTT(MyBLEDevice* pDev);

void pollMQTT();
void flushMQTT();


#endif //MQTT_ADAPTER_H
