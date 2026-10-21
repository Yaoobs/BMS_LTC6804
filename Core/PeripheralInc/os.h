#ifndef __OS_H
#define __OS_H

#define true 1
#define false 0 


//typedef uint32_t  u32;
//typedef uint16_t u16;
//typedef uint8_t  u8;


extern void Os_Init(void);
extern void Os_Period(void);
extern void Os_Task(void);
extern void ShowInformation(void);
enum os_period
{
	OS_0MS = 0u,
	OS_1MS = 1u,
	OS_5MS = 5u,
	OS_10MS = 10u,
	OS_50MS = 50u,
	OS_100MS = 100u,
	OS_500MS = 500u,
	OS_1000MS = 1000u
};
enum Os_Period_5ms
{
	OS_5MS_a = 1u,
	OS_5MS_b = 2u,
	OS_5MS_c = 3u,
	OS_5MS_d = 4u,
	OS_5MS_e = 5u
};
#endif
