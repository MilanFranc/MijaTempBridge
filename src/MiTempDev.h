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
    const char* name() const { return m_name.c_str(); }
    void setBatteryLevel(uint16_t level);
    void setRSSILevel(int level);

    void clearUpdatedFlag();
    void parseBLEData(const uint8_t* data, size_t length);
    bool isUpdated() const { return m_updated; }

    const char* getTemp();
    const char* getHumidity();
    uint16_t    getBatteryLevel() const { return m_batteryLevel; }
    int         getRSSILevel() const { return m_rssiLevel; }

private:
    String m_BLEAddr;
    String m_devId;
    String m_name;

    bool m_updated;
    unsigned long m_timestamp;
    String m_temperatureValue;
    String m_humidityValue;
    uint16_t m_batteryLevel;
    int  m_rssiLevel;
};

#endif
