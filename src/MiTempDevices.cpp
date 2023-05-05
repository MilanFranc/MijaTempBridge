
#include <NimBLEAddress.h>
#include <NimBLEClient.h>
#include <NimBLEDevice.h>
#include <vector>
#include <ArduinoQueue.h>


#include "MiTempDevices.h"
#include "MiTempDev.h"
#include "utils.h"

std::vector<MiTempDev*> myDevicesList;

extern ArduinoQueue<MiTempDev*> myUpdateList;   //Remove this

uint32_t g_bleConnectTimeout = 50;

       

MiTempDev* createMiTempDev(NimBLEAdvertisedDevice* pAdvertDevice)
{
    NimBLEAddress addr = pAdvertDevice->getAddress();

    bool found = false;
    for (MiTempDev* item : myDevicesList) {
        if (std::string(item->addr()) == addr.toString()) {
            return item;
        }
    }

    Serial.print("Create new device:");
    Serial.println(addr.toString().c_str());

    MiTempDev* pMiTempDev = new MiTempDev(addr.toString().c_str());

    const uint8_t* pNativeAddress = addr.getNative();
    uint8_t reversedBuffer[6] = {0};
    utils::reverseBytes(pNativeAddress, 6, reversedBuffer);

    uint8_t buffer[16] = {0};
    char* pHexAddress = NimBLEUtils::buildHexData(buffer, reversedBuffer, 6);
    pMiTempDev->setDevId(pHexAddress);
    Serial.println("N:" + String(pAdvertDevice->getName().c_str()) );
    pMiTempDev->setName(pAdvertDevice->getName().c_str());

    myDevicesList.push_back(pMiTempDev);
    return pMiTempDev;
}


/*
void readDeviceName(NimBLEClient* pClient, MiTempDev* pDev)
{
    NimBLERemoteService* pService = pClient->getService(NimBLEUUID((uint16_t)0x1800));
    if (pService != nullptr) {
      std::string val = pService->getValue(NimBLEUUID((uint16_t)0x2a00));
      pDev->setName(val.c_str());
      Serial.println("Name:" + String(val.c_str()));

    }
}*/

bool readDeviceBatteryLevel(NimBLEClient* pClient, MiTempDev* pDev)
{
    NimBLERemoteService* pService = pClient->getService(NimBLEUUID((uint16_t)0x180F));
    if (pService != nullptr) {
        NimBLERemoteCharacteristic* pChar = pService->getCharacteristic(NimBLEUUID((uint16_t)0x2a19));
        if (pChar) {
            uint8_t val = pChar->readValue<uint8_t>();
            pDev->setBatteryLevel(val);
            Serial.println("Battery:" + String(val));      
            return true;
        }
    }

    return false;
}

MiTempDev* pActualMiTempDev = nullptr;
static TaskHandle_t g_myTaskHandle = 0;

void my_notify_callback(NimBLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify)
{
    Serial.print("subscribe notify. read:");  
    std::string strData((char*)pData, length);
    Serial.println(strData.c_str());

    if (pActualMiTempDev) {
        pActualMiTempDev->parseBLEData(pData, length);
    }

    xTaskNotifyGive(g_myTaskHandle);
}

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

void readDeviceData(NimBLEClient* pClient)
{
    g_myTaskHandle = xTaskGetCurrentTaskHandle();

    NimBLERemoteService* pService = pClient->getService(NimBLEUUID("226c0000-6476-4566-7562-66734470666d"));
    if (pService != nullptr) {
        NimBLERemoteCharacteristic* pChar = pService->getCharacteristic(NimBLEUUID("226caa55-6476-4566-7562-66734470666d"));    //   
        if (pChar != nullptr) {

            bool bNotified = false;
            pChar->subscribe(true, my_notify_callback, true);
            for(int i = 0; i < 40; i++) {
                if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100)) != pdFALSE) {
                    bNotified = true;
                    break;
                }
                Serial.print(".");
            }
            Serial.println();

            if (!bNotified) {
                Serial.println("Timeout.");
            }

            delay(1200);
            pChar->unsubscribe();
        }
    }
}

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


bool connectAndReadDevice(MiTempDev* pDev)
{
    NimBLEAddress addr(pDev->addr());
    NimBLEClient* pClient = connectToBLEDevice(pDev->addr(), g_bleConnectTimeout);
    if (pClient == nullptr) {
        return false;
    }

    pDev->clearUpdatedFlag();

    Serial.println("Name:" + String(pDev->name()));

    int rssiLevel = pClient->getRssi();
    pDev->setRSSILevel(rssiLevel);
    Serial.println("RSSI:" + String(rssiLevel));

    pActualMiTempDev = pDev;
    bool ret = readDeviceBatteryLevel(pClient, pDev);
    if (!ret) {
        Serial.println("Batt read error.");
    }

    readDeviceData(pClient);
    Serial.println("Close.");

    delay(100);
    pClient->disconnect();

    pActualMiTempDev = NULL;

    return pDev->isUpdated();
}


