#ifndef UTILS_H_
#define UTILS_H_

#include "stdint.h"

uint16_t calculate_average(uint16_t *voltages, uint8_t count) {
    uint32_t sum = 0;
    for (int i = 0; i < count; i++) {
        sum += voltages[i];
    }
    return (uint16_t)(sum / count);
}

uint16_t calculate_median(uint16_t *voltages, uint8_t count) {
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (voltages[i] > voltages[j]) {
                uint16_t temp = voltages[i];
                voltages[i] = voltages[j];
                voltages[j] = temp;
            }
        }
    }
    if (count % 2 == 0) {
        return (voltages[count / 2 - 1] + voltages[count / 2]) / 2;
    } else {
        return voltages[count / 2];
    }
}

#endif //UTILS_H_