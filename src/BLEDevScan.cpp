
#include <NimBLEDevice.h>
#include <NimBLEScan.h>
#include <NimBLEAddress.h>
#include <esp_bt.h>     //for power level

#include <Arduino.h>

#include "BLEDevScan.h"

class AdvertisedDeviceCallbacks;


static bool g_bInitialized = false;
static TonDeviceFoundCB g_onDeviceFoundCB = nullptr;


//void setupScanDev()
//{
//    pBLEScan = NimBLEDevice::getScan(); //create new scan
//    // Set the callback for when devices are discovered, no duplicates.
//    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), false);
//    pBLEScan->setActiveScan(true); // Set active scanning, this will get more data from the advertiser.
//    pBLEScan->setInterval(97); // How often the scan occurs / switches channels; in milliseconds,
//    pBLEScan->setWindow(37);  // How long to scan during the interval; in milliseconds.
//    pBLEScan->setMaxResults(0); // do not store the scan results, use callback only.
//  
//}


/** Define a class to handle the callbacks when advertisments are received */
class AdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks 
{
public:
    AdvertisedDeviceCallbacks(TonDeviceFoundCB deviceFoundCB) : m_deviceFoundCB(deviceFoundCB) {}

    void onResult(NimBLEAdvertisedDevice* advertisedDevice) override {
        Serial.print("Device:");
        NimBLEAddress addr = advertisedDevice->getAddress();
        Serial.print(" Mac:");
        Serial.print(addr.toString().c_str());
        Serial.print(" RSSI:");
        Serial.print(String(advertisedDevice->getRSSI()));
        Serial.print(" Name:");
        Serial.print(advertisedDevice->getName().c_str());
        Serial.println();

        if (m_deviceFoundCB) 
            m_deviceFoundCB(advertisedDevice);
    };

private:
    TonDeviceFoundCB m_deviceFoundCB;
};

static void scanEndedCB(BLEScanResults results)
{
    Serial.println("Scan Ended");

    if (g_onDeviceFoundCB) {
        g_onDeviceFoundCB(nullptr);
    }
}

void startBLEDevicesScan(uint32_t scanTime, TonDeviceFoundCB deviceFoundCB)
{
    /** Optional: set the transmit power, default is 3db */
#ifdef ESP_PLATFORM
    NimBLEDevice::setPower(ESP_PWR_LVL_P9); /** +9db */
#else
    NimBLEDevice::setPower(9); /** +9db */
#endif

    /** create new scan */
    NimBLEScan* pScan = NimBLEDevice::getScan();
    if (!g_bInitialized) {

        /** create a callback that gets called when advertisers are found */
        pScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks(deviceFoundCB));

        /** Set scan interval (how often) and window (how long) in milliseconds */
        pScan->setInterval(45);
        pScan->setWindow(15);

        pScan->setDuplicateFilter(true);

        /** Active scan will gather scan response data from advertisers
         *  but will use more energy from both devices
         */
        pScan->setActiveScan(true);
        g_bInitialized = true;
    }

    g_onDeviceFoundCB = deviceFoundCB;
    pScan->clearDuplicateCache();
    pScan->clearResults();

    /** Start scanning for advertisers for the scan time specified (in seconds) 0 = forever
     *  Optional callback for when scanning stops.
     */
    Serial.println("Start BLE scan..");    
    pScan->start(scanTime, scanEndedCB);
}

void clearBLEScanData()
{
    if (!g_bInitialized) {
        return;
    }

    NimBLEScan* pScan = NimBLEDevice::getScan();
    if (pScan) {
        pScan->clearDuplicateCache();
        pScan->clearResults();
    }
}
