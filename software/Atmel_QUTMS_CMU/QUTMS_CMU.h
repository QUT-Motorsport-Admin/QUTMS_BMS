/*
 * Atmel_QUTMS_CMU.h
 *
 * Created: 3/07/2016 10:44:46 AM
 *  Author: julius
 */ 


#ifndef QUTMS_CMU_H_
#define QUTMS_CMU_H_

#define F_CPU 		16000000UL
#define DEVICE_ID 	0x03
#include "AtmelCAN.h"
#include <util/delay.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <stdlib.h>
#include <math.h>

#define RAIL_V 5000
#define AVG_V_SIZE 5
//#define BALANCE_DUTY_CYCLE 90
#define MV_OFFSET 62
#define TEMP_MULTIPLEX_PORT PORTD	//if ever the PCB design changes and the adc multiplexer changes pins, these two need to be changed
#define TEMP_MULTIPLEX_CH 2

#define WAKE_IN_PIN		PINC&(1<<PINC0)
#define VOLT1_ID 		0b00001
#define VOLT2_ID 		0b00010
#define TEMP1_ID 		0b00100
#define TEMP2_ID 		0b00101
#define STATA_ID		0b01001
#define AUDIT_REQUEST 	0b10001
#define AUDIT_RESPONSE	0b00000000000000000000000010011|(deviceID<<8)
#define AUDIT_REQmsk	0b11000000000000000000000000000
#define ADMIN		 	0b10000000000000000000000000000
#define ADMINmsk		0b10000000000000000000000000000
#define AMU				0b01000000000000000000000000000
#define AMUmsk			0b01000000000000000000000000000
#define BALANCE_NPACKETS	5



#define READ_RECEIPT 	0b10010
#define BALANCE_ON		0b10100
#define BALANCE_OFF		0b10101


#define EEPROM_DEVICE_ID					0x000
#define EEPROM_ADC_SAMPLES					0x002

#define EEPROM_FW_VERSION					0x004
#define EEPROM_CELL_PARAMETERS				0x040
#define EEPROM_CELL_V_DIFF_MAX_THRESHOLD	0x040
#define EEPROM_CELL_V_DIFF_MIN_THRESHOLD	0x042
#define EEPROM_CELL_V_ERR_MAX				0x044
#define EEPROM_CELL_V_ERR_MIN				0x046
#define EEPROM_DISCHARGE_SCALE				0x048

#define CELL0			1
#define CELL1			2
#define CELL2			4
#define CELL3			8
#define CELL4			16
#define CELL5			32
#define CELL6			64
#define CELL7			128

#define _cellsToBalance GPIOR0

const int TEMP_Celsius_pos[141] =  // Positive Celsius temperatures (ADC-value)
{                           			// from 0 to 140 degrees
	938, 935, 932, 929, 926, 923, 920, 916, 913, 909, 906, 902, 898,
	894, 891, 887, 882, 878, 874, 870, 865, 861, 856, 851, 847, 842,
	837, 832, 827, 822, 816, 811, 806, 800, 795, 789, 783, 778, 772,
	766, 760, 754, 748, 742, 735, 729, 723, 716, 710, 703, 697, 690,
	684, 677, 670, 663, 657, 650, 643, 636, 629, 622, 616, 609, 602,
	595, 588, 581, 574, 567, 560, 553, 546, 539, 533, 526, 519, 512,
	505, 498, 492, 485, 478, 472, 465, 459, 452, 446, 439, 433, 426,
	420, 414, 408, 402, 396, 390, 384, 378, 372, 366, 360, 355, 349,
	344, 338, 333, 327, 322, 317, 312, 307, 302, 297, 292, 287, 282,
	277, 273, 268, 264, 259, 255, 251, 246, 242, 238, 234, 230, 226,
	222, 219, 215, 211, 207, 204, 200, 197, 194, 190, 1
};


typedef struct cell
{
	uint8_t temp_channel, voltage_channel, discharge_pin, cell_num;
	uint16_t temperature, voltage;
	uint16_t voltages[AVG_V_SIZE];
}cell;

//live changeable parameters
uint16_t ADC_SAMPLES = 10;					//3 samples will be taken of the ADC 10 bit values, before the variable is stored.
uint8_t DISCHARGE_DURATION = 5;				//in seconds, how long the cell will dissipate power for, per discharge. DISCHARGE_MODE specifies how many discharges are to take place. HEAVY MODE AT 5 seconds would be 25 seconds per CMU to balance cells
uint16_t CELL_V_DIFF_MAX_THRESHOLD = 500;	//differences greater than 500mV will stop the balancing process.
uint16_t CELL_V_DIFF_MIN_THRESHOLD = 10;	//ignore differences between voltages of +- 10mV
uint16_t CELL_V_ERR_MIN = 2900;
uint16_t CELL_V_ERR_MAX = 4500;
uint16_t DISCHARGE_SCALE = 200;				//scale of discharge. works in us per mV of difference
uint8_t registered = 0;
uint8_t EEPROMWriteComplete = 0;
uint8_t CellBalanceMode = 0;
uint16_t deviceID = 0x05;
uint16_t fw_version = 0;
uint8_t AMUID = 0xFF;
//volatile uint8_t _cellsToBalance = 0;
volatile uint8_t CellBalanceCounter = 0;
volatile uint8_t CellNum[8] = {1, 2, 4, 8, 16, 32, 64, 128};
volatile uint8_t BALANCE_DUTY_CYCLE = 20;
uint16_t BALANCE_TARGET_VOLTAGE = 3700;
uint8_t BALANCE_RESERVED1 = 0x00;
uint8_t BALANCE_RESERVED2 = 0x00;

//void *NULL;
void Discharge_cells(cell high_1, cell high_2, cell high_3, cell lowest);
void LED_flash(uint8_t duration);

uint16_t CMU_eeprom_read(uint16_t address);
void CMU_eeprom_write(uint16_t address, uint16_t value);



#endif /* QUTMS_CMU_H_ */