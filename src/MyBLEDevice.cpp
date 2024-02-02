#include "MyBLEDevice.h"
#include "drivers/DevDriver.h"
#include "utils.h"

#include <NimBLEAddress.h>
#include <NimBLEClient.h>
#include <NimBLEDevice.h>

#include <ArduinoJson.h>



    #if 0 //temp remove        
            NimBLERemoteDescriptor* pDesc = pChar->getDescriptor(NimBLEUUID((uint16_t)0x2902));
            if (pDesc) {
                Serial.println("Desc:" + String(pDesc->toString().c_str()));

                pDesc->writeValue(nullptr, 0, true);
        
                Serial.println("Val:" + String(pDesc->readValue().c_str()));
    //          uint16_t temp = pDesc->readValue<uint16_t>();
    //          Serial.println("Val:" + String(temp));
            }
    #endif          



String MyBLEDevice::devId() const
{
    return utils::removeColons(m_addr.toString());
}

MyBLEDevice* MyBLEDevice::createDev(NimBLEAdvertisedDevice* pAdvertDevice)
{
    DevDriver* pDriver = DevDriver::createDriver(pAdvertDevice);
    if (pDriver == nullptr) {
        return nullptr;
    }

    assert(pDriver != nullptr);

    NimBLEAddress addr = pAdvertDevice->getAddress();

    MyBLEDevice* device = new MyBLEDevice(1);
    device->m_addr = addr;
    device->m_name = pAdvertDevice->getName();

    device->m_driver = pDriver;
    pDriver->attachBLEDevice(device);

    device->m_hasBattery = pAdvertDevice->isAdvertisingService(NimBLEUUID((uint16_t)0x180F));
    Serial.println("Battery:" + String(device->m_hasBattery));

    return device;
}

MyBLEDevice::MyBLEDevice(int devType)
    : m_hasBattery(false), m_batteryLevel(0), m_rssiLevel(0), m_driver(nullptr)
    , m_connectCount(0), m_failedConnectCount(0)
{
}

bool MyBLEDevice::readBattLevel(NimBLEClient* pClient)
{
    NimBLERemoteService* pService = pClient->getService(NimBLEUUID((uint16_t)0x180F));
    if (pService != nullptr) {
        NimBLERemoteCharacteristic* pChar = pService->getCharacteristic(NimBLEUUID((uint16_t)0x2a19));
        if (pChar != nullptr) {
            uint8_t val = pChar->readValue<uint8_t>();
            m_batteryLevel = static_cast<int16_t>(val);
            return true;
        }
    }

    m_batteryLevel = -1;
    return false;
}

bool MyBLEDevice::connectAndReadDevice(NimBLEClient* pClient)
{
    m_rssiLevel = pClient->getRssi();

    if (m_hasBattery) {
        if (!readBattLevel(pClient)) {
            Serial.println("Read battery failed!");
        }
    }

    bool bUpdated = false;
    if (m_driver != nullptr) {
        bUpdated = m_driver->readData(pClient);

        //TODO: handle somehow data ??

    }
    return bUpdated;
}

String MyBLEDevice::getData()
{
    if (m_driver == nullptr) {
        return String();
    }

    JsonDocument doc;

    //TODO: sensor data...
    doc["temp"] = m_driver->getTemp();
    doc["humidity"] = m_driver->getHum();
    if (m_hasBattery && this->getBatteryLevel() != -1) {
        doc["batt"] = this->getBatteryLevel();
    }
    doc["rssi"] = m_rssiLevel;

    String jsonData;
    serializeJson(doc, jsonData);

    return jsonData;
}
