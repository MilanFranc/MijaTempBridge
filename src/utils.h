#pragma once

#include <stdint.h>
#include <stddef.h>
#include <Arduino.h>

namespace utils {

void reverseBytes(const uint8_t* originBuffer, size_t size, uint8_t* reversedBuffer);
String getMacAddrString();

}