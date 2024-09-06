#ifndef BLE_H
#define BLE_H

#include <BLEDevice.h>
#include "bms_jbd.h"

BLEScan* pBLEScan;
BLEScanResults found;

BLEClient* pClientBMS;
BLERemoteService* pServiceBMS;
BLERemoteCharacteristic* pChar_BMS_RX;
BLERemoteCharacteristic* pChar_BMS_TX;

bool client_connected = false;

bool send_ble_command(uint8_t* data, int length) {

    pChar_BMS_RX = pServiceBMS->getCharacteristic(BMS_RX_CHAR);

    if (client_connected) {
        pChar_BMS_TX->writeValue(data, length, true);
        return true;
    } else {
        return false;
    }
}

void bluetooth_task() {
    if(client_connected) {

        #ifdef JBD_BMS
            if(!send_ble_command(data_cell, sizeof(data_cell))) {
                client_connected = false;
            }

            if(!send_ble_command(data_info, sizeof(data_info))) {
                client_connected = false;
            }
        #endif

        vTaskDelay(1000);
        
    }
    vTaskDelay(1);
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

bool connect(BLEAddress device) {
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

    found = pBLEScan->getResults();
    
    printf("Found devices:\n");

    for (int i=0; i<found.getCount(); i++) {
        printf("%s\n", found.getDevice(i).toString().c_str());
	}
}

#endif
