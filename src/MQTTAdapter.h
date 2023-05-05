#ifndef MQTT_ADAPTER_H
#define MQTT_ADAPTER_H

class MiTempDev;

bool connectToMQTT();

void sendStatusOnlineMsg();
void sendMiTempDataToMQTT(MiTempDev* pDev);

void pollMQTT();
void flushMQTT();


#endif //MQTT_ADAPTER_H
