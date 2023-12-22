
#include <Arduino.h>

void printBuffer(const char* buffer, int length)    //Debug
{
    int lineLen = 0;
    for(int i = 0; i< length; i++) {
  
      Serial.print(buffer[i] >> 4, HEX); lineLen++;
      Serial.print(buffer[i] & 0x0f, HEX); lineLen++;
      if (lineLen > 100) {
        lineLen = 0;
        Serial.println("");
      }
    }
    Serial.println("");
}

