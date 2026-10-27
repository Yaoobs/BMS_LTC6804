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
#include "store.h"
#include "balance.h"

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
uint8_t Temp_up_flag, OC_FLAG, balance_on_flag; /* 过压，欠压，过流标志 */

void LEDXToggle(void)
{
    HAL_GPIO_TogglePin(GPIOB, LED_Ctrl_Pin);
}



const uint16_t ocv_table[] = {4200, 4150, 4100, 4050, 4000, 3950, 3900, 3850, 3800, 3750, 3700, 3650, 3600, 3550, 3500, 3450, 3400, 3300, 3200, 3100};
const uint16_t soc_table[] = {10000, 9500, 9000, 8800, 8700, 8600, 8500, 8400, 8300, 8200, 8100, 8000, 7900, 7800, 7700, 4000, 3000, 2000, 1000, 500};

#define TABLE_SIZE (sizeof(ocv_table) / sizeof(ocv_table[0]))

/**
 * @brief  根据单体电压查表获取 SOC (整数线性插值)
 * @param  voltage_mV: 输入的单体电压，单位 mV
 * @retval SOC 万分比 (0 ~ 10000)，代表 0.00% ~ 100.00%
 */
uint16_t Get_SOC_From_OCV(uint16_t voltage_mV) {
    // 1. 边界处理：超出表格范围直接返回极值
    if (voltage_mV >= ocv_table[0]) {
        return soc_table[0]; // 高于 4200mV，返回 10000 (100%)
    }
    if (voltage_mV <= ocv_table[TABLE_SIZE - 1]) {
        return soc_table[TABLE_SIZE - 1]; // 低于 3100mV，返回 0 (0%)
    }

    // 2. 遍历表格，寻找电压所在的区间
    for (uint8_t i = 0; i < TABLE_SIZE - 1; i++) {
        if (voltage_mV <= ocv_table[i] && voltage_mV >= ocv_table[i + 1]) {
            
            uint16_t v_high = ocv_table[i];
            uint16_t v_low  = ocv_table[i + 1];
            uint16_t soc_high = soc_table[i];
            uint16_t soc_low  = soc_table[i + 1];

            // 3. 线性插值公式
            // 注意：由于 soc_diff 最大可能达到 1000，v_diff 最大可能达到 1000，
            // 乘法结果 1000*1000=1,000,000，依然在 uint32_t (约42亿) 安全范围内。
            uint32_t v_diff = voltage_mV - v_low;
            uint32_t soc_diff = soc_high - soc_low;
            uint32_t v_range = v_high - v_low;

            uint16_t soc_result = soc_low + (uint16_t)((v_diff * soc_diff) / v_range);
            
            return soc_result;
        }
    }
    
    return 0; 
}

void SOC_Estimator() 
{
	uint8_t count = 9;
	if(total_voltage == 0) return;
	if(InitialSOC != 0)
	{
		Ah_Integrator_Estimator_step();
		if(SOC >= 8000 || SOC<= 3000)//运行中的“两头校准”
		{
			uint16_t SOC_OCV = Get_SOC_From_OCV(((uint16_t)total_voltage+count/2)/count);
			int16_t offset = SOC_OCV - SOC;
			if(offset > 1000 || offset < -1000)
			{
				SOC = SOC_OCV;
				InitialSOC = SOC;
				Ah_Integrator_Estimator_initialize();//需要把积分器清零
			}
		}
	} else 
	{
		
		uint16_t SOC_OCV = Get_SOC_From_OCV(((uint16_t)total_voltage+count/2)/count);
		int16_t offset = SOC_OCV - SOC;
		if(offset > 1000 || offset < -1000)//上电后，读取的SOC超出校准SOC 10%,用校准SOC
		{
			SOC = SOC_OCV;
			InitialSOC = SOC;
		} else 
		{
			InitialSOC = SOC;
		}
		
		Ah_Integrator_Estimator_initialize();//待机时需要把积分器清零
	}
}

void Store_SaveOnStandby(void)
{
    StoreUser *user;

    user = Store_Data();
    user->soc = SOC;
    user->total_voltage = total_voltage;
    Store_Save();
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
	SOC_Estimator();

	protect_step();
	CriticalFault = (boolean_T)(OV_FLAG || UV_FLAG || OC_FLAG);
	
	state_machine_step();
	JudgeBMSState();
    
//	if(BMS_State == BMS_Standby) 
//	{
//		balance_step();
//		send_balance_commend();
//		balance_on_flag = 1;
//	} else 
//	{
//		if(balance_on_flag == 1)
//		{
//			exit_balance_commend();
//			balance_on_flag = 0;
//		}
//	}
}

void Os_Init(void)

{	StoreUser *user;
	
	HAL_Delay(5000); //初始化增加延时
    HAL_ADCEx_Calibration_Start(&hadc1); /* 校准 ADC */
	LTC6804_initialize();                /* LTC6804初始化配置 */
    USART1_Init();
	
	if (Store_Init() == 0)
    {
        user = Store_Data();
        SOC = user->soc;
        total_voltage = user->total_voltage;
		
    }


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
	Current_SamplePush();
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
	OC_FLAG = Current >= 2000 || Current <= -2000;

    /* 10 ms 任务里只发命令 */
    LTC6804_adcv();
    adcv_tick = HAL_GetTick();
    adcv_wait = 1;
	
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
