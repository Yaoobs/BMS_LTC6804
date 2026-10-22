#ifndef __USART1_H
#define __USART1_H

#include "stm32f1xx_hal.h"
#include "usart.h"
#include "state_machine.h"
#include "Ah_Integrator_Estimator.h"
#include <stdio.h>
#include <string.h>

extern void USART1_SendTxStr(void);
extern void RECEICE_DATA_DEAL(void);

extern void USART1_Init(void);

#endif
