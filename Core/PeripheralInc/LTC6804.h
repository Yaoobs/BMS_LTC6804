#ifndef __LTC6804_H
#define __LTC6804_H

#include "gpio.h"
#include <stdio.h>

void wakeup_idle(void);
uint16_t pec15_calc(uint8_t len, //Number of bytes that will be used to calculate a PEC
					uint8_t *data //Array of data that will be used to calculate  a PEC
					);
extern void LTC6804_initialize(void);
extern void LTC6804_adcv(void);
extern int8_t LTC6804_rdcv(uint16_t cell_codes[12]);
extern void send_balance_commend(void);
extern void exit_balance_commend(void);

void spi_write_read(uint8_t tx_Data[],//array of data to be written on SPI port
					uint8_t tx_len, //length of the tx data arry
					uint8_t *rx_data,//Input: array that will store the data read by the SPI port
					uint8_t rx_len //Option: number of bytes to be read from the SPI port
					);

int8_t LTC6804_rdcfg(uint8_t r_config[8] //A two dimensional array that the function stores the read configuration data.
					 );
void LTC6804_wrcfg(uint8_t config[6] //A two dimensional array of the configuration data that will be written
				   );

#endif
