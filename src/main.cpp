#include "Arduino.h"
#include "ble.h"

void show(bms_values_jbd *bms) {
        for(int i = 0; i < bms->cell_num; i++) {
            printf("Cell %d: %.3f\n", i, bms->cell_voltage[i]/1000);
        }
}

void setup() 
{
    Serial.begin(115200);

    bluetooth_init();

    connectJBD(BLEAddress("a5:c2:37:13:e0:48"));

}

void loop() {

    if(client_connected) {
        jbd_request_cell_info();
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        show(&bms_data);
    }
}

    

