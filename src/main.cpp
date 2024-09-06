#include "Arduino.h"
#include "ble.h"
#include "espnow.h"

void setup() 
{
    Serial.begin(115200);

    espnow_init();

    bluetooth_init();

    connect(BLEAddress("a5:c2:37:13:e0:48"));

}

void loop() {

    bluetooth_task();

    espnow_task(&bms_data);

}

    

