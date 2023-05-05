#ifndef MITEMP_DEVICES_H__
#define MITEMP_DEVICES_H__

class NimBLEAdvertisedDevice;
class MiTempDev;

MiTempDev* createMiTempDev(NimBLEAdvertisedDevice* pAdvertDevice);

NimBLEClient* connectToBLEDevice(const char* pBLEAddr, uint32_t timeout);

bool connectAndReadDevice(MiTempDev* pDev);




#endif //MITEMP_DEVICES_H__