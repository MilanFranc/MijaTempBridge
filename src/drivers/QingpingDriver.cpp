#include "QingpingDriver.h"
#include "../debug.h"


#include <NimBLEAddress.h>
#include <NimBLEClient.h>
#include <NimBLEDevice.h>

static const NimBLEUUID serviceReadData = NimBLEUUID("22210000-554a-4546-5542-46534450464d");
static BLEUUID characteristicReadData = NimBLEUUID((uint16_t)0x0100);



bool QingpingDriver::isMatch(NimBLEAdvertisedDevice* pAdvertDevice)
{
    const uint8_t addrPrefix[3] = { 0x58, 0x2d, 0x34 };

    NimBLEAddress addr = pAdvertDevice->getAddress();
    const uint8_t* natAddr = addr.getNative();

    if (natAddr[5] == addrPrefix[0] && natAddr[4] == addrPrefix[1] &&
        natAddr[3] == addrPrefix[2]) 
    {
        if (pAdvertDevice->getName() == "Qingping Temp RH Lite") {
            return true;
        }
    }
    return false;
}

bool QingpingDriver::readData(NimBLEClient* pClient)
{
    NimBLEAttValue readData;
    bool bResult = readNotifyData(pClient, serviceReadData, characteristicReadData, readData);
    if (bResult) {
        bResult = this->parseBLEData(readData.data(), readData.size());
    }


#if 0
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
    NimBLERemoteService* pService = pClient->getService(NimBLEUUID("22210000-554a-4546-5542-46534450464d"));
    if (pService != nullptr) {
        NimBLERemoteCharacteristic* pChar = pService->getCharacteristic(NimBLEUUID((uint16_t)0x0100));    //   
        if (pChar != nullptr) {

            NotifyCallback notifier;

            bool bNotified = false;
            pChar->subscribe(true, std::bind(&NotifyCallback::notify, &notifier, 
                                                std::placeholders::_1,
                                                std::placeholders::_2,
                                                std::placeholders::_3,
                                                std::placeholders::_4), true);
            for(int i = 0; i < 40; i++) {
                if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100)) != pdFALSE) {
                    break;
                }
                Serial.print(".");
            }
            Serial.println();

            if (!notifier.isNotified()) {
                Serial.println("Timeout.");
            }
            else {
                Serial.print("Notify read:");

                std::vector<uint8_t> data = notifier.data();
                printBuffer(reinterpret_cast<const char*>(data.data()), data.size());

                bResult = this->parseBLEData(notifier.data());
            }

            delay(50);
            pChar->unsubscribe();
            delay(50);


        }
    }
#endif

    return bResult;
}

String QingpingDriver::getData()
{
    return String();
}

bool QingpingDriver::parseBLEData(const uint8_t* data, uint32_t size)
{
    if (size < 6)
        return false;

    if (data[0] != 0x06 || data[1] != 0x17) {
        return false;
    }

    uint16_t val1 = data[2] | (data[3] << 8);
    //Serial.println("A:" + String(temp1));

    uint16_t val2 = data[4] | (data[5] << 8);
    //Serial.println("A:" + String(temp2));

    this->setTemp(String(val1 / 10.0).c_str());
    this->setHum(String(val2 / 10.0).c_str());
    Serial.println("T:" + String(getTemp()) + " H:" + String(getHum()));

    return true;
}

