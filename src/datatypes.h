#ifndef DATATYPES_H_
#define DATATYPES_H_

#include <stdint.h>
#include <stdbool.h>

#define BMS_MAX_CELLS	50
#define BMS_MAX_TEMPS	50

typedef struct {
	float v_tot;
	float v_charge;
	float i_in;
	float i_in_ic;
	float ah_cnt;
	float wh_cnt;
	int cell_num;
	float v_cell[BMS_MAX_CELLS];
	bool bal_state[BMS_MAX_CELLS];
	int temp_adc_num;
	float temps_adc[BMS_MAX_TEMPS];
	float temp_ic;
	float temp_hum;
	float hum;
	float pressure;
	float temp_max_cell;
	float soc;
	float soh;
	int can_id;
	float ah_cnt_chg_total;
	float wh_cnt_chg_total;
	float ah_cnt_dis_total;
	float wh_cnt_dis_total;
	int is_charging;
	int is_balancing;
	int is_charge_allowed;
	uint32_t update_time;
} bms_values;

typedef struct {
    float voltage;
    float current;
    float ah_now;
    float ah_total;

    int temp_num;
    float temp1;
    float temp2;

    int cell_num;
    float cell_voltage[20];
    int cell_max;
    int cell_min;           
    int cell_diff;           
    int cell_avg;   
    int cell_median;  

    int cycle_number;
    int production_date;

    int balance_state;  
    int balance_state_high;

    int protection_status;  
    int output_status;

    int soc;
} bms_values_jbd;













#endif //DATATYPES_H_