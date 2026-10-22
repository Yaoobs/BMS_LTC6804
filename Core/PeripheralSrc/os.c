/*
 * @Author       : Steve Yao
 * @Date         : 2026-05-31
 * @LastEditors  : Steve Yao
 * @LastEditTime : 2026-09-27
 * @Description  :
 * @FilePath     :
 */
#include "os.h"
#include "main.h"
#include "adc.h"
#include "iwdg.h"
#include "sample.h"
#include "LTC6804.h"
#include "protect.h"
#include "usart1.h"
#include "switch.h"

uint16_t TaskCount;                  /* For task period count */
uint8_t Period_5ms_Count;            /* 5ms task period count */
uint8_t Period_10ms_Count;           /* 10ms task period count */
uint8_t Period_50ms_Count;           /* 20ms task period count */
uint8_t Period_100ms_Count;          /* 100ms task period count */
uint8_t Period_1000ms_Count;         /* 1000ms task period count */

_Bool volatile Period_1ms_Expire_Flag = false;
_Bool volatile Period_5ms_Expire_Flag = false;
_Bool volatile Period_10ms_Expire_Flag = false;
_Bool volatile Period_50ms_Expire_Flag = false;
_Bool volatile Period_100ms_Expire_Flag = false;
_Bool volatile Period_1000ms_Expire_Flag = false;

static void Task_1ms(void);
static void Task_5ms(void);
static void Task_10ms(void);
static void Task_50ms(void);
static void Task_100ms(void);
static void Task_1000ms(void);
static void SetModelInput(void);
static void SetModelOutput(void);

/* 业务 */
uint16_t total_voltage;
uint16_t cell_voltage[12];

uint16_t InitialSOC;

static uint32_t adcv_tick;
static uint8_t adcv_wait;
uint16_t cell_codes[12];
//int8_t SOC;
uint8_t Temp_up_flag, OC_FLAG; /* 过压，欠压，过流标志 */

void LEDXToggle(void)
{
    HAL_GPIO_TogglePin(GPIOB, LED_Ctrl_Pin);
}


uint16_t Get_SOC(void)
{
    if (total_voltage > (4100 * 9))
    {
        return 100;
    }
    else if ((total_voltage > (4100 * 9)) && (total_voltage < (4150 * 9)))
    {
        return 95;
    }
    else if ((total_voltage > (4050 * 9)) && (total_voltage < (4100 * 9)))
    {
        return 90;
    }
    else if ((total_voltage > (4000 * 9)) && (total_voltage < (4050 * 9)))
    {
        return 88;
    }
    else if ((total_voltage > (3950 * 9)) && (total_voltage < (4000 * 9)))
    {
        return 87;
    }
    else if ((total_voltage > (3900 * 9)) && (total_voltage < (3950 * 9)))
    {
        return 86;
    }
    else if ((total_voltage > (3900 * 9)) && (total_voltage < (3950 * 9)))
    {
        return 85;
    }
    else if ((total_voltage > (3850 * 9)) && (total_voltage < (3900 * 9)))
    {
        return 84;
    }
    else if ((total_voltage > (3800 * 9)) && (total_voltage < (3850 * 9)))
    {
        return 83;
    }
    else if ((total_voltage > (3750 * 9)) && (total_voltage < (3800 * 9)))
    {
        return 82;
    }
    else if ((total_voltage > (3700 * 9)) && (total_voltage < (3750 * 9)))
    {
        return 81;
    }
    else if ((total_voltage > (3650 * 9)) && (total_voltage < (3700 * 9)))
    {
        return 80;
    }
    else if ((total_voltage > (3600 * 9)) && (total_voltage < (3650 * 9)))
    {
        return 79;
    }
    else if ((total_voltage > (3550 * 9)) && (total_voltage < (3600 * 9)))
    {
        return 78;
    }
    else if ((total_voltage > (3500 * 9)) && (total_voltage < (3550 * 9)))
    {
        return 77;
    }
    else if ((total_voltage > (3450 * 9)) && (total_voltage < (3500 * 9)))
    {
        return 40;
    }
    else if ((total_voltage > (3400 * 9)) && (total_voltage < (3450 * 9)))
    {
        return 30;
    }
    else if ((total_voltage > (3300 * 9)) && (total_voltage < (3400 * 9)))
    {
        return 20;
    }
    else if ((total_voltage > (3200 * 9)) && (total_voltage < (3300 * 9)))
    {
        return 10;
    }
    else if ((total_voltage > (3100 * 9)) && (total_voltage < (3200 * 9)))
    {
        return 5;
    }
	return 0;
}

void SOC_Estimator() 
{
	if(BMS_State == BMS_Standby)
	{
		SOC = Get_SOC() * 100;
		InitialSOC = SOC;
		Ah_Integrator_Estimator_initialize();//待机时需要把积分器清零
	}
	else 	
	{
		Ah_Integrator_Estimator_step();
	}
	
}

void Get_TotalVoltage(void)
{
    uint8_t vi;

    total_voltage = 0;
    for (vi = 0; vi < 12; vi++)
    {
        cell_voltage[vi] = cell_codes[vi] / 10;
        total_voltage += cell_voltage[vi];
    }
}


void JudgeBMSState()
{
	if(BMS_State == BMS_Standby) {
		Only_Close_DSG();
		Only_Close_CHG();
	}
	else if(BMS_State == BMS_Driving)
	{
		Only_Close_CHG();
		Only_Open_DSG();
	}
	else if(BMS_State == BMS_Charging)
	{
		Only_Close_DSG();
		Only_Open_CHG();
	}else if (BMS_State == BMS_Fault)
	{
		Only_Close_CHG();
		Only_Close_DSG();
	}
}
/* Os_Task() 里每次都调用，不要写在 while 里空等 */
void Voltage_Poll(void)
{
    if (adcv_wait == 0)
    {
        return;
    }
    if ((uint32_t)(HAL_GetTick() - adcv_tick) < 4U)
    {
        return;
    }

    adcv_wait = 0;
    LTC6804_rdcv(cell_codes);
    Get_TotalVoltage();
	protect_step();
    
//    Get_Cell_Voltage_Max_Min();
//    Judge_CHG_DSG();
}

void Os_Init(void)
{
	HAL_Delay(5000); //初始化增加延时
    HAL_ADCEx_Calibration_Start(&hadc1); /* 校准 ADC */
	LTC6804_initialize();                /* LTC6804初始化配置 */
    USART1_Init();
//    Only_Open_CHG();                     /* 打开充电MOS管 */
//    Only_Open_DSG();                     /* 打开放电MOS管 */


}

void Os_Period(void)
{
    TaskCount++;
    Period_1ms_Expire_Flag = true; /* 1ms period */
    switch (TaskCount)
    {
        case OS_5MS_a:
            Period_5ms_Expire_Flag = true;
            break;
        case OS_5MS_b:
            Period_10ms_Count++;
            if (Period_10ms_Count >= 2) /* 5 ms * 2 = 10 ms */
            {
                Period_10ms_Expire_Flag = true;
                Period_10ms_Count = 0;
            }
            break;
        case OS_5MS_c:
            Period_50ms_Count++;
            if (Period_50ms_Count >= 10) /* 5 ms * 10 = 50 ms */
            {
                Period_50ms_Expire_Flag = true;
                Period_50ms_Count = 0;
            }
            break;
        case OS_5MS_d:
            Period_100ms_Count++;
            if (Period_100ms_Count >= 20) /* 5 ms * 20 = 100 ms */
            {
                Period_100ms_Expire_Flag = true;
                Period_100ms_Count = 0;
            }
            break;
        case OS_5MS_e:
            TaskCount = 0;
            Period_1000ms_Count++;
            if (Period_1000ms_Count >= 200) /* 5 ms * 200 = 1000 ms */
            {
                Period_1000ms_Expire_Flag = true;
                Period_1000ms_Count = 0;
            }
            break;
        default:
            break;
    }
}

void Os_Task(void)
{
    HAL_IWDG_Refresh(&hiwdg);
    RECEICE_DATA_DEAL();
    Voltage_Poll();
	
	CriticalFault = (boolean_T)(OV_FLAG || UV_FLAG);
	
	state_machine_step();
	JudgeBMSState();

    if (Period_1ms_Expire_Flag)
    {
        Task_1ms();
        Period_1ms_Expire_Flag = false;
    }
    if (Period_5ms_Expire_Flag)
    {
        Task_5ms();
        Period_5ms_Expire_Flag = false;
    }
    if (Period_10ms_Expire_Flag)
    {
        Task_10ms();
        Period_10ms_Expire_Flag = false;
    }
    if (Period_50ms_Expire_Flag)
    {
        Task_50ms();
        Period_50ms_Expire_Flag = false;
    }
    if (Period_100ms_Expire_Flag)
    {
        Task_100ms();
        Period_100ms_Expire_Flag = false;
    }
    if (Period_1000ms_Expire_Flag)
    {
        Task_1000ms();
        Period_1000ms_Expire_Flag = false;
    }
}

static void Task_1ms(void)
{
	if(Current_SamplePush() == -1)
	{
		OC_FLAG = 1;
		Close_DSG_CHG();	
	} 
}

static void Task_5ms(void)
{
}

static void Task_10ms(void)
{



}

static void Task_50ms(void)
{

}

static void Task_100ms(void)
{
	Current_CalcFromAverage();

    /* 10 ms 任务里只发命令 */
    LTC6804_adcv();
    adcv_tick = HAL_GetTick();
    adcv_wait = 1;
	SOC_Estimator();
	
}

static void Task_1000ms(void)
{
    LEDXToggle();
    Temp_SamplePush();
   
    USART1_SendTxStr();
}

static void SetModelInput(void)
{
}

static void SetModelOutput(void)
{
}
