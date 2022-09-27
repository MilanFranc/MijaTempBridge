#ifndef MITEMP_DEV_H
#define MITEMP_DEV_H

#include <Arduino.h>

class MiTempDev
{
public:
    MiTempDev(const char* pBLEAddr);
    const char* addr();

    void setDevId(const char* pDevId);
    const char* devId();

    void setName(const char* name);
    void setBatteryLevel(uint16_t level);

    void setBLEData(const uint8_t* data, size_t length);

    const char* getTemp();
    const char* getHumidity();
    uint16_t    getBatteryLevel();

private:
    String m_BLEAddr;
    String m_devId;
    String m_name;

    unsigned long m_timestamp;
    String m_temperatureValue;
    String m_humidityValue;
    uint16_t m_batteryLevel;
};

#endif
