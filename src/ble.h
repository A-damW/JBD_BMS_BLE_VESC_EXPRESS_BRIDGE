#ifndef BLE_H
#define BLE_H

#include <BLEDevice.h>
#include "datatypes.h"
#include "buffer.h"
#include "utils.h"

BLEScan* pBLEScan;
BLEScanResults found;

BLEClient* pClientBMS;
BLERemoteService* pServiceBMS;
BLERemoteCharacteristic* pChar_BMS_RX;
BLERemoteCharacteristic* pChar_BMS_TX;

#ifdef JBD_BMS
const std::string BMS_SERVICE = "0000ff00-0000-1000-8000-00805f9b34fb";
const std::string BMS_RX_CHAR = "0000ff02-0000-1000-8000-00805f9b34fb";
const std::string BMS_TX_CHAR = "0000ff01-0000-1000-8000-00805f9b34fb";
bms_values_jbd bms_data;
#endif

bool client_connected = false;

void jbd_request_pack_info() {
    uint8_t data_info[7] = {0xdd, 0xa5, 0x3, 0x0, 0xff, 0xfd, 0x77};
    int lenght = 7;

    pChar_BMS_RX = pServiceBMS->getCharacteristic(BMS_RX_CHAR);
    pChar_BMS_RX->writeValue(data_info, lenght);
}

void jbd_request_cell_info() {
    uint8_t data_cell[7] = {0xdd, 0xa5, 0x4, 0x0, 0xff, 0xfc, 0x77};
    int lenght = 7;

    pChar_BMS_RX = pServiceBMS->getCharacteristic(BMS_RX_CHAR);
    pChar_BMS_RX->writeValue(data_cell, lenght);
}

void jbd_process_pack_info(uint8_t *data, uint32_t dataSize, bms_values_jbd *bms) {
    int32_t index = 0;

    index += 4;

    bms->voltage = buffer_get_float16(data, 100, &index);
    bms->current = buffer_get_float16(data, 100, &index);

    bms->ah_now = buffer_get_float16(data, 100, &index);
    bms->ah_total = buffer_get_float16(data, 100, &index);
    bms->cycle_number = buffer_get_uint16(data, &index);
    bms->production_date = buffer_get_uint16(data, &index);

    bms->balance_state = buffer_get_uint16(data, &index);
    bms->balance_state_high = buffer_get_uint16(data, &index);
    bms->protection_status = buffer_get_uint16(data, &index);

    index++; //Skip software version

    bms->soc = data[index++];
    bms->output_status = data[index++];
    bms->cell_num = data[index++];

    bms->temp_num = data[index++];
    index += 4; // Skip unwanted values
    bms->temp1 = (2731 + buffer_get_uint16(data, &index)) / 100.0;
    bms->temp2 = (2731 + buffer_get_uint16(data, &index)) / 100.0;
}


void jbd_process_cell_info(const uint8_t *data, uint32_t dataSize, bms_values_jbd *bms) {
    if (data == NULL || dataSize < 3) {
        return;
    }

    int32_t index = 3;

    if (index + 1 >= dataSize) {
        return;
    }

    bms->cell_num = data[index++];
    bms->cell_num /= 2;

    if (bms->cell_num > 20 || index + bms->cell_num * 2 > dataSize) {
        return;
    }

    uint16_t voltages[20];
    uint16_t min_voltage = UINT16_MAX;
    uint16_t max_voltage = 0;

    for (int i = 0; i < bms->cell_num; i++) {
        voltages[i] = buffer_get_uint16(data, &index);
        bms->cell_voltage[i] = voltages[i];
        if (voltages[i] > max_voltage) {
            max_voltage = voltages[i];
        }
        if (voltages[i] < min_voltage) {
            min_voltage = voltages[i];
        }
    }

    bms->cell_max = max_voltage;
    bms->cell_min = min_voltage;
    bms->cell_diff = max_voltage - min_voltage;
    bms->cell_avg = calculate_average(voltages, bms->cell_num);
    bms->cell_median = calculate_median(voltages, bms->cell_num);
}

#define MAX_PACKET_SIZE 128
#define START_BYTE 0xDD
#define STOP_BYTE 0x77
#define COMMAND_INFO 0x03
#define COMMAND_CELL 0x04

bool jbd_packet_process(uint8_t byte) {
    static uint8_t packetBuffer[MAX_PACKET_SIZE] = {0x0};
    static uint8_t currentPos = 0;
    static bool receivingPacket = false;
    if (byte == START_BYTE && !receivingPacket) {
        receivingPacket = true;
        currentPos = 0;
        packetBuffer[currentPos++] = byte;
    } else if (receivingPacket) {
        packetBuffer[currentPos++] = byte;
        if (currentPos >= MAX_PACKET_SIZE) {
            receivingPacket = false;
            currentPos = 0;
            return false;
        }
        if (byte == STOP_BYTE) {
            receivingPacket = false;
            if (currentPos >= 6) {
                int crc = 0;
                for (int i = 3; i < currentPos - 3; i++) {
                    crc += packetBuffer[i];
                }
                crc = ~crc & 0xFF;
                crc = (crc + 1) & 0xFF;
                int receivedCrc = packetBuffer[currentPos - 2];
                if (crc == receivedCrc) {
                    int command = packetBuffer[1];
                    if (command == COMMAND_INFO) {
                        jbd_process_pack_info(packetBuffer, currentPos, &bms_data);
                    } else if (command == COMMAND_CELL) {
                        jbd_process_cell_info(packetBuffer, currentPos, &bms_data);
                    }
                } else {
                    printf("CRC invalid! Calculated: %02X, Received: %02X\n", crc, receivedCrc);
                }
            }
            currentPos = 0;
            return true;
        }
    }
    return false;
}

class ClientCallbacks : public BLEClientCallbacks {
    void onConnect(BLEClient* pclient) {
    }

    void onDisconnect(BLEClient* pclient) {
        client_connected = false;
    }
};

void notifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify) {
    for (int i = 0; i < length; ++i) {
        #ifdef JBD_BMS
        jbd_packet_process(pData[i]);
        #endif
    }
}

bool connectJBD(BLEAddress device) {
    if (client_connected) {
        pClientBMS->disconnect();
        client_connected = false;
        vTaskDelay(1000);
        //return false;
    }    

    pClientBMS = BLEDevice::createClient();
    pClientBMS->setClientCallbacks(new ClientCallbacks());

    if (!pClientBMS->connect(device) ||
        !(pServiceBMS = pClientBMS->getService(BMS_SERVICE)) ||
        !(pChar_BMS_RX = pServiceBMS->getCharacteristic(BMS_RX_CHAR)) ||
        !(pChar_BMS_TX = pServiceBMS->getCharacteristic(BMS_TX_CHAR)) ||
        !pClientBMS->setMTU(23)) {
        pClientBMS->disconnect();
        return false;
    }

    pChar_BMS_TX->registerForNotify(notifyCallback);
    client_connected = true;
    return true;
}

void bluetooth_init() {

    BLEDevice::init("");

    pBLEScan = BLEDevice::getScan();
    pBLEScan->setActiveScan(false);
    pBLEScan->setInterval(1349);
    pBLEScan->setWindow(449);
    pBLEScan->start(1);

    for (int i=0; i<found.getCount(); i++) {
        printf("%s\n", found.getDevice(i).toString().c_str());
	}
}

#endif
