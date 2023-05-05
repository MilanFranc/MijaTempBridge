
/** 
 *
 *  Demonstrates many of the available features of the NimBLE client library.
 *
 *
*/

#include <NimBLEDevice.h>

#include <ArduinoQueue.h>
#include <SoftTimers.h>
#include <ArduinoJson.h>

#include <freertos/task.h>


#if defined(ARDUINO_SAMD_MKRWIFI1010) || defined(ARDUINO_SAMD_NANO_33_IOT) || defined(ARDUINO_AVR_UNO_WIFI_REV2)
  #include <WiFiNINA.h>
#elif defined(ARDUINO_SAMD_MKR1000)
  #include <WiFi101.h>
#elif defined(ARDUINO_ARCH_ESP8266)
  #include <ESP8266WiFi.h>
#elif defined(ARDUINO_PORTENTA_H7_M7) || defined(ARDUINO_NICLA_VISION) || defined(ARDUINO_ARCH_ESP32)
  #include <WiFi.h>
#endif

#include "MQTTAdapter.h"

#include "MiTempDevices.h"
#include "MiTempDev.h"

#include "BLEDevScan.h"
#include "utils.h"
#include "version.h"



#include "arduino_secrets.h"
///////please enter your sensitive data in the Secret tab/arduino_secrets.h
char ssid[] = SECRET_SSID;    // your network SSID (name)
char pass[] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)


/**
 * Mija temperature and humidity
 * 
 *  uuid_characteristic_temperature_fine    = UUID("00002a6e-0000-1000-8000-00805f9b34fb") #handle 21
 *  uuid_characteristic_temperature_coarse  = UUID("00002a1f-0000-1000-8000-00805f9b34fb") #handle 18
 *  uuid_characteristic_humidity            = UUID("00002a6f-0000-1000-8000-00805f9b34fb") #handle 24
 *  uuid_characteristic_battery             = UUID("00002a19-0000-1000-8000-00805f9b34fb") #handle 14
 * 
 */


NimBLEScan* pBLEScan = NULL;


SoftTimer bleDeviceScanTimer;
SoftTimer bleDataScanTimer;

const unsigned long nDataRefreshTime = 2 * 60 * 1000;  // 15 minutes
const unsigned long nDevicesRefreshTime = 4 * 60 * 60 * 1000;   //4 hours
const int nDeviceScanTimeout = 50; //seconds


const int STATE_IDLE = 0;
const int STATE_READ_BLE_DATA = 1;
const int STATE_CONNECT_WIFI_AND_MQTT = 2;
const int STATE_SEND_MQTT_DATA = 3;
const int STATE_DISCONNECT_WIFI = 4;

int nCurrentState = STATE_IDLE;

int count = 0;


ArduinoQueue<MiTempDev*> myUpdateList(30);

extern std::vector<MiTempDev*> myDevicesList;

/////////////////////////////////////////////////////////////////////

void initBLEDev()
{
    if (NimBLEDevice::getInitialized())
        return;

    /** *Optional* Sets the filtering mode used by the scanner in the BLE controller.
     *
     *  Can be one of:
     *  CONFIG_BTDM_SCAN_DUPL_TYPE_DEVICE (0) (default)
     *  Filter by device address only, advertisements from the same address will be reported only once.
     *
     *  CONFIG_BTDM_SCAN_DUPL_TYPE_DATA (1)
     *  Filter by data only, advertisements with the same data will only be reported once,
     *  even from different addresses.
     *
     *  CONFIG_BTDM_SCAN_DUPL_TYPE_DATA_DEVICE (2)
     *  Filter by address and data, advertisements from the same address will be reported only once,
     *  except if the data in the advertisement has changed, then it will be reported again.
     *
     *  Can only be used BEFORE calling NimBLEDevice::init.
    */
    NimBLEDevice::setScanFilterMode(CONFIG_BTDM_SCAN_DUPL_TYPE_DEVICE);


    /** *Optional* Sets the scan filter cache size in the BLE controller.
     *  When the number of duplicate advertisements seen by the controller
     *  reaches this value it will clear the cache and start reporting previously
     *  seen devices. The larger this number, the longer time between repeated
     *  device reports. Range 10 - 1000. (default 20)
     *
     *  Can only be used BEFORE calling NimBLEDevice::init.
     */
    NimBLEDevice::setScanDuplicateCacheSize(200);


    /** Initialize NimBLE, no device name spcified as we are not advertising */
    NimBLEDevice::init("");


#if 0
    /** Set the IO capabilities of the device, each option will trigger a different pairing method.
     *  BLE_HS_IO_KEYBOARD_ONLY    - Passkey pairing
     *  BLE_HS_IO_DISPLAY_YESNO   - Numeric comparison pairing
     *  BLE_HS_IO_NO_INPUT_OUTPUT - DEFAULT setting - just works pairing
     */
    //NimBLEDevice::setSecurityIOCap(BLE_HS_IO_KEYBOARD_ONLY); // use passkey
    //NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_YESNO); //use numeric comparison

    /** 2 different ways to set security - both calls achieve the same result.
     *  no bonding, no man in the middle protection, secure connections.
     *
     *  These are the default values, only shown here for demonstration.
     */
    //NimBLEDevice::setSecurityAuth(false, false, true);
    NimBLEDevice::setSecurityAuth(/*BLE_SM_PAIR_AUTHREQ_BOND | BLE_SM_PAIR_AUTHREQ_MITM |*/ BLE_SM_PAIR_AUTHREQ_SC);

    /** Optional: set the transmit power, default is 3db */
    NimBLEDevice::setPower(ESP_PWR_LVL_P9); /** +9db */
#endif
  
}

void deinitBLEDev()
{
    if (!NimBLEDevice::getInitialized())
        return;

    NimBLEDevice::deinit();
}

bool connectToWifiAndMQTT()
{
    if (WiFi.status() != WL_CONNECTED) {

        WiFi.begin(ssid, pass);
    
        // attempt to connect to WiFi network:
        Serial.print("Attempting to connect to WPA SSID: ");
        Serial.println(ssid);
    
        for(int i = 0; i < 20 ; i++) {
            if (WiFi.status() == WL_CONNECTED) {
                break;
            }
        
            // failed, retry
            Serial.print(".");
            delay(5000);
        }
    
        if (WiFi.status() != WL_CONNECTED) {
            return false;
        }

        Serial.println();
        Serial.println("You're connected to the network");
    }
    else {
        Serial.println("Still connected to the network");    
    }

    return connectToMQTT();
}


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

#if 0
void scanForDevices(int timeout)
{
    Serial.println("Start scan..");
  
    pBLEScan = NimBLEDevice::getScan(); //create new scan
//    setupScanDev();

    NimBLEScanResults results = pBLEScan->start(timeout, false);

    Serial.println("Scan done.");

    const uint8_t addrPrefix[3] = { 0x58, 0x2d, 0x34 };

    std::vector<NimBLEAdvertisedDevice*>::iterator it;
    for(it = results.begin(); it != results.end(); ++it) {
        NimBLEAdvertisedDevice* pAdvDevice = *it;
      
        NimBLEAddress addr = pAdvDevice->getAddress();
        const uint8_t* natAddr = addr.getNative();

        if (natAddr[5] == addrPrefix[0] && natAddr[4] == addrPrefix[1] &&
            natAddr[3] == addrPrefix[2]) {

            Serial.println("Addr:" + String(addr.toString().c_str()));

            createMiTempDevice(pAdvDevice);
        }
    }

    Serial.println("Done.");
}
#endif


/////////////////////////////////////////////////////////////////////////////////////


void InitState0();
void UpdateState0();
void InitState1();
void UpdateState1();
void InitState2();
void UpdateState2();
void InitState3();
void UpdateState3();
void InitState4();
void UpdateState4();




void setup() {
    Serial.begin(115200);
    Serial.println("Starting BLE Bridge ver:" APP_VERSION);

    WiFi.mode(WIFI_STA);

    bleDeviceScanTimer.setTimeOutTime(nDevicesRefreshTime);
    bleDataScanTimer.setTimeOutTime(nDataRefreshTime);
    
    InitState0();
}

///////////////////////////////////////

void InitState0()
{
    nCurrentState = 0;    
    if (!connectToWifiAndMQTT()) {
        Serial.println("Unable to connect to wifi.");
        delay(5000);

        ESP.restart();
    }

    sendStatusOnlineMsg();
    pollMQTT();    
    delay(500);

    Serial.println("Disconnect Wifi");
    WiFi.disconnect();
    delay(500);    

    InitState1();
}

void UpdateState0()
{
    //nothing
}

///////////////////////////////////////

bool filterDevices(NimBLEAdvertisedDevice* pAdvDevice)
{
    const uint8_t addrPrefix[3] = { 0x58, 0x2d, 0x34 };

    NimBLEAddress addr = pAdvDevice->getAddress();
    const uint8_t* natAddr = addr.getNative();

    if (natAddr[5] == addrPrefix[0] && natAddr[4] == addrPrefix[1] &&
        natAddr[3] == addrPrefix[2]) 
    {
        if (pAdvDevice->getName() == "MJ_HT_V1") {
            return true;
        }
    }
    return false;
}

void onDeviceFound(NimBLEAdvertisedDevice* pDevice)
{
    if (pDevice == nullptr) {   //Scan end
        InitState2();
        bleDeviceScanTimer.reset();
        return;
    }
    else {
        if (filterDevices(pDevice)) {
            MiTempDev* newDev = createMiTempDev(pDevice);
        }
    }
}

void InitState1()
{
    nCurrentState = 1;
    initBLEDev();
    vTaskDelay(500 / portTICK_PERIOD_MS);

    startBLEDevicesScan(nDeviceScanTimeout, onDeviceFound);
}

void UpdateState1()
{
    //Nothing...
}

///////////////////////////////////////

void InitState2()
{
    nCurrentState = 2;

    initBLEDev();
    vTaskDelay(500 / portTICK_PERIOD_MS);
}

void UpdateState2()
{
    Serial.println("State: Read sensors data");
    if (!myDevicesList.empty()) {

        std::vector<MiTempDev*>::iterator it;
        for(it = myDevicesList.begin(); it != myDevicesList.end(); ++it) {
            if (connectAndReadDevice(*it)) {
                myUpdateList.enqueue(*it);
            }

            vTaskDelay(500 / portTICK_PERIOD_MS);
        }

        Serial.println("Update:" + String(myUpdateList.itemCount()));
        if (!myUpdateList.isEmpty()) {
            InitState3();
            return;
        }

        //TODO: ????
        //nCurrentState = STATE_CONNECT_WIFI_AND_MQTT;
        //sleepTime = 500;
    }
    else {
        Serial.println("Device list is empty!");      
    }

    vTaskDelay(20000 / portTICK_PERIOD_MS);
}

///////////////////////////////////////

void InitState3()
{
    nCurrentState = 3;
    Serial.println("State: WiFi and MQTT connect");

    deinitBLEDev();
    vTaskDelay(5000 / portTICK_PERIOD_MS);

    if (connectToWifiAndMQTT()) {

        vTaskDelay(50 / portTICK_PERIOD_MS);
        sendStatusOnlineMsg();
    }
    else {
        Serial.println("Unable to connect to WiFi!");
        nCurrentState = 99; //Error..
    }
}

void UpdateState3()
{
    // call poll() regularly to allow the library to send MQTT keep alives which
    // avoids being disconnected by the broker
    pollMQTT();

    MiTempDev* pDev;
    for(int i = 0; i < 1; i++) {
        if (myUpdateList.isEmpty())
            break;

        pDev = myUpdateList.dequeue();
        sendMiTempDataToMQTT(pDev);
    }

    flushMQTT();
    Serial.println();

    if (myUpdateList.isEmpty()) {
        vTaskDelay(2000 / portTICK_PERIOD_MS);

        Serial.println("WiFi disconnect");
        WiFi.disconnect();
        count++;     

        InitState4();
    }
    else {
        vTaskDelay(50 / portTICK_PERIOD_MS);

    }
}

///////////////////////////////////////

void InitState4()
{
    nCurrentState = 4;
    Serial.println("State: IDLE state");

    bleDataScanTimer.reset();    
}

void UpdateState4()
{
    if (bleDeviceScanTimer.hasTimedOut()) {
        InitState1();
    }
    else if (bleDataScanTimer.hasTimedOut()) {
        InitState2();
    }

    delay(1000);
}

void loop() 
{
    switch(nCurrentState) {
    case 0: UpdateState0(); break;
    case 1: UpdateState1(); break;
    case 2: UpdateState2(); break;
    case 3: UpdateState3(); break;
    case 4: UpdateState4(); break;
    default:
        break;
    }

    //TODO: ArduinoOTA.handle();

#if 0
    unsigned long sleepTime = 30 * 1000;  //default sleep time

    switch(currentState)
    {
    case STATE_IDLE:
        Serial.println("State: IDLE");
        if (bleDeviceScanTimer.hasTimedOut()) {
            scanForDevices(nDeviceScanTimeout);
            bleDeviceScanTimer.reset();
        }
    
        if (bleDataScanTimer.hasTimedOut()) {
            Serial.println("aa");
            if (!myDevicesList.empty()) {
                currentState = STATE_READ_BLE_DATA;
                sleepTime = 10;
            }
            else {
              Serial.println("Empty list..");              
            }

            bleDataScanTimer.reset();   
        }

        if (currentState == STATE_IDLE) {
            sleepTime = min(bleDeviceScanTimer.getRemainingTime(), bleDataScanTimer.getRemainingTime());
        }
        break;

    case STATE_READ_BLE_DATA:
        Serial.println("State: Read BLE data");

        if (!myDevicesList.empty()) {
  
            std::vector<MiTempDev*>::iterator it;
            for(it = myDevicesList.begin(); it != myDevicesList.end(); ++it) {
                connectAndReadDevice(*it);
            }

            currentState = STATE_CONNECT_WIFI_AND_MQTT;
            sleepTime = 500;
            
        }
        else {
            Serial.println("Device list is empty!");      
        }
        break;

    case STATE_CONNECT_WIFI_AND_MQTT:
        Serial.println("State: WiFi and MQTT connect");
        if (connectToWifiAndMQTT()) {

            // call poll() regularly to allow the library to send MQTT keep alives which
            // avoids being disconnected by the broker
            pollMQTT();
            currentState = STATE_SEND_MQTT_DATA;
            sleepTime = 50;
        }
        else {
            Serial.println("Unable to connect to WiFi!");          
        }
        break;

    case STATE_SEND_MQTT_DATA:
        Serial.println("State: Send MQTT data");
    
        MiTempDev* pDev;
        while(!myUpdateList.isEmpty()) {
            pDev = myUpdateList.dequeue();
    
            sendMiTempDataToMQTT(pDev);
        }

        flushMQTT();
        Serial.println();

        sleepTime = 2000;
        currentState = STATE_DISCONNECT_WIFI;
        break;

    case STATE_DISCONNECT_WIFI:
        Serial.println("State: WiFi disconnect");
    
        WiFi.disconnect();
        count++;     

        bleDataScanTimer.reset();
        currentState = STATE_IDLE;
        break;
    }

    Serial.println("Sleep (" + String(count) + ") for: " + String(sleepTime) + "ms..........");
    delay(sleepTime);
#endif

}
