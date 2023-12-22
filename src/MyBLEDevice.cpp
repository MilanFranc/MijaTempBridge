#include "MyBLEDevice.h"
#include "drivers/DevDriver.h"
#include "utils.h"

#include <NimBLEAddress.h>
#include <NimBLEClient.h>
#include <NimBLEDevice.h>

#include <ArduinoJson.h>

uint32_t g_bleConnectTimeout = 50;


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



namespace internal {

    NimBLEClient* connectToBLEDevice(const char* pBLEAddr, uint32_t timeout)
    {
        NimBLEClient* pClient = nullptr;

        NimBLEAddress addr(pBLEAddr);
        if (NimBLEDevice::getClientListSize() > 0) {
            /** Special case when we already know this device, we send false as the
             *  second argument in connect() to prevent refreshing the service database.
             *  This saves considerable time and power.
             */
            pClient = NimBLEDevice::getClientByPeerAddress(addr);
            if (pClient) {
                Serial.println("Connecting to " + String(pBLEAddr) + " - exist client");

                if (pClient->isConnected()) {
                    Serial.println("Disconnect client");
                    pClient->disconnect();
                    delay(100);
                }

                pClient->setConnectTimeout(timeout);
                if (!pClient->connect(addr, false)) {
                    Serial.println("Connect failed");
                    assert(pClient->isConnected() == false && "IS connected!!!");

                    NimBLEDevice::deleteClient(pClient);
                    return nullptr;
                }
                Serial.println("Reconnected client");
            }
            else {
                pClient = NimBLEDevice::getDisconnectedClient();
            }
        }

        if (!pClient) {

            if (NimBLEDevice::getClientListSize() >= NIMBLE_MAX_CONNECTIONS) {
                Serial.println("Max clients reached - no more connections available");
                return nullptr;
            }

            pClient = NimBLEDevice::createClient(addr);

            Serial.println("Connecting to " + String(addr.toString().c_str()) + " - new client");
            pClient->setConnectTimeout(timeout);
            
            if (!pClient->connect()) {

                /** Created a client but failed to connect, don't need to keep it as it has no data */
                NimBLEDevice::deleteClient(pClient);
                Serial.println("Failed to connect, deleted client");
                return nullptr;
            }
        }

        if (!pClient->isConnected()) {
            Serial.println("Connecting to " + String(pBLEAddr));

            pClient->setConnectTimeout(timeout);
            if (!pClient->connect(addr)) {
                Serial.println("Failed to connect");
                return nullptr;
            }
        }
        return pClient;
    }
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
    device->m_BLEAddr = String(addr.toString().c_str());
    device->m_devId = utils::removeColons(addr.toString());
    device->m_name = String(pAdvertDevice->getName().c_str());
    device->m_driver = pDriver;
    pDriver->attachBLEDevice(device);

    device->m_hasBattery = pAdvertDevice->isAdvertisingService(NimBLEUUID((uint16_t)0x180F));
    Serial.println("Battery:" + String(device->m_hasBattery));

    return device;
}

MyBLEDevice::MyBLEDevice(int devType)
    : m_hasBattery(false), m_batteryLevel(0), m_rssiLevel(0), m_driver(nullptr)
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

bool MyBLEDevice::connectAndReadDevice()
{
    NimBLEAddress addr(this->addr());
    NimBLEClient* pClient = internal::connectToBLEDevice(this->addr(), g_bleConnectTimeout);
    if (pClient == nullptr) {
        //TODO: create some counter for failed connects...

        return false;
    }

    Serial.println("Name:" + String(this->name()));
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

    delay(100);
    Serial.println("Close.");    
    pClient->disconnect();

    return bUpdated;
}

String MyBLEDevice::getData()
{
    if (m_driver == nullptr) {
        return String();
    }

    StaticJsonDocument<200> doc;

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
