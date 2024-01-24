

/** 
 *
 *  Demonstrates many of the available features of the NimBLE client library.
 *
 *
*/

#define FS_NO_GLOBALS

#include <NimBLEDevice.h>

#include <ArduinoQueue.h>
#include <SoftTimers.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>

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

#include "MyBLEDevice.h"
#include "BLESensorsMngr.h"

#include "BLEDevScan.h"
#include "utils.h"
#include "version.h"



#include "arduino_secrets.h"
///////please enter your sensitive data in the Secret tab/arduino_secrets.h
char ssid[] = SECRET_SSID;    // your network SSID (name)
char pass[] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)

const char mqtt_broker[] = MQTT_BROKER_ADDR;
int        mqtt_port     = 1883;

#define BLE_DEVICES_LIST  "/BLEDevList1"

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

const unsigned long nDataRefreshTime = 15 * 60 * 1000;  // 15 minutes
const unsigned long nDevicesRefreshTime = 4 * 60 * 60 * 1000;   //4 hours
const int nDeviceScanTimeout = 90; //seconds


const int STATE_IDLE = 0;
const int STATE_READ_BLE_DATA = 1;
const int STATE_CONNECT_WIFI_AND_MQTT = 2;
const int STATE_SEND_MQTT_DATA = 3;
const int STATE_DISCONNECT_WIFI = 4;

int nCurrentState = STATE_IDLE;

int count = 0;

ArduinoQueue<MyBLEDevice*> myUpdateList(30);

BLESensorsMngr myDevices;

const int g_maxNumberOfConnectAttempts = 20;
const uint32_t g_timeToRestartAfterConnectionFailed = 5 * 60 * 1000;


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

    //TODO: make it optional - by settings
    /** Optional: set the transmit power, default is 3db */
    NimBLEDevice::setPower(ESP_PWR_LVL_P9); /** +9db */

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
#endif
  
}

void deinitBLEDev()
{
    if (!NimBLEDevice::getInitialized())
        return;

    NimBLEDevice::deinit(true);
}

bool connectToWifiAndMQTT()
{
    if (WiFi.status() != WL_CONNECTED) {

        WiFi.begin(ssid, pass);
    
        // attempt to connect to WiFi network:
        Serial.print("Attempting to connect to WPA SSID: ");
        Serial.println(ssid);
    
        for(int i = 0; i < g_maxNumberOfConnectAttempts; i++) {
            if (WiFi.status() == WL_CONNECTED) {
                break;
            }
        
            // failed, retry
            Serial.print(".");
            delay(5000);
        }
    
        Serial.println();
        if (WiFi.status() != WL_CONNECTED) {
            return false;
        }

        Serial.println("You're connected to the network");
    }
    else {
        Serial.println("Still connected to the network");    
    }

    return connectToMqtt();
}

void turnOffWifi()
{

#if 0    
    if (WiFi.getMode() != WIFI_OFF) {
        Serial.println("WiFi disconnect");
        if (!WiFi.disconnect(true)) {
            Serial.println("WiFi disconnect failed!!");
        }
        delay(100);
        WiFi.mode(WIFI_OFF);
    }
#endif

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


static uint32_t g_oldCPUFrequency = 0; //

void setupModemSleep()
{
    assert(WiFi.getMode() != WIFI_OFF);
    WiFi.setSleep(true);
    g_oldCPUFrequency = getCpuFrequencyMhz();
    if (!setCpuFrequencyMhz(40)){
        Serial2.println("Not valid frequency!");
    }
    // Use this if 40Mhz is not supported
    // setCpuFrequencyMhz(80);
    delay(10);    
}

void resumeModemSleep() {
    assert(g_oldCPUFrequency != 0);
    setCpuFrequencyMhz(g_oldCPUFrequency);
    delay(10);
    WiFi.setSleep(false);
}

void enterModemSleep(unsigned long period)
{
    setupModemSleep();

    unsigned long startLoop = millis();
    for(;;) {
        if ((startLoop + period) < millis())
            break;
    }

    resumeModemSleep();
}

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
    while(!Serial) { delay(500); }

    Serial1.begin(115200);
    while(!Serial1) { delay(500); }

    Serial.println("Starting BLE Bridge ver:" APP_VERSION " Build:" APP_BUILD_DATE);
    Serial1.println("Starting BLE Bridge ver:" APP_VERSION " Build:" APP_BUILD_DATE);

    setupMqtt(mqtt_broker, mqtt_port);

    bleDeviceScanTimer.setTimeOutTime(nDevicesRefreshTime);
    bleDataScanTimer.setTimeOutTime(nDataRefreshTime);

    // check file system exists
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS failed.");
        while(true) {};
    }

    InitState0();
}

///////////////////////////////////////

void InitState0()
{
    nCurrentState = 0;
    WiFi.mode(WIFI_STA);
    if (!connectToWifiAndMQTT()) {
        Serial.println("Unable to connect to wifi.");
        delay(g_timeToRestartAfterConnectionFailed);

        ESP.restart();
    }

    delay(500);

    turnOffWifi();
    delay(500);    

    InitState1();
}

void UpdateState0()
{
    //nothing
}

///////////////////////////////////////
//TODO: what about devices that are not found ?



void onDeviceFound(NimBLEAdvertisedDevice* pDevice)
{
    if (pDevice == nullptr) {   //Scan end
        bleDeviceScanTimer.reset();

        //TODO:
        //Convert myDevicesList to ...
        //Write dev list to file


        InitState2();
        return;
    }
    else {
        myDevices.onDeviceFound(pDevice);
    }
}

void InitState1()
{
    nCurrentState = 1;
    turnOffWifi();
    delay(200);

    initBLEDev();
    delay(200);

    startBLEDevicesScan(nDeviceScanTimeout, onDeviceFound);
}

void UpdateState1()
{
    //Nothing...
}

///////////////////////////////////////

static int nUpdateListEmptyCounter = 0;

void InitState2()
{
    nCurrentState = 2;
    turnOffWifi();
    delay(200);

    initBLEDev();
    vTaskDelay(500 / portTICK_PERIOD_MS);
}

void UpdateState2()
{
    Serial.println("State: Read sensors data");

    if (myDevices.deviceCount() == 0) {
        Serial.println("Device list is empty!");      
        InitState1();
        return;
    }

    int count = myDevices.connectAndReadData(myUpdateList);
    if (count > 0) {
        nUpdateListEmptyCounter = 0;

        Serial.println("Update count " + String(myUpdateList.itemCount()));            
        InitState3();
    }
    else {
        Serial.println("Update list is empty.... enter sleep");

        nUpdateListEmptyCounter++;
        if (nUpdateListEmptyCounter > 5) {
            Serial.println("Rescan devices..");
            InitState1();
        }
        else {
            InitState4();
        }
    }
}

///////////////////////////////////////

void InitState3()
{
    nCurrentState = 3;
    Serial.println("State: WiFi and MQTT connect");

    deinitBLEDev();
    delay(2000);

    WiFi.mode(WIFI_STA);
    if (connectToWifiAndMQTT()) {

        delay(50);
    }
    else {
        Serial.println("Unable to connect to WiFi!");
        delay(2000);

        ESP.restart();
    }
}

void UpdateState3()
{
    MyBLEDevice* pDev = nullptr;
    for(int i = 0; i < 5; i++) {
        if (myUpdateList.isEmpty())
            break;

        pDev = myUpdateList.dequeue();
        assert(pDev);
        sendSensorDataToMQTT(pDev->devId(), pDev->getData());        
    }

    delay(10);  //to flush MQTT data...
    Serial.println();

    if (myUpdateList.isEmpty()) {

        count++;

        InitState4();
    }
    else {
        delay(50);
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

///////////////////////////////////////

void InitState5()
{
    Serial.flush();


#if 0
    WiFi.mode(WIFI_STA);
    if (connectToWifiAndMQTT()) {
        enterModemSleep(20000);


    }




        delay(50);
        sendStatusOnlineMsg();
    }
    else {
        Serial.println("Unable to connect to WiFi!");
        delay(2000);

        ESP.restart();
    }
#endif

}

void UpdateState5()
{

}

///////////////////////////////////////

void loop() 
{
    Serial1.println("Hello world..");

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
