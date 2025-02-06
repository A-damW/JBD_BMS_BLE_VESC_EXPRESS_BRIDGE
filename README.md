# JBD_BMS_BLE_VESC_EXPRESS_BRIDGE
An ESP32 firmware and companion LispBM script to connect multiple simultaneous JBD ble BMSes to VESC-EXPRESS via ESPNOW

## TODO
- [ ] Write a better readme. (Ongoing...)
- :white_check_mark: Softcode the battery cell number, i.e. tally the total cell count on the esp32 bridge, and send over esp-now to lisp on vesc-express.
- :white_check_mark: Add support for 6.02 and 6.05. See https://github.com/A-damW/JBD_BMS_BLE_VESC_EXPRESS_BRIDGE/issues/6
- [ ] Add support for cell/bms temperatures.
- [ ] Add support for charge/discarge control.

## Features

- Connect to one or more JBD bms simultaneously and transmit cell data to vesc-express.
- For example, you could have three 14s bms connected to a single 42s battery pack, and see all 42 cells in vesc-tool.

## Requied hardware

* Single or multiple "smart" JBD bms, with built in BT, or BT dongle.
	* Tested: model SP14S004, SP17S005, working.

* An ESP32 with BluetoothLE, this is the ESP-BRIDGE that connects to the single or multiple JBD bms over BT, and relays to VESC-EXPRESS, via esp-now.
	* Tested: ESP32-C3, ESP32-S3, working.

* A VESC-EXPRESS, this is an ESP32-C3 with a can-bus tranceiver, running the vesc-express firmware.

