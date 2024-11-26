
/** JBD_BMS_BLE_VESC_EXPRESS_BRIDGE:
 *
 *  An ESP32 firmware and companion LispBM script
*   to connect multiple JBD BLE BMSs to VESC-EXPRESS via ESPNOW
 *
 *  Created: on September 4 2024
 *      Author: A-damW, https://github.com/A-damW
 *
*/

//----------------------------------------
#include <NimBLEDevice.h>

static NimBLEAdvertisedDevice* advDevice;

static bool doConnect = false;
static uint32_t scanTime = 15; /** 0 = scan forever */

#include <esp_now.h>
#include <WiFi.h>

#define TRACE
#include "mydatatypes.h"

#define commSerial Serial

packBasicInfoStruct packBasicInfo;  //structures for BMS data
packEepromStruct packEeprom;        //structures for BMS data
packCellInfoStruct packCellInfo;    //structures for BMS data

const byte cBasicInfo3 = 3;         //type of packet 3= basic info
const byte cCellInfo4 = 4;          //type of packet 4= individual cell info

static BLEUUID serviceUUID("0000ff00-0000-1000-8000-00805f9b34fb");  //xiaoxiang bms original module
static BLEUUID charUUID_rx("0000ff01-0000-1000-8000-00805f9b34fb");  //xiaoxiang bms original module
static BLEUUID charUUID_tx("0000ff02-0000-1000-8000-00805f9b34fb");  //xiaoxiang bms original module

// Notifications from this characteristic is received data from BMS
// 0000ff01-0000-1000-8000-00805f9b34fb
// NOTIFY, READ

// Write to this characteristic (charUUID_tx) to send data to BMS.
// The data will be returned on the charUUID_rx characteristic.
// 0000ff02-0000-1000-8000-00805f9b34fb
// READ, WRITE, WRITE NO RESPONSE


bool newPacketReceived = false;

// List of BMS BLE addresses, this list is exclusive.
// Uncomment the empty list: bmsBLEMacAddressesFilter[0]={} to allow all MACs(Max 3 connections default)-
// with serviceUUID("0000ff00-0000-1000-8000-00805f9b34fb")
// Remember to set the count [3] to match the number of MACs.
std::string bmsBLEMacAddressesFilter[2]={"A5:C2:37:06:C7:36","A5:C2:37:18:C9:E1"};
//std::string bmsBLEMacAddressesFilter[3]={"A5:C2:37:06:C7:36","A5:C2:37:18:C9:E1","A5:C2:37:06:C7:36"};
// std::string bmsBLEMacAddressesFilter[1]={"A5:C2:37:06:C7:36"};
// std::string bmsBLEMacAddressesFilter[0]={};


// REPLACE WITH YOUR VESC Express MAC Address, found by running the companion Lisp script.
uint8_t expressAddress[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }; // <This is the ESP-NOW broadcast address

esp_now_peer_info_t peerInfo;

//----------------------------------------

// callback when data is sent
void OnDataSent(const uint8_t* mac_addr, esp_now_send_status_t status) {
  // Serial.print("\r\nLast Packet Send Status:\t");
  // Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}


class ClientCallbacks : public NimBLEClientCallbacks {
  void onConnect(NimBLEClient* pClient) {
    commSerial.println("Connected");
    /** After connection we should change the parameters if we don't need fast response times.
         *  These settings are 150ms interval, 0 latency, 450ms timout.
         *  Timeout should be a multiple of the interval, minimum is 100ms.
         *  I find a multiple of 3-5 * the interval works best for quick response/reconnect.
         *  Min interval: 120 * 1.25ms = 150, Max interval: 120 * 1.25ms = 150, 0 latency, 60 * 10ms = 600ms timeout
         */
    // pClient->updateConnParams(120,120,0,60);

  };

  void onDisconnect(NimBLEClient* pClient) {
    commSerial.print(pClient->getPeerAddress().toString().c_str());
    commSerial.println(" Disconnected - Starting scan");
    NimBLEDevice::getScan()->start(scanTime, scanEndedCB);
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
};


/** Define a class to handle the callbacks when advertisments are received */
class AdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks {

  void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
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

      Serial.printf("Adding %s to whitelist\n", std::string(advertisedDevice->getAddress()).c_str());
      NimBLEDevice::whiteListAdd(advertisedDevice->getAddress());
    }
  };
};


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
  //----------------My stuff------------------------
  bleCollectPacket((char*)pData, length);
  //----------------My stuff------------------------
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
  if (NimBLEDevice::getClientListSize()) {
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
    if (NimBLEDevice::getClientListSize() >= NIMBLE_MAX_CONNECTIONS) {
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
    pClient->setConnectTimeout(5);


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
  commSerial.println("Starting NimBLE Client");

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


  /** Initialize NimBLE, no device name spcified as we are not advertising */
  NimBLEDevice::init("");

  /** Optional: set the transmit power, default is 3db */
  // #ifdef ESP_PLATFORM
  //     NimBLEDevice::setPower(ESP_PWR_LVL_P9); /** +9db */
  // #else
  //     NimBLEDevice::setPower(9); /** +9db */
  // #endif

  /** create new scan */
  NimBLEScan* pScan = NimBLEDevice::getScan();

  /** create a callback that gets called when advertisers are found */
  pScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());

  /** Set scan interval (how often) and window (how long) in milliseconds */
  pScan->setInterval(45);
  pScan->setWindow(15);

// Populate MAC filter
  for (byte i = 0; i <sizeof(bmsBLEMacAddressesFilter); ++i) {  
    NimBLEDevice::whiteListAdd(bmsBLEMacAddressesFilter[i]);
  }

    /** Set setFilterPolicy to ONLY connect to BMS MAC addresses specified above
     *  but will use more energy from both devices
     */
  if (sizeof(bmsBLEMacAddressesFilter) == 0) {
    pScan->setFilterPolicy(BLE_HCI_SCAN_FILT_NO_WL);
  }
  else{
    pScan->setFilterPolicy(BLE_HCI_SCAN_FILT_USE_WL);
  }
  /** Active scan will gather scan response data from advertisers
     *  but will use more energy from both devices
     */
  pScan->setActiveScan(true);
  /** Start scanning for advertisers for the scan time specified (in seconds) 0 = forever
     *  Optional callback for when scanning stops.
     */
  pScan->start(scanTime, scanEndedCB);
  // NimBLEDevice::whiteListAdd(advertisedDevice->getAddress());
}


void loop() {
  /** Loop here until we find a device we want to connect to */
  while (!doConnect) {


    NimBLERemoteService* pSvc = nullptr;
    NimBLERemoteCharacteristic* pChr = nullptr;
    NimBLERemoteDescriptor* pDsc = nullptr;
    NimBLEClient* pClient = nullptr;

    /** Check if we have a client we should reuse first **/
    if (NimBLEDevice::getClientListSize()) {
      /** Special case when we already know this device, we send false as the
          *  second argument in connect() to prevent refreshing the service database.
          *  This saves considerable time and power.
          */
      // for (auto i = 0; i < NimBLEDevice::getClientListSize(); ++i) {
      for (auto i = 0; i < NimBLEDevice::getWhiteListCount(); ++i) {  
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
              commSerial.println("pChr->canWrite FALSE");
            }

          } else {
            commSerial.println("pSvc null");
          }







          delay(100);
          if (newPacketReceived == true) {
            for (byte i = 1; i <= packCellInfo.NumOfCells; i++) {
              commSerial.print(pClient->getPeerAddress().toString().c_str());
              commSerial.printf("   %.3f\n", (float)packCellInfo.CellVolt[i - 1] / 1000);


              // This one sends just the the cell voltage: 3.700
              //char data[6];
              //snprintf(data, sizeof(data), "%.3f", (float)packCellInfo.CellVolt[i - 1] / 1000);

              
              // This one sends the the cell number and cell voltage: 1:3.700, 2:3.703 2:3.699, etc.
              char data[9];
              snprintf(data, sizeof(data), "%i:%.3f", i-1, (float)packCellInfo.CellVolt[i - 1] / 1000);



              esp_err_t result = esp_now_send(expressAddress, (uint8_t*)data, sizeof(data));
              delay(100);
            }
          }
        }
      }
    }









    delay(1000);


  }  //while(!doConnect){

  doConnect = false;

  /** Found a device we want to connect to, do it now */
  if (connectToServer()) {
    commSerial.println("Success! we should now be getting notifications, scanning for more!");
  } else {
    commSerial.println("Failed to connect, starting scan");
  }

  NimBLEDevice::getScan()->start(scanTime, scanEndedCB);
}
