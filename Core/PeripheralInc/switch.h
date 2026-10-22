#ifndef __SWITCH_H
#define __SWITCH_H

#include "gpio.h"

extern void Only_Open_CHG(void);
extern void Only_Close_CHG(void);
extern void Only_Open_DSG(void);
extern void Only_Close_DSG(void);
extern void Close_DSG_CHG(void);

#endif
