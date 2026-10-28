#ifndef __SAMPLE_H
#define __SAMPLE_H

#include "adc.h"
#include <math.h>

extern void Current_SamplePush(void);
extern void Current_CalcFromAverage(void);
extern uint8_t Current_WindowCount(void);
extern void Current_WindowClear(void);
extern void Temp_SamplePush(void);

extern int16_t Current;
extern uint8_t temp;

#endif
