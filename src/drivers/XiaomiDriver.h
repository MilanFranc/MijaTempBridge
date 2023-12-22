#pragma once

#include "DevDriver.h"

class XiaomiDriver : public DevDriver
{
public:
    XiaomiDriver() = default;

    static bool isMatch(NimBLEAdvertisedDevice* pAdvertDevice);

    bool readData(NimBLEClient* pClient) override;
    String getData() override;

private:
    bool parseBLEData(const std::string& data);

};

