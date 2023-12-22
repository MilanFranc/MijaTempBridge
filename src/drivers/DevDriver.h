#pragma once

#include <Arduino.h>

#include <NimBLEAddress.h>
#include <NimBLEClient.h>
#include <NimBLEDevice.h>

class NimBLEAdvertisedDevice;
class NimBLEClient;
class MyBLEDevice;

class DevDriver
{
public:
    DevDriver();

    static DevDriver* createDriver(NimBLEAdvertisedDevice* pAdvertDevice);
    void attachBLEDevice(MyBLEDevice* dev);

    virtual bool readData(NimBLEClient* pClient) = 0;

    //get data summary in json...
    virtual String getData() = 0;

    const char* getTemp() const { return m_temperatureValue.c_str(); }
    const char* getHum() const { return m_humidityValue.c_str(); }

    void clearUpdatedFlag() { m_updated = false; }
    bool isUpdated() const { return m_updated; }

protected:
    void setTemp(const char* value) { m_temperatureValue = value; }
    void setHum(const char* value) { m_humidityValue = value; }

    bool isTempSet() const { return !m_temperatureValue.isEmpty(); }
    bool isHumSet() const { return !m_humidityValue.isEmpty(); }

private:
    MyBLEDevice* m_BLEDevice;

    bool m_updated;
    unsigned long m_timestamp;
    String m_temperatureValue;
    String m_humidityValue;
};

bool readNotifyData(NimBLEClient* pClient, const NimBLEUUID& service, const NimBLEUUID& characteristic, NimBLEAttValue& readData);

