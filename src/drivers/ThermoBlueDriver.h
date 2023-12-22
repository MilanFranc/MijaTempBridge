#pragma once

#include "DevDriver.h"

class ThermoBlueDriver  : public DevDriver
{
public:
    ThermoBlueDriver() = default;

    static bool isMatch(NimBLEAdvertisedDevice* pAdvertDevice);

    bool readData(NimBLEClient* pClient) override;
    String getData() override;

};
