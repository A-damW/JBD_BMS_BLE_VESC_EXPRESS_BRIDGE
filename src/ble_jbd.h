#ifndef BLUETOOTH_JBD_H_
#define BLUETOOTH_JBD_H_

#include <BLEDevice.h>
#include <float.h>

const std::string BMS_SERVICE = "0000ff00-0000-1000-8000-00805f9b34fb";
const std::string BMS_RX_CHAR = "0000ff02-0000-1000-8000-00805f9b34fb";
const std::string BMS_TX_CHAR = "0000ff01-0000-1000-8000-00805f9b34fb";

bool connectedtojbd = false;



jbd_pack_info jbd_pack;
jbd_cell_info jbd_cell;

void jbd_process_pack_info(uint8_t *data, uint32_t dataSize) {
    int32_t index = 0;

    index += 4;

    jbd_pack.totalVoltage = buffer_get_float16(data, 100, &index);
    jbd_pack.current = buffer_get_float16(data, 100, &index);

    jbd_pack.balanceCapacity = buffer_get_float16(data, 100, &index);
    jbd_pack.ratedCapacity = buffer_get_float16(data, 100, &index);
    jbd_pack.cycleCount = buffer_get_uint16(data, &index);
    jbd_pack.productionDate = buffer_get_uint16(data, &index);

    //uint8_t day = jbd_pack.productionDate & 0x1F;
    //uint8_t month = (jbd_pack.productionDate >> 5) & 0x0F;
    //uint16_t year = 2000 + ((jbd_pack.productionDate >> 9) & 0x7F);

    jbd_pack.balanceStatus = buffer_get_uint16(data, &index);
    jbd_pack.balanceStatusHigh = buffer_get_uint16(data, &index);
    jbd_pack.protectionStatus = buffer_get_uint16(data, &index);

    index++; //Skip software version

    jbd_pack.SOC = data[index++];
    jbd_pack.fetControlStatus = data[index++];
    jbd_pack.batterySeriesNum = data[index++];
    jbd_pack.ntcNumber = data[index++];

    index += 4;
    jbd_pack.ntc1 = (2731 + buffer_get_uint16(data, &index)) / 100.0;
    jbd_pack.ntc2 = (2731 + buffer_get_uint16(data, &index)) / 100.0;
}

void jbd_process_cell_info(const uint8_t *data, uint32_t dataSize) {
    if (data == NULL || dataSize < 3) {
        return;
    }

    int32_t index = 3;

    if (index + 1 >= dataSize) {
        return;
    }

    jbd_cell.cell_count = data[index++];
    jbd_cell.cell_count /= 2;

    if (jbd_cell.cell_count > 20 || index + jbd_cell.cell_count * 2 > dataSize) {
        return;
    }

    uint16_t voltages[20];
    uint16_t min_voltage = UINT16_MAX;
    uint16_t max_voltage = 0;

    for (int i = 0; i < jbd_cell.cell_count; i++) {
        voltages[i] = buffer_get_uint16(data, &index);
        jbd_cell.cell_voltage[i] = voltages[i];
        if (voltages[i] > max_voltage) {
            max_voltage = voltages[i];
        }
        if (voltages[i] < min_voltage) {
            min_voltage = voltages[i];
        }
    }

    jbd_cell.cell_max = max_voltage;
    jbd_cell.cell_min = min_voltage;
    jbd_cell.cell_diff = max_voltage - min_voltage;
    jbd_cell.cell_avg = calculate_average(voltages, jbd_cell.cell_count);
    jbd_cell.cell_median = calculate_median(voltages, jbd_cell.cell_count);
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
                        jbd_process_pack_info(packetBuffer, currentPos);
                    } else if (command == COMMAND_CELL) {
                        jbd_process_cell_info(packetBuffer, currentPos);
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

class JBDClientCallbacks : public BLEClientCallbacks {
    void onConnect(BLEClient* pclient) {
    }

    void onDisconnect(BLEClient* pclient) {
        connectedtojbd = false;
    }
};

void notifyCallbackJBD(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify) {
    for (int i = 0; i < length; ++i) {
        jbd_packet_process(pData[i]);
    }
}

bool connectJBD(BLEAddress device) {
    BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT);
    BLEDevice::setSecurityCallbacks(new MySecurity());

    auto pSecurity = new BLESecurity();
    pSecurity->setAuthenticationMode(ESP_LE_AUTH_NO_BOND);
    pSecurity->setCapability(ESP_IO_CAP_NONE);
    pSecurity->setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

    if (connectedtojbd) {
        pClientBMS->disconnect();
        connectedtojbd = false;
        return false;
    }    

    pClientBMS = BLEDevice::createClient();
    pClientBMS->setClientCallbacks(new JBDClientCallbacks());

    if (!pClientBMS->connect(device) ||
        !(pServiceBMS = pClientBMS->getService(BMS_SERVICE)) ||
        !(pChar_BMS_RX = pServiceBMS->getCharacteristic(BMS_RX_CHAR)) ||
        !(pChar_BMS_TX = pServiceBMS->getCharacteristic(BMS_TX_CHAR)) ||
        !pClientBMS->setMTU(23)) {
        pClientBMS->disconnect();
        return false;
    }

    pChar_BMS_TX->registerForNotify(notifyCallbackJBD);
    connectedtojbd = true;
    return true;
}

#endif  // BLUETOOTH_JBD_H_