#include "WiFiModemSleep.h"

#include <Arduino.h>
#include <assert.h>

#if defined(ARDUINO_SAMD_MKRWIFI1010) || defined(ARDUINO_SAMD_NANO_33_IOT) || defined(ARDUINO_AVR_UNO_WIFI_REV2)
  #include <WiFiNINA.h>
#elif defined(ARDUINO_SAMD_MKR1000)
  #include <WiFi101.h>
#elif defined(ARDUINO_ARCH_ESP8266)
  #include <ESP8266WiFi.h>
#elif defined(ARDUINO_PORTENTA_H7_M7) || defined(ARDUINO_NICLA_VISION) || defined(ARDUINO_ARCH_ESP32)
  #include <WiFi.h>
#endif


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
