
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

#include <ArduinoMqttClient.h>
#if defined(ARDUINO_SAMD_MKRWIFI1010) || defined(ARDUINO_SAMD_NANO_33_IOT) || defined(ARDUINO_AVR_UNO_WIFI_REV2)
  #include <WiFiNINA.h>
#elif defined(ARDUINO_SAMD_MKR1000)
  #include <WiFi101.h>
#elif defined(ARDUINO_ARCH_ESP8266)
  #include <ESP8266WiFi.h>
#elif defined(ARDUINO_PORTENTA_H7_M7) || defined(ARDUINO_NICLA_VISION) || defined(ARDUINO_ARCH_ESP32)
  #include <WiFi.h>
#endif

#include "MiTempDev.h"


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



WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

const char broker[] = MQTT_BROKER_ADDR;
int        port     = 1883;
const char topicPrefix[]  = "MiTemp/";
const char topicType[]  = "data";

SoftTimer bleDeviceScanTimer;
SoftTimer bleDataScanTimer;

const unsigned long nDataRefreshTime = 15 * 60 * 1000;  // 15 minutes
const unsigned long nDevicesRefreshTime = 4 * 60 * 60 * 1000;   //4 hours
const int nDeviceScanTimeout = 30; //seconds


const int STATE_IDLE = 0;
const int STATE_READ_BLE_DATA = 1;
const int STATE_CONNECT_WIFI_AND_MQTT = 2;
const int STATE_SEND_MQTT_DATA = 3;
const int STATE_DISCONNECT_WIFI = 4;


int currentState = STATE_IDLE;

int count = 0;


std::vector<MiTempDev*> myDevicesList;

ArduinoQueue<MiTempDev*> myUpdateList(30);


/////////////////////////////////////////////////////////////////////

void initBLEDev()
{
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

  // You can provide a unique client ID, if not set the library uses Arduino-millis()
  // Each client must have a unique client ID
  // mqttClient.setId("clientId");

  // You can provide a username and password for authentication
  // mqttClient.setUsernamePassword("username", "password");

  if (!mqttClient.connected()) {

    Serial.print("Attempting to connect to the MQTT broker: ");
    Serial.println(broker);

    if (!mqttClient.connect(broker, port)) {
      Serial.print("MQTT connection failed! Error code = ");
      Serial.println(mqttClient.connectError());
  
      return false;
    }

    Serial.println("You're connected to the MQTT broker!");
    Serial.println();
  }
  else {
    Serial.println("Still connected to the MQTT broker!");
  }

  return true;
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

void createMiTempDevice(NimBLEAdvertisedDevice* pAdvDevice)
{
  bool found = false;
  NimBLEAddress addr = pAdvDevice->getAddress();
  for (MiTempDev* item : myDevicesList) {
      if (std::string(item->addr()) == addr.toString()) {
          found = true;
          break;
      }
  }

  if (!found) {
      MiTempDev* pMiTempDev = new MiTempDev(addr.toString().c_str());

      const uint8_t* pNativeAddress = addr.getNative();
      uint8_t reversedBuffer[6] = {0};
      for(int i = 0; i < 6; i++) {
         reversedBuffer[i] = pNativeAddress[(6 - 1) - i];
      }
      
      uint8_t buffer[16] = {0};
      char* pHexAddress = NimBLEUtils::buildHexData(buffer, reversedBuffer, 6);
      pMiTempDev->setDevId(pHexAddress);
      
      myDevicesList.push_back(pMiTempDev);
  }
}

void readDeviceName(NimBLEClient* pClient, MiTempDev* pDev)
{
    NimBLERemoteService* pService = pClient->getService(NimBLEUUID((uint16_t)0x1800));
    if (pService != nullptr) {
      std::string val = pService->getValue(NimBLEUUID((uint16_t)0x2a00));
      pDev->setName(val.c_str());
      Serial.println("Name:" + String(val.c_str()));

    }
}

void readDeviceBatteryLevel(NimBLEClient* pClient, MiTempDev* pDev)
{
    NimBLERemoteService* pService = pClient->getService(NimBLEUUID((uint16_t)0x180F));
    if (pService != nullptr) {
      NimBLERemoteCharacteristic* pChar = pService->getCharacteristic(NimBLEUUID((uint16_t)0x2a19));
      if (pChar) {
        uint8_t val = pChar->readValue<uint8_t>();
        pDev->setBatteryLevel(val);
        Serial.println("Battery:" + String(val));      
      }
    }
}

static int g_notify = 0;
MiTempDev* pActualMiTempDev = NULL;

void my_notify_callback(NimBLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify)
{
    Serial.print("subscribe notify. read:");  
    std::string strData((char*)pData, length);
    Serial.println(strData.c_str());

    if (pActualMiTempDev) {
      pActualMiTempDev->setBLEData(pData, length);
    }

    g_notify++;
}

void readDeviceData(NimBLEClient* pClient)
{
    NimBLERemoteService* pService = pClient->getService(NimBLEUUID("226c0000-6476-4566-7562-66734470666d"));
    if (pService != nullptr) {
      NimBLERemoteCharacteristic* pChar = pService->getCharacteristic(NimBLEUUID("226caa55-6476-4566-7562-66734470666d"));    //   
      if (pChar != nullptr) {

          g_notify = 0;
          pChar->subscribe(true, my_notify_callback, true);
          for(int i = 0; i < 40 && g_notify == 0; i++) {
              delay(100);
              Serial.print(".");
          }
          Serial.println();

          if (g_notify == 0) {
            Serial.println("Timeout.");

            delay(1200);
            pChar->unsubscribe();
            return;
          }

          myUpdateList.enqueue(pActualMiTempDev);

#if 0 //temp remove        
          NimBLERemoteDescriptor* pDesc = pChar->getDescriptor(NimBLEUUID((uint16_t)0x2902));
          if (pDesc) {
            Serial.println("Desc:" + String(pDesc->toString().c_str()));

            pDesc->writeValue(nullptr, 0, true);
    
            Serial.println("Val:" + String(pDesc->readValue().c_str()));
  //          uint16_t temp = pDesc->readValue<uint16_t>();
  //          Serial.println("Val:" + String(temp));
          }
#endif          

          delay(1200);
          pChar->unsubscribe();
          
      }

    }
  
}

void connectAndReadDevice(MiTempDev* pDev)
{
    NimBLEAddress addr(pDev->addr());
    NimBLEClient* pClient = nullptr;

    if (NimBLEDevice::getClientListSize() > 0) {
        /** Special case when we already know this device, we send false as the
         *  second argument in connect() to prevent refreshing the service database.
         *  This saves considerable time and power.
         */
        pClient = NimBLEDevice::getClientByPeerAddress(addr);
        if (pClient) {
            Serial.println("Connecting to " + String(addr.toString().c_str()) + " - exist client");
            if (!pClient->connect(addr, false)) {
                Serial.println("Reconnect failed");
                return;
            }
            Serial.println("Reconnected client");
        }
        else {
            pClient = NimBLEDevice::getDisconnectedClient();
        }
    }

    if (!pClient) {

        if (NimBLEDevice::getClientListSize() >= NIMBLE_MAX_CONNECTIONS) {
            Serial.println("Max clients reached - no more connections available");
            return;
        }

        pClient = NimBLEDevice::createClient(addr);

        Serial.println("Connecting to " + String(addr.toString().c_str()) + " - new client");
        pClient->setConnectTimeout(30);
        
        if (!pClient->connect()) {

            /** Created a client but failed to connect, don't need to keep it as it has no data */
            NimBLEDevice::deleteClient(pClient);
            Serial.println("Failed to connect, deleted client");
            return;          
        }
    }

    if (!pClient->isConnected()) {
        Serial.println("Connecting to " + String(addr.toString().c_str()));
        if (!pClient->connect(addr)) {
            Serial.println("Failed to connect");
            return;
        }
    }

    if (pClient->isConnected()) {
      pActualMiTempDev = pDev;
      
      readDeviceName(pClient, pDev);
      readDeviceBatteryLevel(pClient, pDev);

      readDeviceData(pClient);
      Serial.println("Close.");

      delay(100);
      pClient->disconnect();
      
    }

    pActualMiTempDev = NULL;
}


void sendMiTempDataToMQTT(MiTempDev* pDev)
{
    String topic = topicPrefix;
    topic += pDev->devId();
    topic += "/";
    topic += topicType;

    Serial.print("Sending message to topic: ");
    Serial.println(topic);        
  
    // send message, the Print interface can be used to set the message contents
    mqttClient.beginMessage(topic);

    StaticJsonDocument<200> doc;
    doc["temp"] = pDev->getTemp();
    doc["humidity"] = pDev->getHumidity();
    doc["batt"] = pDev->getBatteryLevel();

    String jsonData;
    serializeJson(doc, jsonData);

    Serial.print("data:");
    Serial.println(jsonData);

    mqttClient.print(jsonData);
    mqttClient.endMessage();
  
}

/////////////////////////////////////////////////////////////////////////////////////

void setup() {
    Serial.begin(115200);
    Serial.println("Starting BLE Bridge");

    WiFi.mode(WIFI_STA);

    bleDeviceScanTimer.setTimeOutTime(nDevicesRefreshTime);
    bleDataScanTimer.setTimeOutTime(nDataRefreshTime);
    
    initBLEDev();
    scanForDevices(nDeviceScanTimeout);

    bleDeviceScanTimer.reset();
    bleDataScanTimer.reset();
}

void loop() {

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
            mqttClient.poll();
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

        mqttClient.flush();
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
}
