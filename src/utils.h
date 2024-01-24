#pragma once

#include <stdint.h>
#include <stddef.h>
#include <Arduino.h>

namespace utils {

void reverseBytes(const uint8_t* originBuffer, size_t size, uint8_t* reversedBuffer);
String getMacAddrString();
String removeColons(const std::string& addr);


String getStringFrontItem(const String& topic, char delim);
String getStringBackItem(const String& topic, char delim);
String getStringItem(const String& topic, char delim, int index);

String stdStringToStr(const std::string& str);

}