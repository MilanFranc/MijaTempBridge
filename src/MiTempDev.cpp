#include "MiTempDev.h"


MiTempDev::MiTempDev(const char* pBLEAddr)
    : m_updated(false), m_batteryLevel(0), m_rssiLevel(0)
{
    m_BLEAddr = String(pBLEAddr);
}

const char* MiTempDev::addr()
{
    return m_BLEAddr.c_str();
}

void MiTempDev::setDevId(const char* pDevId)
{
    m_devId = pDevId;
}

const char* MiTempDev::devId()
{
    return m_devId.c_str();
}

void MiTempDev::setName(const char* name)
{
    m_name = String(name);
}

void MiTempDev::setBatteryLevel(uint16_t level)
{
    m_batteryLevel = level;
}

void MiTempDev::setRSSILevel(int level)
{
    m_rssiLevel = level;
}

void MiTempDev::clearUpdatedFlag()
{
    m_updated = false;
}

void MiTempDev::parseBLEData(const uint8_t* data, size_t length)
{
    String temp;
    temp.reserve(8);

    m_timestamp = millis();

    //TODO: parssing: T=23.5 H=52.4
    int idx = 0;
    if (length > 2 && data[idx] == 'T' && data[idx+1] == '=') {
        idx += 2;

        for(; idx < length; idx++) {
            if (data[idx] == ' ')
            break;

            temp += (char)data[idx];
        }

        m_temperatureValue = temp;

        idx++;  //skip space
    }

    if ((length - idx) > 2 && data[idx] == 'H' && data[idx+1] == '=') {
        idx += 2;

        temp = "";
        for(; idx < length; idx++) {
            if (data[idx] == ' ')
            break;

            temp += (char)data[idx];
        }

        m_humidityValue = temp;
        m_updated = true;
    }
}

const char* MiTempDev::getTemp()
{
    return m_temperatureValue.c_str();
}

const char* MiTempDev::getHumidity()
{
    return m_humidityValue.c_str();
}

