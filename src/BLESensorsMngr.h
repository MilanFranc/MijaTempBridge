#pragma once

#include <vector>
#include <NimBLEAddress.h>
#include <ArduinoQueue.h>


class MyBLEDevice;
class NimBLEAdvertisedDevice;

class BLESensorsMngr
{
public:
    BLESensorsMngr();
    size_t deviceCount() const { return myDevicesList.size(); }

    void onDeviceFound(NimBLEAdvertisedDevice* pDevice);

    int connectAndReadData(ArduinoQueue<MyBLEDevice*>& updateQueue);

    void createDevicesMACList(std::string& result);

private:
    MyBLEDevice* findAdvDeviceInList(const NimBLEAddress& devAddress);

private:
    std::vector<MyBLEDevice*> myDevicesList;

};