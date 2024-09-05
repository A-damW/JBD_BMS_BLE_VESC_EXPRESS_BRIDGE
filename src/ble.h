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
#endif

bool client_connected = false;

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
