#ifndef __SAMPLE_H
#define __SAMPLE_H

#include "adc.h"
#include <math.h>

extern int Current_SamplePush(void);
extern void Current_CalcFromAverage(void);
extern void Temp_SamplePush(void);

extern int current;
extern int temp;

#endif
