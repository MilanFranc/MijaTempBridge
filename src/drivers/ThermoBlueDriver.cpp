#include "ThermoBlueDriver.h"

#include <NimBLEAddress.h>
#include <NimBLEClient.h>
#include <NimBLEDevice.h>


namespace internal {

bool BLEValueToFloat(NimBLEAttValue& value, float& fReadValue)
{
    if (value.length() == sizeof(float)) {
        fReadValue = *((float*)value.getValue(nullptr));
        return true;
    }
    return false;
}

}

bool ThermoBlueDriver::isMatch(NimBLEAdvertisedDevice* pAdvertDevice)
{
    if (pAdvertDevice->getName() == "ThermoB") {
        return true;
    }

    return false;
}

bool ThermoBlueDriver::readData(NimBLEClient* pClient)
{
    bool bRet;
    NimBLEAttValue dataBuffer;

    //Temperature
    bRet = readNotifyData(pClient, NimBLEUUID("00002234-4ae7-44c4-b235-b75e375084b8"), NimBLEUUID("00002235-4ae7-44c4-b235-b75e375084b8"), dataBuffer);
    if (bRet) {
        float fValue;
        if (internal::BLEValueToFloat(dataBuffer, fValue)) {
            //TODO check the value out of range...

            this->setTemp(String(fValue).c_str());
            Serial.println("T:" + String(fValue));            
        }
        else {
            this->setTemp("");
        }
    }

    //Humidity
    bRet = readNotifyData(pClient, NimBLEUUID("00001234-4ae7-44c4-b235-b75e375084b8"), NimBLEUUID("00001235-4ae7-44c4-b235-b75e375084b8"), dataBuffer);
    if (bRet) {
        float fValue;
        if (internal::BLEValueToFloat(dataBuffer, fValue)) {
            //TODO check the value out of range...

            this->setHum(String(fValue).c_str());
            Serial.println("H:" + String(fValue));
        }
        else {
            this->setHum("");
        }
    }

#if 0
    {
        NimBLERemoteService* pService = pClient->getService(NimBLEUUID("00002234-4ae7-44c4-b235-b75e375084b8"));
        if (pService != nullptr) {
            NimBLERemoteCharacteristic* pChar = pService->getCharacteristic(NimBLEUUID("00002235-4ae7-44c4-b235-b75e375084b8"));    //   
            if (pChar != nullptr) {
                NimBLEAttValue value = pChar->getValue();   //Temperature
                float fValue;
                if (internal::BLEValueToFloat(value, fValue)) {
                    Serial.println("T:" + String(fValue));
                    this->setTemp(String(fValue).c_str());
                }
                else {
                    this->setTemp("");
                }
            }
        }
    }

    {
        NimBLERemoteService* pService = pClient->getService(NimBLEUUID("00001234-4ae7-44c4-b235-b75e375084b8"));
        if (pService != nullptr) {
            NimBLERemoteCharacteristic* pChar = pService->getCharacteristic(NimBLEUUID("00001235-4ae7-44c4-b235-b75e375084b8"));    //   
            if (pChar != nullptr) {
                NimBLEAttValue value = pChar->getValue();   //Humidity
                float fValue;
                if (internal::BLEValueToFloat(value, fValue)) {
                    Serial.println("H:" + String(fValue));
                    this->setHum(String(fValue).c_str());
                }
                else {
                    this->setHum("");
                }
            }
        }
    }
#endif    

    bool bResult = (this->isTempSet() && this->isHumSet());
    return bResult;
}

String ThermoBlueDriver::getData()
{
    return String();
}


