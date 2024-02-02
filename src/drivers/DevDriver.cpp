#include "DevDriver.h"

#include "XiaomiDriver.h"
#include "QingpingDriver.h"
#include "ThermoBlueDriver.h"
#include "debug.h"

DevDriver::DevDriver() : m_BLEDevice(nullptr)
{
    //TODO:
}

DevDriver* DevDriver::createDriver(NimBLEAdvertisedDevice* pAdvertDevice)
{
    DevDriver* pDriver = nullptr;
    if (XiaomiDriver::isMatch(pAdvertDevice)) {
        pDriver = new XiaomiDriver;
        Serial.println("Created Xiaomi driver.");
    }
    else if (QingpingDriver::isMatch(pAdvertDevice)) {
        pDriver = new QingpingDriver;
        Serial.println("Created Qingping driver.");
    }
    else if (ThermoBlueDriver::isMatch(pAdvertDevice)) {
        pDriver = new ThermoBlueDriver;
        Serial.println("Created ThermoBlue driver.");
    }

    return pDriver;
}

void DevDriver::attachBLEDevice(MyBLEDevice* dev)
{
    m_BLEDevice = dev;
}


bool readNotifyData(NimBLEClient* pClient, const NimBLEUUID& service, const NimBLEUUID& characteristic, NimBLEAttValue& readData)
{
    class NotifyCallback
    {
    public:
        NotifyCallback()
        {
            m_hTaskHandle = xTaskGetCurrentTaskHandle();
            m_notified = false;
        }

        void notify(NimBLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify)
        {
            if (m_notified)
                return;

            std::copy(pData, pData + length, std::back_inserter(m_data));

            m_notified = true;
            xTaskNotifyGive(m_hTaskHandle);
        }

        bool isNotified() const { return m_notified; }
        std::vector<uint8_t> data() const { return m_data; }

    private:
        TaskHandle_t m_hTaskHandle;
        bool m_notified;
        std::vector<uint8_t> m_data;
    };

    bool bResult = false;
    NimBLERemoteService* pService = pClient->getService(service);
    if (pService != nullptr) {
        NimBLERemoteCharacteristic* pChar = pService->getCharacteristic(characteristic);    //   
        if (pChar != nullptr) {

            NotifyCallback notifier;

            bool bNotified = false;
            pChar->subscribe(true, std::bind(&NotifyCallback::notify, &notifier, 
                                                std::placeholders::_1,
                                                std::placeholders::_2,
                                                std::placeholders::_3,
                                                std::placeholders::_4), true);
            for(int i = 0; i < 50; i++) {
                if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100)) != pdFALSE) {
                    break;
                }
                Serial.print(".");
            }
            Serial.println();

            if (!notifier.isNotified()) {
                readData = NimBLEAttValue();
                Serial.println("Timeout.");
            }
            else {
                readData = NimBLEAttValue(notifier.data());
                bResult = true;

                Serial.print("Notify read:");                
                printBuffer(reinterpret_cast<const char*>(readData.data()), readData.size());
            }

            delay(50);
            pChar->unsubscribe();
            delay(50);
        }
    }

    return bResult;
}



