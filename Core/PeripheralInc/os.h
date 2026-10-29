#ifndef __OS_H
#define __OS_H

#include "main.h"

extern void Os_Init(void);
extern void Os_Period(void);
extern void Os_Task(void);
extern void ShowInformation(void);
extern uint16_t total_voltage;
extern uint16_t cell_voltage[12];
extern uint8_t OC_FLAG;
extern uint8_t balance_on_flag;
extern void SOC_Estimator(void);
extern void JudgeBMSState(void);
extern void Store_SaveOnStandby(void);
extern void Control_PostLine(const char *line);
extern void LEDXToggle(void);

enum Os_Period_5ms
{
	OS_5MS_a = 1u,
	OS_5MS_b = 2u,
	OS_5MS_c = 3u,
	OS_5MS_d = 4u,
	OS_5MS_e = 5u
};
#endif
