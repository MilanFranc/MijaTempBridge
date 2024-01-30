#include "BLESensorsMngr.h"
#include "MyBLEDevice.h"
#include "utils.h"

#include <NimBLEDevice.h>
#include <NimBLEClient.h>
#include <NimBLEAdvertisedDevice.h>

static const uint32_t g_bleConnectTimeout = 50;


namespace internal {

    NimBLEClient* connectToBLEDevice(const NimBLEAddress& addrBLE, uint32_t timeout)
    {
        NimBLEClient* pClient = nullptr;
        String addrBLEString(addrBLE.toString().c_str());

        if (NimBLEDevice::getClientListSize() > 0) {
            /** Special case when we already know this device, we send false as the
             *  second argument in connect() to prevent refreshing the service database.
             *  This saves considerable time and power.
             */
            pClient = NimBLEDevice::getClientByPeerAddress(addrBLE);
            if (pClient) {
                Serial.println("Connecting to " + addrBLEString + " - exist client");

                if (pClient->isConnected()) {
                    Serial.println("Disconnect client");
                    pClient->disconnect();
                    delay(100);
                }

                pClient->setConnectTimeout(timeout);
                if (!pClient->connect(addrBLE, false)) {
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

            pClient = NimBLEDevice::createClient(addrBLE);

            Serial.println("Connecting to " + addrBLEString + " - new client");
            pClient->setConnectTimeout(timeout);
            
            if (!pClient->connect()) {

                /** Created a client but failed to connect, don't need to keep it as it has no data */
                NimBLEDevice::deleteClient(pClient);
                Serial.println("Failed to connect, deleted client");
                return nullptr;
            }
        }

        if (!pClient->isConnected()) {
            Serial.println("Connecting to " + addrBLEString);

            pClient->setConnectTimeout(timeout);
            if (!pClient->connect(addrBLE)) {
                Serial.println("Failed to connect");
                return nullptr;
            }
        }
        return pClient;
    }
}



BLESensorsMngr::BLESensorsMngr()
{
}

MyBLEDevice* BLESensorsMngr::deviceAt(size_t index) const
{
    if (index >= myDevicesList.size()) {
        return nullptr;
    }
    return myDevicesList[index];
}

MyBLEDevice* BLESensorsMngr::findAdvDeviceInList(const NimBLEAddress& devAddress)
{
    for (MyBLEDevice* item : myDevicesList) {
        if (item->native_addr() == devAddress) {
            return item;
        }
    }
    return nullptr;
}

void BLESensorsMngr::onDeviceFound(NimBLEAdvertisedDevice* pDevice)
{
    NimBLEAddress addr = pDevice->getAddress();

    MyBLEDevice* pTempDev = findAdvDeviceInList(addr);
    if (pTempDev == nullptr) {
        pTempDev = MyBLEDevice::createDev(pDevice); //  new MiTempDev(addr.toString().c_str());
        if (pTempDev != nullptr) {
            Serial.print("Create new device:" + utils::stdStringToStr(pTempDev->name()));
            Serial.print(" Id:" + pTempDev->devId() );
            Serial.println(" Addr:" + utils::stdStringToStr(pTempDev->addr()));

            myDevicesList.push_back(pTempDev);
        }
    }
}

int BLESensorsMngr::connectAndReadData(ArduinoQueue<MyBLEDevice*>& updateQueue)
{
    if (myDevicesList.empty()) {
        return 0;
    }

    for(MyBLEDevice* pDev : myDevicesList) {
        assert(pDev);

        NimBLEClient* pClient = internal::connectToBLEDevice(pDev->native_addr(), g_bleConnectTimeout);
        if (pClient == nullptr) {
            //TODO: create some counter for failed connects...
            pDev->incFailedCount();
            return false;
        }

        Serial.println("Name:" + utils::stdStringToStr(pDev->name()) );
        pDev->incConnectCount();

        //TODO: add check for device if it doesn't respond..
        if (pDev->connectAndReadDevice(pClient)) {
            updateQueue.enqueue(pDev);
        }

        delay(100);
        Serial.println("Close.");    
        pClient->disconnect();

        Serial.println();
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }

    return updateQueue.itemCount();
}

void BLESensorsMngr::createDevicesMACList(std::string& result)
{
    result.clear();
    for(auto item : myDevicesList) {
        result.append( item->addr() );
        result.append(";");
    }
}

