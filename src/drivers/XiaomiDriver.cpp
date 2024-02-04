#include "XiaomiDriver.h"
#include "../MyBLEDevice.h"

#include <NimBLEAddress.h>
#include <NimBLEClient.h>
#include <NimBLEDevice.h>

#include <ArduinoJson.h>

/**
 * Mija temperature and humidity
 * 
 *  uuid_characteristic_temperature_fine    = UUID("00002a6e-0000-1000-8000-00805f9b34fb") #handle 21
 *  uuid_characteristic_temperature_coarse  = UUID("00002a1f-0000-1000-8000-00805f9b34fb") #handle 18
 *  uuid_characteristic_humidity            = UUID("00002a6f-0000-1000-8000-00805f9b34fb") #handle 24
 *  uuid_characteristic_battery             = UUID("00002a19-0000-1000-8000-00805f9b34fb") #handle 14
 * 
 */

bool XiaomiDriver::isMatch(NimBLEAdvertisedDevice* pAdvertDevice)
{
    const uint8_t addrPrefix[3] = { 0x58, 0x2d, 0x34 };

    NimBLEAddress addr = pAdvertDevice->getAddress();
    const uint8_t* natAddr = addr.getNative();

    if (natAddr[5] == addrPrefix[0] && natAddr[4] == addrPrefix[1] &&
        natAddr[3] == addrPrefix[2]) 
    {
        if (pAdvertDevice->getName() == "MJ_HT_V1") {
            return true;
        }
    }
    return false;
}

bool XiaomiDriver::readData(NimBLEClient* pClient)
{
    NimBLEAttValue dataBuffer;    
    bool bResult = readNotifyData(pClient,
                                   NimBLEUUID("226c0000-6476-4566-7562-66734470666d"),
                                   NimBLEUUID("226caa55-6476-4566-7562-66734470666d"),
                                   dataBuffer);
    if (bResult) {
        bResult = this->parseBLEData(std::string(dataBuffer.c_str()));
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

            m_data.append(reinterpret_cast<char*>(pData), length);
            m_notified = true;
            xTaskNotifyGive(m_hTaskHandle);
        }

        bool isNotified() const { return m_notified; }
        std::string data() const { return m_data; }

    private:
        TaskHandle_t m_hTaskHandle;
        bool m_notified;
        std::string m_data;
    };

    bool bResult = false;
    NimBLERemoteService* pService = pClient->getService(NimBLEUUID("226c0000-6476-4566-7562-66734470666d"));
    if (pService != nullptr) {
        NimBLERemoteCharacteristic* pChar = pService->getCharacteristic(NimBLEUUID("226caa55-6476-4566-7562-66734470666d"));    //   
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
                Serial.print("Read:");
                Serial.println(notifier.data().c_str());

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

String XiaomiDriver::getData()
{
    //TODO: create Json string...

    // StaticJsonDocument<200> doc;

    // doc["temp"] = m_temperatureValue.c_str();
    // doc["humidity"] = m_humidityValue.c_str();


    return String();
}

bool XiaomiDriver::parseBLEData(const std::string& data)
{
    String temp;
    temp.reserve(8);

    // m_timestamp = millis();
    bool bParsed = false;

    //TODO: parssing: T=23.5 H=52.4
    int idx = 0;
    if (data.length() > 2 && data[idx] == 'T' && data[idx+1] == '=') {
        idx += 2;

        for(; idx < data.length(); idx++) {
            if (data[idx] == ' ')
            break;

            temp += (char)data[idx];
        }

        this->setTemp(temp.c_str());
        idx++;  //skip space
    }

    if ((data.length() - idx) > 2 && data[idx] == 'H' && data[idx+1] == '=') {
        idx += 2;

        temp = "";
        for(; idx < data.length(); idx++) {
            if (data[idx] == ' ')
            break;

            temp += (char)data[idx];
        }

        this->setHum(temp.c_str());
        bParsed = true;
    }

    return bParsed;
}

