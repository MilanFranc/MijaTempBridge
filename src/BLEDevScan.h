#pragma once

#include <vector>
class NimBLEAdvertisedDevice;

typedef void(*TonDeviceFoundCB)(NimBLEAdvertisedDevice* device);

void startBLEDevicesScan(uint32_t scanTime, TonDeviceFoundCB deviceFoundCB);
void clearBLEScanData();

