
#include <NimBLEDevice.h>

#include <ArduinoQueue.h>
#include <ArduinoJson.h>
#include <NTPClient.h>

// Time library: // https://github.com/PaulStoffregen/Time
#include <TimeLib.h>


#define _TASK_SLEEP_ON_IDLE_RUN
#define _TASK_STATUS_REQUEST
#include <TaskScheduler.h>

//#include <freertos/task.h>


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

//const char* ntpServerName = "time.nist.gov";
//const char* ntpServerName = "pool.ntp.org";
const char* ntpServerName = "time.google.com";

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, ntpServerName);



#define BLE_DEVICES_LIST  "/BLEDevList1"

NimBLEScan* pBLEScan = NULL;

const unsigned long nDataRefreshTime = 15 * TASK_MINUTE;  // 15 minutes
const unsigned long nDevicesRefreshTime = 4 * 60 * TASK_MINUTE;   //4 hours
const unsigned long nRescanDevicesTime = 15 * TASK_MINUTE;   //15 minutes
const int nDeviceScanTimeout = 90; //seconds

int count = 0;
bool isBLEScanInProgress = false;

ArduinoQueue<MyBLEDevice*> myUpdateList(30);

BLESensorsMngr myDevices;

const int g_maxNumberOfConnectAttempts = 20;
const uint32_t g_timeToRestartAfterConnectionFailed = 5 * 60 * 1000;

void taskScanBLEDevices();
void taskScanBLECompleted();
void taskReadDataFromDevices();
void taskReportData();

bool onEnableReadDataFromDevices();

Scheduler ts;

Task tScanBLEDevices(nDevicesRefreshTime, TASK_FOREVER, &taskScanBLEDevices, &ts);
Task tScanBLECompleted(&taskScanBLECompleted, &ts);
Task tReadData(nDataRefreshTime, TASK_FOREVER, &taskReadDataFromDevices, &ts, false, &onEnableReadDataFromDevices);
Task tReportData(&taskReportData, &ts);

StatusRequest sScanDone;


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

    Serial.println("initBLEDev");  
}

void deinitBLEDev()
{
    if (!NimBLEDevice::getInitialized())
        return;

    clearBLEScanData();
    NimBLEDevice::deinit(true);
    Serial.println("deinitBLEDev");
}

bool connectToWifiAndMQTT(uint16_t numberOfAttempts)
{
    if (WiFi.status() != WL_CONNECTED) {

        WiFi.begin(ssid, pass);
    
        // attempt to connect to WiFi network:
        Serial.print("Attempting to connect to WPA SSID: ");
        Serial.println(ssid);
    
        for(uint16_t i = 0; i < numberOfAttempts; i++) {
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



void reportBLEDevicesToMQTT()
{
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    for(size_t i = 0; i < myDevices.size(); i++) {
        MyBLEDevice* pDev = myDevices.deviceAt(i);
        assert(pDev != nullptr);

        JsonObject obj = arr.add<JsonObject>();
        obj["mac"] = pDev->addr();
//        obj["con"] = pDev->connectCount();
//        obj["fail"] = pDev->failedConnectCount();
    }

    String jsonData;
    serializeJson(doc, jsonData);

    sendSensorsListToMQTT(jsonData);
}

time_t NTPTimeProvider()
{
    timeClient.update();
    return timeClient.getEpochTime();
}

/////////////////////////////////////////////////////////////////////////////////////

void setup()
{
    Serial.begin(115200);
    while(!Serial) { delay(10); }

    Serial.println("Starting BLE Bridge ver:" APP_VERSION " Build:" APP_BUILD_DATE);

    setupMqtt(mqtt_broker, mqtt_port);

    WiFi.mode(WIFI_STA);
    if (!connectToWifiAndMQTT(g_maxNumberOfConnectAttempts)) {
        Serial.println("Unable to connect to wifi.");
        delay(g_timeToRestartAfterConnectionFailed);

        ESP.restart();
    }

    delay(500);

    timeClient.begin();
    timeClient.update();

    //TimeLib.h
    setSyncProvider(NTPTimeProvider);    

    tScanBLEDevices.enableDelayed(100);  //Start BLE Scan
}

///////////////////////////////////////
//TODO: what about devices that are not found ?


void onDeviceFound(NimBLEAdvertisedDevice* pDevice)
{
    if (pDevice == nullptr) {   //Scan end

        //TODO:
        //Convert myDevicesList to ...
        //Write dev list to file
        Serial.println("Found:" + String(myDevices.size()) + " devices.");

        if (myDevices.size() > 0) {

            sScanDone.signalComplete(0);  //Status OK
        }
        else {
            sScanDone.signalComplete(-1); //Status Devices not found.
        }
    }
    else {
        myDevices.onDeviceFound(pDevice);
    }
}

void taskScanBLEDevices()
{
    Serial.println("Task: scan BLE devices   Time:" + String(now()));

    initBLEDev();
    delay(200);

    isBLEScanInProgress = true;
    sScanDone.setWaiting();

    startBLEDevicesScan(nDeviceScanTimeout, onDeviceFound);

    tScanBLECompleted.waitFor(&sScanDone);
}

void taskScanBLECompleted()
{
    Serial.println("Task: Scan completed");
    isBLEScanInProgress = false;

    if ( sScanDone.getStatus() >= 0) { 

        reportBLEDevicesToMQTT();

        tReadData.enableDelayed(100);
    }
    else {

        delay(20);
        deinitBLEDev();

        Serial.println("Restart BLE scan...");
        tScanBLEDevices.enableDelayed(nRescanDevicesTime);   //re-start Scan BLE devices task.
    }
}

///////////////////////////////////////

bool onEnableReadDataFromDevices()
{
    return !isBLEScanInProgress;
}

void taskReadDataFromDevices()
{
    Serial.println("Task: Read sensors data   Time:" + String(now()));

    if (myDevices.size() == 0) {
        Serial.println("Device list is empty!");
        return;
    }

    initBLEDev();
    vTaskDelay(500 / portTICK_PERIOD_MS);

    int count = myDevices.connectAndReadData(myUpdateList);

    delay(20);
    deinitBLEDev();

    if (count > 0) {
        Serial.println("Update count " + String(myUpdateList.itemCount()));            

        tReportData.restartDelayed(50);
    }
    else {
        Serial.println("Update list is empty.... enter sleep");

        //TODO: add some re-read devices data...
        tReadData.restartDelayed(5 * TASK_MINUTE);

    }

    Serial.println("Task: done.");
}

///////////////////////////////////////

void taskReportData()
{
    Serial.println("Task: Report data to MQTT    Time:" + String(now()));

    if (!connectToWifiAndMQTT(g_maxNumberOfConnectAttempts)) {
        Serial.println("Err: Unable to connect to WiFi!");
        
        ts.currentTask().restartDelayed(5 * TASK_MINUTE);
        return;
    }

    for(int i = 0; i < myUpdateList.maxQueueSize(); i++) {
        if (myUpdateList.isEmpty())
            break;

        MyBLEDevice* pDev = myUpdateList.dequeue();
        assert(pDev);
        sendSensorDataToMQTT(pDev->devId(), pDev->getData());
    }
    count++;

    delay(10);  //to flush MQTT data...
    Serial.println("Task: done.");
}

///////////////////////////////////////

void loop() 
{
    ts.execute();

    timeClient.update();

    //TODO: ArduinoOTA.handle();

}
