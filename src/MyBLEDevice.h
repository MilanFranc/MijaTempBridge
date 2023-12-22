#ifndef MITEMP_DEV_H
#define MITEMP_DEV_H

#include <Arduino.h>

class NimBLEAdvertisedDevice;
class NimBLEClient;
class DevDriver;

class MyBLEDevice
{
public:
    static MyBLEDevice* createDev(NimBLEAdvertisedDevice* pAdvertDevice);
    ~MyBLEDevice() = default;

    const char* addr() const { return m_BLEAddr.c_str(); }
    const char* name() const { return m_name.c_str(); }
    const char* devId() const { return m_devId.c_str(); }

    bool readBattLevel(NimBLEClient* pClient);
    int16_t getBatteryLevel() const { return m_batteryLevel; }

    bool connectAndReadDevice();

    String getData();

private:
    MyBLEDevice(int devType);

private:
    String m_BLEAddr;
    String m_devId;
    String m_name;

    bool m_hasBattery;
    int16_t m_batteryLevel;
    int  m_rssiLevel;
    DevDriver* m_driver;

};

#endif
