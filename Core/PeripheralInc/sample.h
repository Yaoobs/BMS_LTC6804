#ifndef __SAMPLE_H
#define __SAMPLE_H

#include "adc.h"
#include <math.h>

extern int8_t Current_SamplePush(void);
extern void Current_CalcFromAverage(void);
extern void Temp_SamplePush(void);

extern int16_t current;
extern uint8_t temp;

#endif
