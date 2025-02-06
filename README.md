# JBD_BMS_BLE_VESC_EXPRESS_BRIDGE
An ESP32 firmware and companion LispBM script to connect multiple simultaneous JBD ble BMSes to VESC-EXPRESS via ESPNOW

## TODO
- [ ] Write a better readme. (Ongoing...)
- :white_check_mark: Softcode the battery cell number, i.e. tally the total cell count on the esp32 bridge, and send over esp-now to lisp on vesc-express.
- :white_check_mark: Add support for 6.02 and 6.05

## Features

The xESC2 firmware is based off the current version of the VESC firmware. Therefore it is able to do everything you'd wish your brushless ESC had. Including: FOC, speed control, current limiting, ...


## Requied hardware

* Single or multiple "smart" JBD bms, with built in BT, or BT dongle.
	* Tested: model SP14S004, SP17S005, working.

* An ESP32 with BluetoothLE, this is the ESP-BRIDGE that connects to the single or multiple JBD bms over BT, and relays to VESC-EXPRESS, via esp-now.
	* Tested: ESP32-C3, ESP32-S3, working.

* A VESC-EXPRESS, this is an ESP32-C3 with a can-bus tranceiver, running the vesc-express firmware.

