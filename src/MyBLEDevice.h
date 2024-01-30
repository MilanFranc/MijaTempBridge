#ifndef MITEMP_DEV_H
#define MITEMP_DEV_H

#include <Arduino.h>
#include <NimBLEAddress.h>

class NimBLEAdvertisedDevice;
class NimBLEClient;
class DevDriver;

class MyBLEDevice
{
public:
    static MyBLEDevice* createDev(NimBLEAdvertisedDevice* pAdvertDevice);
    ~MyBLEDevice() = default;

    NimBLEAddress native_addr() const { return m_addr; }
    std::string addr() const { return m_addr.toString(); }
    std::string name() const { return m_name; }
    String devId() const;

    bool readBattLevel(NimBLEClient* pClient);
    int16_t getBatteryLevel() const { return m_batteryLevel; }

    bool connectAndReadDevice(NimBLEClient* pClient);

    String getData();

    uint16_t connectCount() const { return m_connectCount; }
    uint16_t failedConnectCount() const { return m_failedConnectCount; }

    void incConnectCount() { m_connectCount++; }
    void incFailedCount() { m_failedConnectCount++; }

private:
    MyBLEDevice(int devType);

private:
    NimBLEAddress m_addr;
    std::string   m_name;

    bool m_hasBattery;
    int16_t m_batteryLevel;
    int16_t m_rssiLevel;
    DevDriver* m_driver;

    uint16_t  m_connectCount;
    uint16_t  m_failedConnectCount;

};

#endif
