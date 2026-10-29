#ifndef __USART1_H
#define __USART1_H

#include "stm32f1xx_hal.h"
#include "usart.h"
#include "state_machine.h"
#include "Ah_Integrator_Estimator.h"
#include <stdio.h>
#include <string.h>

#define TX_FRAME_LEN 96U

typedef struct
{
    uint8_t len;
    uint8_t data[TX_FRAME_LEN];
} TxFrame;

extern void USART1_SendTxStr(uint16_t soc, uint16_t total_mv, int16_t current);
extern void USART1_SendText(const char *text);
extern void RECEICE_DATA_DEAL(void);

extern void USART1_Init(void);

#endif
