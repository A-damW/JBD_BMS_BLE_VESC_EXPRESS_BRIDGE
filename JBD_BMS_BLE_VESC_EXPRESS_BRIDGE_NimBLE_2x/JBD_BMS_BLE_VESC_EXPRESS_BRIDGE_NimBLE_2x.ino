
/* JBD_BMS_BLE_VESC_EXPRESS_BRIDGE:
 *
 *  An ESP32 firmware and companion LispBM script
 *  to connect multiple JBD BLE BMSs to VESC-EXPRESS via ESPNOW
 *
 *  Created: on September 4 2024
 *  Author: A-damW, https://github.com/A-damW
 *
*/

#include <NimBLEDevice.h>
#include <esp_now.h>
#include <WiFi.h>
#include "mydatatypes.h"

// ----------------- Begin user config -----------------

#define DEBUG true // flag to turn on or off debugging (Serial messages)
#define commSerial Serial
#define commSerial if(DEBUG)Serial

// List of JBD BMS BLE addresses, this list is exclusive and ordinal.
// The empty list: bmsBLEMacAddressesFilter[]={}; will allow any jbd bms MAC addresses (Max 3 simultaneous bms connections by default) to connect
// change to max 9 connections here: /home/USERNAME/Arduino/libraries/NimBLE-Arduino/src/nimconfig.h
std::string bmsBLEMacAddressesFilter[] = {}; //Will connect to ANY JBD BMS in ble range

//std::string bmsBLEMacAddressesFilter[] = { "a5:c2:37:18:c9:e1" }; //Dewalt?
//std::string bmsBLEMacAddressesFilter[] = { "a5:c2:37:18:c9:e1" , "a5:c2:37:06:c7:36" }; //works
//std::string bmsBLEMacAddressesFilter[] = { "a5:c2:37:06:c7:36" , "a5:c2:37:18:c9:e1", "a5:c2:37:18:d3:fb" }; //works



// Enter your VESC Express MAC Address, found by running the companion Lisp script.
// Example:
// This is the ESP-NOW broadcast address, both decimal and hex notation work.
//uint8_t expressAddress[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
uint8_t expressAddress[] = { 255, 255, 255, 255, 255, 255 };


//uint8_t expressAddress[] = { 0xC0, 0x4E, 0x30, 0x80, 0xC4, 0xC9 }; //Dewalt?
//uint8_t expressAddress[] = { 236, 218, 59, 187, 44, 221 }; //test mini's
//uint8_t expressAddress[] = { 0xEC, 0xDA, 0x3B, 0xBB, 0x2C, 0xDD }; //test mini's




// ----------------- End user config -----------------


// ----------------- Begin Declarations -----------------

const NimBLEAdvertisedDevice* advDevice;

// /** Create a single global instance of the callback class to be used by all clients */
// static ClientCallbacks clientCB;

static bool doConnect = false;
static uint32_t scanTimeMs = 5000; /** scan time in milliseconds, 0 = scan forever */

packBasicInfoStruct packBasicInfo;  //structures for BMS data
packEepromStruct packEeprom;        //structures for BMS data
packCellInfoStruct packCellInfo;    //structures for BMS data

const byte cBasicInfo3 = 3;  //type of packet 3= basic info
const byte cCellInfo4 = 4;   //type of packet 4= individual cell info

// Notifications from this characteristic is received data from BMS
// 0000ff01-0000-1000-8000-00805f9b34fb
// NOTIFY, READ
// Write to this characteristic (charUUID_tx) to send data to BMS.
// The data will be returned on the charUUID_rx characteristic.
// 0000ff02-0000-1000-8000-00805f9b34fb
// READ, WRITE, WRITE NO RESPONSE
static BLEUUID serviceUUID("0000ff00-0000-1000-8000-00805f9b34fb");  //xiaoxiang/JBD bms original module
static BLEUUID charUUID_rx("0000ff01-0000-1000-8000-00805f9b34fb");  //xiaoxiang/JBD bms original module
static BLEUUID charUUID_tx("0000ff02-0000-1000-8000-00805f9b34fb");  //xiaoxiang/JBD bms original module

bool newPacketReceived = false;

esp_now_peer_info_t peerInfo;

// ----------------- End Declarations -----------------


// callback when data is sent via esp-now
void OnDataSent(const uint8_t* mac_addr, esp_now_send_status_t status) {
  // Serial.print("\r\nLast Packet Send Status:\t");
  // Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}


/**  None of these are required as they will be handled by the library with defaults. **
 **                       Remove as you see fit for your needs                        */
class ClientCallbacks : public NimBLEClientCallbacks {

  void onConnect(NimBLEClient* pClient) override {
    commSerial.println("Connected");
  };

  void onDisconnect(NimBLEClient* pClient, int reason) override {
    commSerial.printf("%s Disconnected, reason = %d - Starting scan\n", pClient->getPeerAddress().toString().c_str(), reason);
    NimBLEDevice::getScan()->start(scanTimeMs, false, true);
  };

  /** Called when the peripheral requests a change to the connection parameters.
     *  Return true to accept and apply them or false to reject and keep
     *  the currently used parameters. Default will return true.
     */
  bool onConnParamsUpdateRequest(NimBLEClient* pClient, const ble_gap_upd_params* params) {
    if (params->itvl_min < 24) { /** 1.25ms units */
      return false;
    } else if (params->itvl_max > 40) { /** 1.25ms units */
      return false;
    } else if (params->latency > 2) { /** Number of intervals allowed to skip */
      return false;
    } else if (params->supervision_timeout > 100) { /** 10ms units */
      return false;
    }

    return true;
  };

}clientCallbacks;


/** Define a class to handle the callbacks when advertisments are received */
class ScanCallbacks : public NimBLEScanCallbacks {

  void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override {
    commSerial.print("Advertised Device found: ");
    commSerial.println(advertisedDevice->toString().c_str());
    if (advertisedDevice->isAdvertisingService(NimBLEUUID(serviceUUID))) {
      commSerial.println("Found Our Service");
      /** stop scan before connecting */
      NimBLEDevice::getScan()->stop();
      /** Save the device reference in a global for the client to use*/
      advDevice = advertisedDevice;
      /** Ready to connect now */
      doConnect = true;

      commSerial.printf("Adding %s to whitelist\n", std::string(advertisedDevice->getAddress()).c_str());
      NimBLEDevice::whiteListAdd(advertisedDevice->getAddress());
    }
  }

  // void onScanEnd(const NimBLEScanResults& results, int reason) override {
  //   Serial.printf("Scan Ended, reason: %d, device count: %d; Restarting scan\n", reason, results.getCount());
  //   NimBLEDevice::getScan()->start(scanTimeMs, false, true);
  // }

}scanCallbacks;


/** Notification / Indication receiving handler callback */
void notifyCB(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  // std::string str = (isNotify == true) ? "Notification" : "Indication";
  // str += " from ";
  // /** NimBLEAddress and NimBLEUUID have std::string operators */
  // str += std::string(pRemoteCharacteristic->getRemoteService()->getClient()->getPeerAddress());
  // str += ": Service = " + std::string(pRemoteCharacteristic->getRemoteService()->getUUID());
  // str += ", Characteristic = " + std::string(pRemoteCharacteristic->getUUID());
  // str += ", Value = " + std::string((char*)pData, length);
  // commSerial.println("notifyCB");
  // commSerial.println(str.c_str());
  //----------------Parse the BLE Packet------------------------
  bleCollectPacket((char*)pData, length);
  //----------------Parse the BLE Packet------------------------
}

/** Callback to process the results of the last scan or restart it */
void scanEndedCB(NimBLEScanResults results) {
  commSerial.println("Scan Ended");
}


/** Create a single global instance of the callback class to be used by all clients */
static ClientCallbacks clientCB;


/** Handles the provisioning of clients and connects / interfaces with the server */
bool connectToServer() {

  NimBLEClient* pClient = nullptr;

  /** Check if we have a client we should reuse first **/
  //if (NimBLEDevice::getClientListSize()) {  //<==========================================================================CHANGED=========================================
  if (NimBLEDevice::getCreatedClientCount()) {
    /** Special case when we already know this device, we send false as the
         *  second argument in connect() to prevent refreshing the service database.
         *  This saves considerable time and power.
         */
    pClient = NimBLEDevice::getClientByPeerAddress(advDevice->getAddress());
    if (pClient) {
      if (!pClient->connect(advDevice, false)) {
        commSerial.println("Reconnect failed");
        return false;
      }
      commSerial.println("Reconnected client");
    }
    /** We don't already have a client that knows this device,
         *  we will check for a client that is disconnected that we can use.
         */
    else {
      pClient = NimBLEDevice::getDisconnectedClient();
    }
  }

  /** No client to reuse? Create a new one. */
  if (!pClient) {
    //if (NimBLEDevice::getClientListSize() >= NIMBLE_MAX_CONNECTIONS) { //<==========================================================================CHANGED=========================================
    if (NimBLEDevice::getCreatedClientCount() >= NIMBLE_MAX_CONNECTIONS) {
      commSerial.println("Max clients reached - no more connections available");
      return false;
    }

    pClient = NimBLEDevice::createClient();

    commSerial.println("New client created");

    pClient->setClientCallbacks(&clientCB, false);
    /** Set initial connection parameters: These settings are 15ms interval, 0 latency, 120ms timout.
         *  These settings are safe for 3 clients to connect reliably, can go faster if you have less
         *  connections. Timeout should be a multiple of the interval, minimum is 100ms.
         *  Min interval: 12 * 1.25ms = 15, Max interval: 12 * 1.25ms = 15, 0 latency, 51 * 10ms = 510ms timeout
         */
    // pClient->setConnectionParams(12,12,0,51);
    pClient->setConnectionParams(20, 40, 0, 100);
    /** Set how long we are willing to wait for the connection to complete (seconds), default is 30. */
    pClient->setConnectTimeout(5000);


    if (!pClient->connect(advDevice)) {
      /** Created a client but failed to connect, don't need to keep it as it has no data */
      NimBLEDevice::deleteClient(pClient);
      commSerial.println("Failed to connect, deleted client");
      return false;
    }
  }

  if (!pClient->isConnected()) {
    if (!pClient->connect(advDevice)) {
      commSerial.println("Failed to connect");
      return false;
    }
  }

  commSerial.print("Connected to: ");
  commSerial.println(pClient->getPeerAddress().toString().c_str());
  commSerial.print("RSSI: ");
  commSerial.println(pClient->getRssi());

  /** Now we can read/write/subscribe the charateristics of the services we are interested in */
  NimBLERemoteService* pSvc = nullptr;
  NimBLERemoteCharacteristic* pChr = nullptr;
  NimBLERemoteDescriptor* pDsc = nullptr;

  pSvc = pClient->getService(serviceUUID);
  if (pSvc) { /** make sure it's not null */
    pChr = pSvc->getCharacteristic(charUUID_rx);

    if (pChr) { /** make sure it's not null */
      if (pChr->canRead()) {
        commSerial.print(pChr->getUUID().toString().c_str());
        commSerial.print(" Value: ");
        commSerial.println(pChr->readValue().c_str());
      }

      if (pChr->canWrite()) {
        uint8_t data[7] = { 0xdd, 0xa5, 0x4, 0x0, 0xff, 0xfc, 0x77 };
        if (pChr->writeValue(data, sizeof(data), true)) {
          commSerial.print("Wrote new value to: ");
          commSerial.println(pChr->getUUID().toString().c_str());
        } else {
          /** Disconnect if write failed */
          pClient->disconnect();
          return false;
        }

        if (pChr->canRead()) {
          commSerial.print("The value of: ");
          commSerial.print(pChr->getUUID().toString().c_str());
          commSerial.print(" is now: ");
          commSerial.println(pChr->readValue().c_str());
        }
      }

      /** registerForNotify() has been deprecated and replaced with subscribe() / unsubscribe().
             *  Subscribe parameter defaults are: notifications=true, notifyCallback=nullptr, response=false.
             *  Unsubscribe parameter defaults are: response=false.
             */
      if (pChr->canNotify()) {
        //if(!pChr->registerForNotify(notifyCB)) {
        if (!pChr->subscribe(true, notifyCB)) {
          /** Disconnect if subscribe failed */
          pClient->disconnect();
          return false;
        }
      } else if (pChr->canIndicate()) {
        /** Send false as first argument to subscribe to indications instead of notifications */
        //if(!pChr->registerForNotify(notifyCB, false)) {
        if (!pChr->subscribe(false, notifyCB)) {
          /** Disconnect if subscribe failed */
          pClient->disconnect();
          return false;
        }
      }
    }

  } else {
    commSerial.println("JBD BMS not found.");
  }



  commSerial.println("Done with this device!");
  return true;
}





void setup() {

  commSerial.begin(9600);
  //commSerial.begin(115200);
  //commSerial.begin(921600);
  commSerial.println("Starting NimBLE Client");

  WiFi.mode(WIFI_STA);
  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    commSerial.println("Error initializing ESP-NOW");
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
    commSerial.println("Failed to add peer");
    return;
  }

  /** Initialize NimBLE, no device name spcified as we are not advertising */
  NimBLEDevice::init("");

  /** Optional: set the transmit power, default is 3db */
  // #ifdef ESP_PLATFORM
  //     NimBLEDevice::setPower(9); /** +9db */
  // #endif

  /** create new scan */
  NimBLEScan* pScan = NimBLEDevice::getScan();

  /** Set the callbacks to call when scan events occur, no duplicates */
  pScan->setScanCallbacks(&scanCallbacks, false);

  /** Set scan interval (how often) and window (how long) in milliseconds */
  pScan->setInterval(45);
  pScan->setWindow(15);

  // Populate MAC filter
  //int size = *(&bmsBLEMacAddressesFilter + 1) - bmsBLEMacAddressesFilter;
  size_t size = sizeof(bmsBLEMacAddressesFilter) / sizeof(bmsBLEMacAddressesFilter[0]);
  for (auto i = 0; i < size; ++i) {
    NimBLEDevice::whiteListAdd(NimBLEAddress(bmsBLEMacAddressesFilter[i],0));
    commSerial.println(bmsBLEMacAddressesFilter[i].c_str());
    delay(150);
  }

  // Set setFilterPolicy to ONLY connect to BMS MAC addresses specified above
  if (sizeof(bmsBLEMacAddressesFilter) == 0) {
    pScan->setFilterPolicy(BLE_HCI_SCAN_FILT_NO_WL);
  } else {
    pScan->setFilterPolicy(BLE_HCI_SCAN_FILT_USE_WL);
  }

  /** Active scan will gather scan response data from advertisers
     *  but will use more energy from both devices
     */
  pScan->setActiveScan(true);
  /** Start scanning for advertisers for the scan time specified (in seconds) 0 = forever
     *  Optional callback for when scanning stops.
     */
  //pScan->start(scanTime, scanEndedCB); //<==========================================================================CHANGED=========================================
  pScan->start(scanTimeMs);

}




void loop() {
  /** Loop here until we find a device we want to connect to */
  while (!doConnect) {

    int totalCellCount = 1;


    NimBLERemoteService* pSvc = nullptr;
    NimBLERemoteCharacteristic* pChr = nullptr;
    NimBLERemoteDescriptor* pDsc = nullptr;
    NimBLEClient* pClient = nullptr;

    /** Check if we have a client we should reuse first **/
    //if (NimBLEDevice::getClientListSize()) { //<==========================================================================CHANGED=========================================
    if (NimBLEDevice::getCreatedClientCount()) {
      /** Special case when we already know this device, we send false as the
          *  second argument in connect() to prevent refreshing the service database.
          *  This saves considerable time and power.
          */
      // for (auto i = 0; i < NimBLEDevice::getClientListSize(); ++i) {
      for (auto i = 0; i < NimBLEDevice::getWhiteListCount(); ++i) {  //WORKS
        pClient = NimBLEDevice::getClientByPeerAddress(NimBLEDevice::getWhiteListAddress(i));
        if (!pClient || !pClient->isConnected()) {
          // commSerial.println("pClient is NULL or not connected");
        } else {
          // commSerial.print("pClient is NOT NULL: ");
          // commSerial.println(pClient->getPeerAddress().toString().c_str());
          // commSerial.print("isConnected?: ");
          // commSerial.println(pClient->isConnected());

          pSvc = pClient->getService(serviceUUID);
          if (pSvc) { /** make sure it's not null */
            pChr = pSvc->getCharacteristic(charUUID_tx);
            // if(pChr->canWrite()) {
            if (pChr->canWriteNoResponse()) {
              uint8_t data[7] = { 0xdd, 0xa5, 0x4, 0x0, 0xff, 0xfc, 0x77 };
              if (pChr->writeValue(data, sizeof(data), true)) {
                // commSerial.print("Wrote new value to: ");
                // commSerial.println(pChr->getUUID().toString().c_str());
              } else {
                commSerial.println("pChr->writeValue FAILED");
              }
            } else {
              commSerial.println("pChr->canWriteNoResponse FALSE");
            }

          } else {
            commSerial.println("pSvc null");
          }




          delay(150);
          if (newPacketReceived == true) {

            for (byte i = 1; i <= packCellInfo.NumOfCells; i++) {
              //int x = find(pClient->getPeerAddress().toString().c_str());
              //commSerial.print(x);
              //bmsCellCount[x] = packCellInfo.NumOfCells;

              //int totalCellCount = 0;

              // for (int element : bmsCellCount) {  // for each element in the array
              //   totalCellCount += bmsCellCount[element];
              // }
              

              //for (int i = 0; i < sizeof(bmsCellCount) / sizeof(bmsCellCount[0]); i++) {
              //  totalCellCount = totalCellCount + bmsCellCount[i];
              //}

              //commSerial.print(totalCellCount);
              

              //commSerial.print(pClient->getPeerAddress().toString().c_str()); //works
              //commSerial.printf("   %.3f\n", (float)packCellInfo.CellVolt[i - 1] / 1000); //works
              //commSerial.printf("   %i:%.3f\n", i, (float)packCellInfo.CellVolt[i - 1] / 1000);  //works
              //commSerial.printf("   %x %i:%.3f\n", x, i, (float)packCellInfo.CellVolt[i - 1] / 1000);  //works
              commSerial.printf("   %i:%.3f\n", totalCellCount, (float)packCellInfo.CellVolt[i - 1] / 1000);  //works




              //char data[6];
              char data[9];
              // snprintf(data, sizeof(data), "Hello, World! #%lu", msg_count++);
              //snprintf(data, sizeof(data), "%i:%.3f", i - 1, (float)packCellInfo.CellVolt[i - 1] / 1000);  //works
              snprintf(data, sizeof(data), "%i:%.3f", totalCellCount - 1, (float)packCellInfo.CellVolt[i - 1] / 1000);
              // if (!broadcast_peer.send_message((uint8_t *)data, sizeof(data))) {
              // Serial.println("Failed to broadcast message");
              // }

              totalCellCount++;

              esp_err_t result = esp_now_send(expressAddress, (uint8_t*)data, sizeof(data));
              //delay(100); //works
              delay(50);
            }
          }
        }
      }
    }




    delay(500);
    


  } //while(!doConnect){}


  doConnect = false;
  /** Found a device we want to connect to, do it now */
  if (connectToServer()) {
    commSerial.println("Success! we should now be getting notifications, scanning for more!");
  } else {
    commSerial.println("Failed to connect, starting scan");
  }

  NimBLEDevice::getScan()->start(scanTimeMs, false, true);


}//Loop
