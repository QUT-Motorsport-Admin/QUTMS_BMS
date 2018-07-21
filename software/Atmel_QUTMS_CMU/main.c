/*
 * Atmel_QUTMS_CMU.c
 *
 * Created: 3/07/2016 10:41:03 AM
 *  Author: julius
 */ 

#include "main.h"

void IO_init()
{

	DDRD = 0b10011111;		//7 for discharge cell 4; 2,3,4 for multiplex selector; 0 for wake out, 1 for discharge cells 1 and 2; 6 is ADC input; 5 is ADC input.
	DDRC = 0b10000110;		//7 and 1 for cell balancing; 6,5 and 4 for ADC in; 3 for CAN Rx; 2 for CAN Tx; 0 for wake in (doesn't matter).
	DDRB = 0b00011011;		//7, 6, 5 and 2 for ADC in; 4, 3, 1 and 0 for cell balance.
	
	EICRA = (1<<ISC31)|(1<<ISC30);	//0b11000000	//to enable rising edge of INT3 on PINC0 to cause an external interrupt
	EIMSK = (1<<INT3);				//this may not be legal, the data sheet said it was read only, otherwise it might be 1<<INT1 and 1<<INT0 to represent 3 (0b00000011)
	//EIFR bits INTF3..0 hold information on whether the interrupt has occurred.
	//PCICR is useful when using the level change is being used to indicate the interrupt. hi-low or low-hi maybe
	sei();
}

void Parameters_init()
{
	deviceID  = CMU_eeprom_read(EEPROM_DEVICE_ID);
	ADC_SAMPLES  = CMU_eeprom_read(EEPROM_ADC_SAMPLES);
	fw_version = CMU_eeprom_read(EEPROM_FW_VERSION);
	CELL_V_DIFF_MAX_THRESHOLD = CMU_eeprom_read(EEPROM_CELL_V_DIFF_MAX_THRESHOLD);
	CELL_V_DIFF_MIN_THRESHOLD = CMU_eeprom_read(EEPROM_CELL_V_DIFF_MIN_THRESHOLD);
	DISCHARGE_SCALE = CMU_eeprom_read(EEPROM_DISCHARGE_SCALE);
	EEPROMWriteComplete = 0;
}

void ADC_init()
{
	ADMUX=(1<<REFS0)|(1<<AREFEN);                      // For Aref=AVcc with external capacitor;
	ADMUX &= ~(1<<ADLAR);								//make sure adlar is not set.
	ADCSRA=(1<<ADEN)|(1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0); //Prescaler div factor = 128, 125kHz --> lowest we can go for best accuracy.
}

void Cells_init(cell * _cells)
{
	uint8_t cell_temp_config[] = {7,6,3,2,5,4,1,0};		//multiplexer number to use for each different cell.
	uint8_t cell_discharge_config[8] = {PIND1,PINC1,PINB0,PINB1,PIND7,PINB3,PINB4,PIND0};
	//uint8_t cell_vLevel_config[8] = {10,9,8,7,6,5,4,2};	//ADC Channels to use	
	uint8_t cell_vLevel_config[8] = {4,7,3,5,8,9,6,10};	//ADC Channels to use
		
	
	for(uint8_t cParse = 0; cParse <=7; cParse++)
	{
		_cells[cParse].cell_num = cParse;								//assign our cell objects a local identifier number
		_cells[cParse].temp_channel	= cell_temp_config[cParse];			//assign cell object the multiplexer value it needs to set before reading ADC value
		_cells[cParse].voltage_channel = cell_vLevel_config[cParse];		//assign cell ADC channel to use when reading
		_cells[cParse].discharge_pin = cell_discharge_config[cParse];	//assign which pin to use to discharge
	}
}

uint16_t CMU_eeprom_read(uint16_t address)
{
	while(!eeprom_is_ready());
	return eeprom_read_word((const uint16_t *)address);
}

void CMU_eeprom_write(uint16_t address, uint16_t value)
{
	while(!eeprom_is_ready());
	eeprom_write_word((uint16_t *)address, value);
}

uint16_t ADC_read(uint8_t channel)
{
	channel = (ADMUX & 0xe0)|(channel & 0x1F); //ADMUX | 0b11100000 and channel | 0b00011111 --> this keeps all bits of ADMUX the same except for the bits signalling which channel to use.
	ADMUX = channel;
	ADCSRA |= (1<<ADSC);							//ADSC (single conversion bit) is set to 1 to start the conversion process.
	while(!(ADCSRA & (1<<ADIF)));				//run a loop while the conversion is taking place.
	uint16_t result = 0;
	result = ADCL;								//read ADCL first, ADCH after --> order is important! --> also not sure if this code is correct. other ADC examples return 'ADC' instead.
	result |= ((3 & ADCH) << 8);
	ADCSRA|=(1<<ADIF);							//once read and done, clear the 'complete' status by writing 1 to the ADIF bit.
	return result;								//pass the 10 bit ADC number to requesting function.
}

void LED_flash(uint8_t duration)
{
	duration = duration/2;
	DDRC |= 1<<PINC0;
	PORTC |= 1<<PINC0;
	for(uint8_t i = 0; i < duration; i++)
	{
		_delay_us(995);
	}
	PORTC &= ~(1<<PINC0);
	DDRC &= ~(1<<PINC0);
	for(uint8_t i = 0; i < duration; i++)
	{
		_delay_us(995);
	}
}
void LED_on()
{
	DDRC |= 1<<PINC0;
	PORTC |= 1<<PINC0;
}
void LED_off()
{
	PORTC &= ~(1<<PINC0);
	DDRC &= ~(1<<PINC0);
}

uint16_t Convert_ADCtoMilliVolts(uint16_t ADCValue)
{
    //return as invalid;
	if(ADCValue >= 1023) return  65535;
    //finish the conversion with error if the value will be completely unreasonable.
	if(ADCValue <  20 ) return 65535;
    //xxxxx.xxxxf results in the calculated value being a float, instead of an integer
	return (uint16_t)(((double)ADCValue*0.000977517f)*RAIL_V-MV_OFFSET);

}

uint16_t Read_voltage(cell * _cell)
{
	uint32_t sum = 0;
	for(uint8_t c = 0; c < AVG_V_SIZE; c++)
	{
		//elementToUpdate = (rand() % AVG_V_SIZE);						//choose a random element to update
		sum += Convert_ADCtoMilliVolts(ADC_read(_cell->voltage_channel));			//read the voltage
		//if(elementToUpdate < AVG_V_SIZE)_cell->voltages[elementToUpdate] = tempVoltage;	//if the random number is valid, set the chosen element to the new voltage.
	}
	return (uint16_t)(sum/AVG_V_SIZE);	//return the average.
}

void Read_all_voltages(cell * _cells)
{

	cell * t_cells = _cells;
	for(uint8_t cellCount = 0; cellCount <= 7; cellCount++)
	{
		t_cells = _cells+cellCount;
		t_cells->voltage = Read_voltage(t_cells);	//move through all cells and assign voltage levels.
	}

}

uint8_t TX_cellVoltage(cell *_cells)
{
	int8_t mob;
	uint8_t tempData[8];
	
	for(uint8_t cellCount = 0; cellCount <= 3; cellCount++)
	{
		tempData[cellCount*2]	= _cells[cellCount].voltage >> 8;
		tempData[cellCount*2+1] = _cells[cellCount].voltage;
	}
	mob = CAN_findFreeTXMOB();
	if(mob>=0)CAN_TXMOB(mob, 8, tempData, ((uint32_t)deviceID<<18)|((uint32_t)AMUID<<8)|VOLT1_ID, 0); //transmit first 4 cell data and do not wait for finish
	else return 0;
	_delay_ms(5);
	
	for(uint8_t cellCount = 4; cellCount <= 7; cellCount++)
	{
		tempData[(cellCount-4)*2]	= _cells[cellCount].voltage >> 8;
		tempData[(cellCount-4)*2+1] = _cells[cellCount].voltage;
	}
	mob = CAN_findFreeTXMOB();
	if(mob>=0)CAN_TXMOB(mob, 8, tempData, ((uint32_t)deviceID<<18)|((uint32_t)AMUID<<8)|VOLT2_ID, 0); //transmit first 4 cell data and do not wait for finish
	else return 0;
	_delay_ms(5);
	
	return 1;
}


uint16_t Convert_ADCtoCelsius(uint16_t ADCValue)		//will spit out a value from 0 to 140  (ADC 10 bit value 1023 will be 255, lower than 187 will be 255)
{
	if(ADCValue >= 1023)return 255;				//this indicates the sensor has failed, perhaps a short somewhere?
	if(ADCValue < 187 )return 255;				//finish the conversion with error if the value will be invalid.
	uint8_t i = 0;
	while(TEMP_Celsius_pos[i] > ADCValue) i++;	//move through the array until values position is found. C will be equal to elements of the array we went through.
	return i;
}

uint16_t Read_temp(cell * _cell)
{


	uint32_t sum = 0;
	PORTD = (PORTD & 0xe3)|((_cell->temp_channel<<2) & 0x1c);		//so as to not change the rest of PORTD, we 'AND' the values of PORTD and uiMX with a mask
	for(uint8_t c = 0; c < ADC_SAMPLES; c++)
	{
		//elementToUpdate = (rand() % AVG_V_SIZE);						//choose a random element to update
		sum += Convert_ADCtoCelsius(ADC_read(TEMP_MULTIPLEX_CH));			//read the voltage
		//if(elementToUpdate < AVG_V_SIZE)_cell->voltages[elementToUpdate] = tempVoltage;	//if the random number is valid, set the chosen element to the new voltage.
	}
	return (uint16_t)(sum/AVG_V_SIZE);	//return the average.
	
	/*
	uint16_t tempTemp[ADC_SAMPLES-1];
	uint8_t count = 0;
	uint16_t sum = 0, avgTemp = 0;
	PORTD = (PORTD & 0xe3)|((_cell.temp_channel & 0x1c)<<2);		//so as to not change the rest of PORTD, we 'AND' the values of PORTD and uiMX with a mask
	for(uint8_t i = 0; i < ADC_SAMPLES; i++)
	{
		tempTemp[i] = Convert_ADCtoCelsius(ADC_read(TEMP_MULTIPLEX_CH));	//retrieve ADC reading from our channel, and convert to celsius.
		if (tempTemp[i] != 255)
		{
			sum+= tempTemp[i];		//add the temp to the sum if it's valid
			count++;				//increment the counter of valid values.
		}
	}
	avgTemp = sum/count;
	if(!count)return 255;			//if there were no valid values, return error value;
	else return avgTemp;			//otherwise return the average of the ADC_SAMPLES taken
	*/
}

void Read_all_temps(cell *_cells)
{
	cell * t_cells = _cells;
	for (uint8_t cellCount = 0; cellCount <= 7; cellCount++)
	{
		t_cells = _cells+cellCount;									//our address must increment to modify/use the next value in the cell array
		t_cells->temperature = Read_temp(t_cells);		//move through all the cells and assign temperatures.
	}
}

uint8_t TX_cellTemps(cell *_cells)
{
	int8_t mob;
	uint8_t tempData[8];
	
	for(uint8_t cellCount = 0; cellCount <= 3; cellCount++)
	{
		tempData[cellCount*2]	= _cells[cellCount].temperature >> 8;
		tempData[cellCount*2+1] = _cells[cellCount].temperature;
	}
	mob = CAN_findFreeTXMOB();
	if(mob>=0)CAN_TXMOB(mob, 8, tempData, ((uint32_t)deviceID<<18)|((uint32_t)AMUID<<8)|TEMP1_ID, 0); //transmit first 4 cell data and do not wait for finish
	else return 0;
	_delay_ms(5);
	for(uint8_t cellCount = 4; cellCount <= 7; cellCount++)
	{
		tempData[(cellCount-4)*2]	= _cells[cellCount].temperature >> 8;
		tempData[(cellCount-4)*2+1] = _cells[cellCount].temperature;
	}
	mob = CAN_findFreeTXMOB();
	if(mob>=0)CAN_TXMOB(mob, 8, tempData, ((uint32_t)deviceID<<18)|((uint32_t)AMUID<<8)|TEMP2_ID, 0); //transmit first 4 cell data and do not wait for finish
	else return 0;
	_delay_ms(5);
	return 1;
}

void Discharge_cells(cell high_1, cell high_2, cell high_3, cell lowest)					//parameters: cell to reduce; pin that the cell discharge circuit is located on; duration of discharge in seconds.
{

	uint16_t duration_high_1 = (high_1.voltage - lowest.voltage)*DISCHARGE_SCALE;			//sets the duration the highest should discharge for. Dependant on the difference between it and the lowest cell voltage
	uint16_t duration_high_2 = (high_1.voltage - lowest.voltage)*DISCHARGE_SCALE;			//sets the duration the second highest should discharge for. Dependant on the difference between it and the lowest cell voltage
	uint16_t duration_high_3 = (high_1.voltage - lowest.voltage)*DISCHARGE_SCALE;			//sets the duration the third highest should discharge for. Dependant on the difference between it and the lowest cell voltage

	uint8_t _cells = ( CellNum[high_1.cell_num] | CellNum[high_2.cell_num] | CellNum[high_3.cell_num]);			//create a byte, with bits of 1 indicating which cells are to be discharged. e.g 0b10010001 would be cell 0, cell 4 and cell 7 are the highest cells, therefore will be discharged, based upon the difference between it and the highest.
	int8_t mob = CAN_findFreeTXMOB();
	CAN_TXMOB(mob, 1, &_cells, ((uint32_t)deviceID<<18)|((uint32_t)AMUID<<8)|15, 0); //transmit cells to be discharged and do not wait for finish
	
	
	/*for(uint16_t count = 0; count < duration_high_1; count++)
	{
		PORTB |= ((_cells&CELL7)>>7)|((_cells&CELL6)>>5)|((_cells&CELL5)>>2)|(_cells&CELL4);					//turn on discharge for cells 7..4, if they are specified in _cells
		PORTC |= (((_cells&CELL3)<<4)|((_cells&CELL0)<<1));														//turn on discharge for cells 3 and 0, if their corresponding bits are specified in _cells
		PORTD |= ((_cells&CELL1)>>1|(_cells&CELL2)>>1);															//turn on discharge for cells 2 and 1, if their corresponding bits are on in _cells

		_delay_us(1);				//~50% duty cycle, ~2us pulse width.

		PORTB &= ~(((_cells&CELL7)>>7)|((_cells&CELL6)>>5)|((_cells&CELL5)>>2)|(_cells&CELL4));					//turn the discharge off.
		PORTC &= ~(((_cells&CELL3)<<4)|((_cells&CELL0)<<1));
		PORTD &= ~(((_cells&CELL1)>>1|(_cells&CELL2)>>1));

		if(count >= duration_high_3 && (_cells & CellNum[high_3.cell_num])) _cells &= ~(CellNum[high_3.cell_num]);		//when the counter reaches the duration of the high_2 and high_3, we will remove the corresponding bit in _cells, so that it does not discharge them anymore
		if(count >= duration_high_2 && (_cells & CellNum[high_2.cell_num])) _cells &= ~(CellNum[high_2.cell_num]);

	}*/
}

void Balance_on()
{
	TCCR0A = 0;
	TCCR0B = (1<<CS01);
	//TCNT0 = 0;
	OCR0A = 200;
	TIMSK0 = (1<<OCIE0A);
	
}

void Balance_off()
{
	//TCCR0B = 0;
	TIMSK0 = 0;
	PORTB &= ~((CELL2>>2)|(CELL3>>2)|(CELL5>>2)|(CELL6>>2)); //turn off discharge for cells 7..4, if they are specified in _cellsToBalance
	PORTC &= ~((CELL1)|(CELL7)); //turn off discharge for cells 3 and 0, if their corresponding bits are specified in _cellsToBalance
	PORTD &= ~((CELL0<<1)|(CELL4<<3)); //turn off discharge for cells 2 and 1, if their corresponding bits are on in _cellsToBalance
	LED_off();
}
void Balance_init(cell * _cells)
{
	//this function finds the cells with the top 3 voltage levels, and the cell with the lowest voltage. Then discharges the highest.
	cell * t_cells = _cells;

	//Analyse cell data in this section
	cell lowest = *_cells;				//assign an arbitrary cell to the lowest, for comparison
	cell high_1 = *_cells;				//assign an arbitrary cell to the highest, for comparison.

	uint8_t iscan = 0;
	for(iscan = 0; iscan <= 7; iscan++ )	//Analyse cell values 1st run, checking for invalid values and assigning highest and lowest
	{
		t_cells = _cells+iscan;
		if (t_cells->voltage >= CELL_V_ERR_MAX || t_cells->voltage <= CELL_V_ERR_MIN) return;	//if any cells give voltages outside error range, we will not attempt to balance
		if (t_cells->voltage <= lowest.voltage)lowest	= *t_cells;				//find the cell with lowest voltage
		if (t_cells->voltage > high_1.voltage)high_1	= *t_cells;				//find the cell with highest voltage
	}
	cell high_2 = lowest;			//assign lowest cell to the second highest.
	cell high_3 = lowest;			//and again assign lowest cell to the third highest.

	for(iscan = 0; iscan <= 7; iscan++)		//analyse cell values 2nd run, checking for large differences. also assign 2nd highest cell
	{
		t_cells = _cells+iscan;
		if (t_cells->voltage > lowest.voltage+CELL_V_DIFF_MAX_THRESHOLD || t_cells->voltage < lowest.voltage-CELL_V_DIFF_MAX_THRESHOLD)return;	//if any cells have difference greater than 0.5V(time of writing threshold), we can't balance, so quit
		if (t_cells->voltage >= high_2.voltage && t_cells->cell_num != high_1.cell_num )high_2 = *t_cells;										//high_2 starts at the lowest value, and checks each cell for the highest value, that isn't assigned to high_1.
	}
	for(iscan = 0; iscan <= 7; iscan++)		//3rd run, assign third highest cell value
	{
		t_cells = _cells+iscan;
		if (t_cells->voltage >= high_3.voltage && t_cells->cell_num != high_1.cell_num && t_cells->cell_num != high_2.cell_num)high_3 = *t_cells;		//high_3 starts at the lowest value, and checks each cell for the highest value, that isn't assigned to high_1 or high_2.
	}
	//IGNORE THE PREVIOUS PART OF THIS FUNCTION
	//THIS IS THE LATEST PART OF THE algorithm... is that how you spell algorithm?
	for(iscan = 0; iscan <= 7; iscan++)
	{
		t_cells = _cells+iscan;
		if(t_cells->voltage > BALANCE_TARGET_VOLTAGE)_cellsToBalance |= CellNum[t_cells->cell_num];
	}

	//Discharge_cells( high_1, high_2, high_3, lowest);			//now discharge the top three cells, to balance them
	//uint16_t duration_high_1 = (high_1.voltage - lowest.voltage)*DISCHARGE_SCALE;			//sets the duration the highest should discharge for. Dependant on the difference between it and the lowest cell voltage
	//uint16_t duration_high_2 = (high_1.voltage - lowest.voltage)*DISCHARGE_SCALE;			//sets the duration the second highest should discharge for. Dependant on the difference between it and the lowest cell voltage
	//uint16_t duration_high_3 = (high_1.voltage - lowest.voltage)*DISCHARGE_SCALE;			//sets the duration the third highest should discharge for. Dependant on the difference between it and the lowest cell voltage
	
	//OLD_cellsToBalance = ( CellNum[high_1.cell_num] | CellNum[high_2.cell_num] | CellNum[high_3.cell_num]);			//create a byte, with bits of 1 indicating which cells are to be discharged. e.g 0b10010001 would be cell 0, cell 4 and cell 7 are the highest cells, therefore will be discharged, based upon the difference between it and the highest.
	//OLD DEBUGGING_cellsToBalance = 0xff;
	//OLDint8_t mob = CAN_findFreeTXMOB();
	//OLD CAUSING POTENTIAL BROADCAST STORMS... CAN_TXMOB(mob, 1, &_cellsToBalance, ((uint32_t)deviceID<<18)|((uint32_t)AMUID<<8)|15, 0); //transmit cells to be discharged and do not wait for finish


}

void Wake_Next_CMU()
{

	PORTD |= (1<<PIND0);		//send wake out, halfway through sending our data back to the AMU.
	LED_flash(10);
	PORTD &= ~(1<<PIND0);

}
void Wake_Set()
{
	PORTD |= (1<<PIND0);		//turn wake pin on, to wake up the next CMU
}
void Wake_Unset()
{
	PORTD &= ~(1<<PIND0);		//turn wake off. our job is done.
}
void INT3_init()	//pinc0
{
	cli();		//disable global interrupt
	EICRA = (1<<ISC31)|(1<<ISC30);	//0b11000000	//to enable rising edge of INT3 on PINC0 to cause an external interrupt
	EIMSK = (1<<INT3);				//this may not be legal, the data sheet said it was read only, otherwise it might be 1<<INT1 and 1<<INT0 to represent 3 (0b00000011)
	//EIFR bits INTF3..0 hold information on whether the interrupt has occurred.
	//PCICR is useful when using the level change is being used to indicate the interrupt. hi-low or low-hi maybe
	sei();		//re-enable global interrupt.

}

void PowerDown()
{

	//SMCR = (1<<SM1)|(1<<SE);	//0b00000101	//power down mode //also known as SLEEP_MODE_PWR_DOWN. This also enables the mode with 1<<SE.
	set_sleep_mode(SLEEP_MODE_PWR_DOWN);		//power down shuts the main clock down.
	sleep_enable();
	sleep_cpu();				//enter the sleep state.

}

int main(void)
{
	_delay_ms(10);
	
	
	CMU_eeprom_write(EEPROM_DEVICE_ID, 6);
	CMU_eeprom_write(EEPROM_ADC_SAMPLES, 3);
	CMU_eeprom_write(EEPROM_FW_VERSION, 1003);
	CMU_eeprom_write(EEPROM_CELL_V_DIFF_MAX_THRESHOLD, 500);
	CMU_eeprom_write(EEPROM_CELL_V_DIFF_MIN_THRESHOLD, 500);
	CMU_eeprom_write(EEPROM_DISCHARGE_SCALE, 200);
	
	EEPROMWriteComplete = 0;
	
	IO_init();		//initialise IO and INT3 for waking up from power down mode
	//Wake_Set();
	ADC_init();		//initialises ADC
	CAN_init();		//initialises CAN, without enabling any receive MOBs
	CAN_RXInit(4, 0, 0, 0);	//set mob up for listening to audit requests
	
	//srand(ADC_read(0));
	Parameters_init();	//disable this until we have the parameters on the eeprom
	PowerDown();		//shutdown until woken for the first time
	_delay_ms(200);	//await registration messages which will be automatically handled in interrupts
	
	if(!registered)_delay_ms(800);
	if (WAKE_IN_PIN && !registered) //if the wake in pin is still high and we aren't registered, there is a problem
	{
		while(1)					//loop endlessly
		{
			LED_flash(50);			//flashing LED 3 times
			LED_flash(50);
			LED_flash(50);
			_delay_ms(2000);		//every two seconds.
			
		}	
	}
	
	CAN_RXInit(5, 0, ADMINmsk, ADMIN );				//set mob up for listening to admin messages
	cell cells[8];					//create primary cell array. this will hold all configuration and collected data on the cells.
    Cells_init(cells);
	CellBalanceMode = 0;
	//uint16_t x = 0;
	//double temp = 0.00;
	Wake_Set();
	while(WAKE_IN_PIN);
	Wake_Unset();
	PowerDown();
	
	while(1)
    {
		//mob = CAN_findFreeTXMOB();
		//if(mob>=0)CAN_TXMOB(mob, 0, 0, ((uint32_t)deviceID<<18)|((uint32_t)AMUID<<8)|AUDIT_RESPONSE, 0); //transmit registration and do not wait for finish
		//
		LED_on();
		_delay_ms(5);		//after powering up again, wait for voltages to settle
		//PORTD = (PORTD & 0xe3)|((cells[3].temp_channel<<2) & 0x1c);
		//x = ADC_read(TEMP_MULTIPLEX_CH);
		//x = 279;
		
		//temp = 0.0000004*pow(x,3) -0.006*pow(x,2) +0.3424*x -37.7329;
		
		
		
		CellBalanceMode = 0;		//after one balance process, ensure that balancing is turned off. this is to ensure that if no contact is received from the AMU, things won't blow up

		Balance_off();
		Read_all_voltages(cells);	//Reads all voltages, 3.7ms
		if(!(TX_cellVoltage(cells)))LED_flash(20); 
		Read_all_temps(cells);
		//cells[3].temperature = (uint16_t)temp;
		
		if(!(TX_cellTemps(cells)))LED_flash(20);
		//_delay_ms(10);
		Wake_Set();
			
		
		if(CellBalanceMode)
		{
			LED_off();
			Balance_init(cells);
			Balance_on();
			LED_flash(50);
		}
		
		if(!(WAKE_IN_PIN))
		{
			LED_off();
			_delay_ms(1);
			Wake_Unset();
			PowerDown();
			//LED_on();
		}
		else
		{
			_delay_ms(200);
			CANPAGE = (4 << 4);
			CANSTMOB &= ~(1 << RXOK);	//unset the RXOK bit to clear the interrupt.
		}
		CANPAGE = (4 << 4);
		CANSTMOB &= ~(1 << RXOK);	//unset the RXOK bit to clear the interrupt.
		CAN_RXInit(4,0,AMUmsk, AMU);	//set mob up for listening to the AMU
		//while(CellBalanceCounter < BALANCE_DUTY_CYCLE);
		//Balance_cells(cells);
		//Read_all_temps(cells);		//Reads all temperatures, 2.9ms

		
		//PORTD |= (1<<PIND7);		//send wake out, halfway through sending our data back to the AMU.
    }
}

ISR(INT3_vect)
{
	SMCR = (0<<SM2)|(0<<SM1)|(1<<SM0)|(0<<SE);		//disable the mode, and disable the enable bit.
}

ISR(CAN_INT_vect)
{
	//uint8_t authority;
	int8_t mob;	
	
	if((CANSIT2 & (1 << SIT4)))	//we received a CAN message on mob 5, which is set up to receive exclusively from the AMU.
	{
		
		CANPAGE = (4 << 4);			//set the canpage to the receiver MOB
		if((CANIDT4 >> 3) == AUDIT_REQUEST && !registered )	//if the received ID has packet type audit request, and we are not already registered, we will send a registration request.
		{
			mob = CAN_findFreeTXMOB();
			AMUID =  ((CANIDT2 >> 5) & 0b00000111);	//last 5 bits of CANIDT3 contain the lower 5 bits of the sender ID
			AMUID |= ((CANIDT1 << 3) & 0b11111000);	//first 3 bits of CANIDT2 contain the last 3 bits of the sender ID
			uint8_t data[2] = {(fw_version>>8) & 0xFF, fw_version & 0xFF};
			if(mob>=0)CAN_TXMOB(mob, 2, data, ((uint32_t)deviceID<<18)|((uint32_t)AMUID<<8)|AUDIT_RESPONSE, 0); //transmit registration and do not wait for finish	
			
		}
		else if((CANIDT4 >> 3)== READ_RECEIPT && !registered )
		{
			Wake_Set();
			registered = 1;
		}
		else if((CANIDT4 >> 3)== BALANCE_ON && (CANCDMOB & 0b1111) == BALANCE_NPACKETS)
		{
			//this packet processing is important, so we need to do it in here.
			for(uint8_t i = 0; i < BALANCE_NPACKETS; i++)
			{
				switch(i)
				{
					case 0:
						BALANCE_TARGET_VOLTAGE = (uint16_t)(CANMSG<<8);		//CAN byte 1
						break;
					case 1:
						BALANCE_TARGET_VOLTAGE |= CANMSG;					//CAN byte 2
						break;
					case 2:
						BALANCE_DUTY_CYCLE = CANMSG;						//CAN byte 3
						break;
					case 3:
						BALANCE_RESERVED1 = CANMSG;							//CAN byte 4
						break;
					case 4:
						BALANCE_RESERVED2 = CANMSG;							//CAN byte 5
						break;
					default:
						break;
				}
			}
			if(BALANCE_TARGET_VOLTAGE > 3200 && BALANCE_TARGET_VOLTAGE < 4200 && BALANCE_DUTY_CYCLE < 50) CellBalanceMode = 1;		//only if the packets we have received are valid, will we turn balancing on
		}
		else if((CANIDT4 >> 3)== BALANCE_OFF )
		{
			CellBalanceMode = 0;
		}
		CAN_RXInit(4,0,AMUmsk, AMU);	//set mob up for listening to the AMU
	}
	CANPAGE = (4 << 4);
	CANSTMOB &= ~(1 << RXOK);	//unset the RXOK bit to clear the interrupt.
	LED_off();
}

ISR(TIMER0_COMPA_vect)
{
	CellBalanceCounter++;
	if(CellBalanceCounter<BALANCE_DUTY_CYCLE)	//for the beginning of the duty cycle, turn cell balancing on.
	{
		LED_on();
		PORTB |= (((_cellsToBalance&CELL2)>>2)|((_cellsToBalance&CELL3)>>2)|((_cellsToBalance&CELL5)>>2)|((_cellsToBalance&CELL6)>>2));					//turn on discharge for cells 7..4, if they are specified in _cellsToBalance
		PORTC |= (_cellsToBalance&CELL1)|(_cellsToBalance&CELL7);														//turn on discharge for cells 3 and 0, if their corresponding bits are specified in _cellsToBalance
		PORTD |= ((_cellsToBalance&CELL0)<<1)|((_cellsToBalance&CELL4)<<3);									//turn on discharge for cells 2 and 1, if their corresponding bits are on in _cellsToBalance
	}
	else			//otherwise, turn the cell balancing off.
	{
		PORTB &= ~((CELL2>>2)|(CELL3>>2)|(CELL5>>2)|(CELL6>>2));					//turn off discharge for cells 7..4, if they are specified in _cellsToBalance
		PORTC &= ~((CELL1)|(CELL7));														//turn off discharge for cells 3 and 0, if their corresponding bits are specified in _cellsToBalance
		PORTD &= ~((CELL0<<1)|(CELL4<<3));									//turn off discharge for cells 2 and 1, if their corresponding bits are on in _cellsToBalance
		LED_off();
	}
	if(CellBalanceCounter >= 100)CellBalanceCounter = 0;
}