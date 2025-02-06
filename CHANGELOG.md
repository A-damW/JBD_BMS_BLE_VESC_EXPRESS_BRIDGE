#### 2025-02-06
* Lisp script:
	* Added 2 files, 6.02.lisp and 6.05.lisp.
  * Removed the 6.02 only lisp script. See https://github.com/A-damW/JBD_BMS_BLE_VESC_EXPRESS_BRIDGE/issues/6

* Added this CHANGELOG.md.
* Updated README.md.


#### 2024-12-13
* Softcode the battery cell number, i.e. tally the total cell count on the esp32 bridge, and send over esp-now to lisp on vesc-express.
	* So now the esp32-bridge tracks the cell count, and sends via esp-now to the vesc-express in a colon delimited cellnum:voltage format:
	* 1:3.604
	* 2:3.685
	* 3:3.691
	* 4:3.690
	* 5:3.668
	* etc.,
	* where it is split and applied to the appropriate values in the vesc-express lisp script.
