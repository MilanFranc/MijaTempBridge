#include "utils.h"

#include <WiFi.h>
#include <WiFiClient.h>

#if !defined(WL_MAC_ADDR_LENGTH)
#define WL_MAC_ADDR_LENGTH  6
#endif

namespace utils {

void reverseBytes(const uint8_t* originBuffer, size_t size, uint8_t* reversedBuffer)
{
    for(int i = 0; i < size; i++) {
        reversedBuffer[i] = originBuffer[(size - 1) - i];
    }
}

String getMacAddrString()
{
    uint8_t macAddr[WL_MAC_ADDR_LENGTH] = {0};
    WiFi.macAddress(macAddr);

    String strMacAddr;
    for(int i = 0; i < 6; i++) {
        strMacAddr.concat(String(macAddr[i] >> 4, HEX));
        strMacAddr.concat(String(macAddr[i] & 0x0f, HEX));
    }    
    return strMacAddr;
}

String removeColons(const std::string& addr)
{
    int idx = 0;
    char buffer[16];
    for(int chr : addr) {
        if (chr != ':') {
            buffer[idx] = chr; idx++;
        }
    }
    buffer[idx] = '\0';
    return String(buffer, idx);
}



String getStringFrontItem(const String& topic, char delim)
{
    String result;
    int idx = topic.indexOf(delim);
    if (idx != -1) {
        result = topic.substring(0, idx);
    }
    return result;
}

String getStringBackItem(const String& topic, char delim)
{
    String result;
    int idx = topic.lastIndexOf(delim);
    if (idx != -1) {
        result = topic.substring(idx + 1);
    }
    return result;
}

String stdStringToStr(const std::string& str)
{
    return String(str.c_str(), str.size());
}




}
