#include "Arduino.h"
#include "ble.h"

void setup() 
{
    Serial.begin(115200);

    bluetooth_init();

    //connectJBD(BLEAddress("a5:c2:37:13:e0:48"));
}

void loop() {}

    

