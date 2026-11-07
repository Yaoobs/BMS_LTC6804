#ifndef __OS_H
#define __OS_H

#include "main.h"

extern void Os_Init(void);
extern void ShowInformation(void);
extern uint16_t total_voltage;
extern uint16_t cell_voltage[12];
extern uint8_t OC_FLAG;
extern uint8_t balance_on_flag;
extern void SOC_Estimator(void);
extern void JudgeBMSState(void);
extern void Store_SaveOnStandby(void);
extern int Control_PostLine(const char *line);
extern void LEDXToggle(void);

#endif
