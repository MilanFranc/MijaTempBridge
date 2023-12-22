#pragma once

#include "DevDriver.h"
#include <vector>

//Name:Qingping Temp RH Lite

class QingpingDriver : public DevDriver
{
public:
    QingpingDriver() = default;

    static bool isMatch(NimBLEAdvertisedDevice* pAdvertDevice);

    bool readData(NimBLEClient* pClient) override;
    String getData() override;

private:
    bool parseBLEData(const uint8_t* data, uint32_t size);

};

