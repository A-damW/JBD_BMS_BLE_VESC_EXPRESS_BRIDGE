
;JBD_BMS_BLE_VESC_BRIDGE:
;An ESP32 firmware and companion LispBM script
;to connect multiple JBD BLE BMSs to VESC-EXPRESS via ESPNOW
;Created: on September 4 2024
;Author: A-damW, https://github.com/A-damW

; ----------------- Begin user config -----------------
; Set the total count of cells in your pack
(def cell-total-count 39)
; ----------------- End user config -----------------

(print (get-mac-addr))
(print (wifi-get-chan))
(def cell-index 0)
(def current-cell 0)

(esp-now-start)

(set-bms-val 'bms-cell-num cell-total-count)
;(print "HI")

; Here src is the mac-address of the sender, des is the mac-address
; of the receiver and data is an array with the sent data. If des is
; the broadcast address (255 255 255 255 255 255) it means that this
; was a broadcast packet.
(defun proc-data (src des data)
;        (print (list data))
    (progn

         (def cell-index (ix (str-split data ":") 0))
         (def current-cell (ix (str-split data ":") 1))
         (print cell-index current-cell)
         (set-bms-val 'bms-v-cell (str-to-i cell-index) (str-to-f current-cell))
         ;(send-bms-can) ;Un-comment to send bms data over can-bus

    );progn    
);proc-data

(defun event-handler ()
    (loopwhile t
        (progn
            (recv
                ((event-esp-now-rx (? src) (? des) (? data)) (proc-data src des data))
                );recv
                
        );progn
    );loopwhile t
);defun event-handler ()

; Spawn the event handler thread and pass the ID it returns to C
(event-register-handler (spawn event-handler))

; Enable the custom app data event
(event-enable 'event-esp-now-rx)
