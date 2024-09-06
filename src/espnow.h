#ifndef ESPNOW_H_
#define ESPNOW_H_

#include "ble.h"
#include <esp_now.h>
#include <WiFi.h>

// REPLACE WITH YOUR VESC Express MAC Address, found by running the companion Lisp script.
uint8_t expressAddress[] = { 0xC0, 0x4E, 0x30, 0x80, 0xC4, 0xC9 };

esp_now_peer_info_t peerInfo;

// callback when data is sent
void OnDataSent(const uint8_t* mac_addr, esp_now_send_status_t status) {
    #ifdef DEBUG
        Serial.print("\r\nLast Packet Send Status:\t");
        Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
    #endif
}

void espnow_task(bms_values_jbd *bms) {
    if (client_connected) {
        for (byte i = 1; i <= bms->cell_num; i++) {
            char data[8];
            snprintf(data, sizeof(data), "%.3f", (float)bms->cell_voltage[i - 1] / 1000);

            esp_err_t result = esp_now_send(expressAddress, (uint8_t*)data, sizeof(data));

            vTaskDelay(100 / portTICK_PERIOD_MS);

            if (result != ESP_OK) {
                printf("Failed to send ESP-NOW message\n");
            }
        }
    }
}

void espnow_init() {

    WiFi.mode(WIFI_STA);

    // Init ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }

    // Once ESPNow is successfully Init, we will register for Send CB to
    // get the status of Transmitted packet
    esp_now_register_send_cb(OnDataSent);

    // Register ESPNow peer
    memcpy(peerInfo.peer_addr, expressAddress, 6);
    peerInfo.channel = 1;
    peerInfo.encrypt = false;

    // Add ESPNow peer
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add peer");
        return;
    } 
}

#endif //ESPNOW_H_