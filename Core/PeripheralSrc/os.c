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
#include "spi.h"
#include "usart.h"
#include "gpio.h"
#include "sample.h"
#include "LTC6804.h"

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

#define TX_UART_HEAD_0  0xAAU
#define TX_UART_HEAD_1  0x01U
static volatile uint8_t tx_busy;

static uint8_t cmd[3];
static uint8_t cmd_index;
static volatile uint8_t cmd_ready; /* 收到完整3个字节命令 */

#define RX_UART_LEN        3U
#define RX_UART_HEAD       0x01U
#define RX_UART_TAIL       0x55U
#define RX_UART_OPEN_DSG   0x04U
#define RX_UART_CLOSE_DSG  0x05U
#define RX_UART_OPEN_CHG   0x06U
#define RX_UART_CLOSE_CHG  0x07U

static uint8_t rx_byte;
unsigned char shang[50];
int total_voltage;
unsigned int cell_voltage[12];
unsigned int Max, Min;

static uint32_t adcv_tick;
static uint8_t adcv_wait;
uint16_t cell_codes[12];
int SOC;
u8 Temp_up_flag, OV_FLAG, UV_FLAG, OC_FLAG; /* 过压，欠压，过流标志 */

#define TX_FRAME_LEN 50U

void LEDXToggle(void)
{
    HAL_GPIO_TogglePin(GPIOB, LED_Ctrl_Pin);
}

void USART1_SendShang(const uint8_t frame[TX_FRAME_LEN])
{
    char vi;

    if (frame == NULL)
    {
        return;
    }

    shang[0] = TX_UART_HEAD_0;
    shang[1] = TX_UART_HEAD_1;
    for (vi = 0; vi < 12; vi++)
    {
        shang[vi + 2 + vi] = (char)(cell_voltage[vi] >> 8);
        shang[vi + 3 + vi] = (char)(cell_voltage[vi] & 0x00FF);
    }
    shang[32] = (char)(total_voltage >> 8);
    shang[33] = (char)(total_voltage & 0xFF);
    shang[34] = (char)(SOC >> 8);
    shang[35] = (char)(SOC & 0xFF);
    shang[36] = current >> 8;
    shang[37] = current & 0x00FF;
    shang[38] = temp;
    shang[39] = HAL_GPIO_ReadPin(GPIOB, DSG_POWER_EN_Pin);
    shang[40] = HAL_GPIO_ReadPin(GPIOB, CHG_POWER_EN_Pin);
    shang[41] = (char)(Max >> 8);
    shang[42] = (char)(Max & 0x00FF);
    shang[43] = (char)(Min >> 8);
    shang[44] = (char)(Min & 0x00FF);

    tx_busy = 1;
    if (HAL_UART_Transmit_IT(&huart1, (uint8_t *)frame, TX_FRAME_LEN) != HAL_OK)
    {
        tx_busy = 0;
    }
}

void Only_Open_CHG(void)
{
    HAL_GPIO_WritePin(GPIOB, CHG_POWER_EN_Pin, GPIO_PIN_SET);
}

void Only_Close_CHG(void)
{
    HAL_GPIO_WritePin(GPIOB, CHG_POWER_EN_Pin, GPIO_PIN_RESET);
}

void Only_Open_DSG(void)
{
    HAL_GPIO_WritePin(GPIOB, DSG_POWER_EN_Pin, GPIO_PIN_SET);
}

void Only_Close_DSG(void)
{
    HAL_GPIO_WritePin(GPIOB, DSG_POWER_EN_Pin, GPIO_PIN_RESET);
}

 void Close_DSG_CHG(void)
{
    HAL_GPIO_WritePin(GPIOB, CHG_POWER_EN_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, DSG_POWER_EN_Pin, GPIO_PIN_RESET);
}

void RECEICE_DATA_DEAL(void)
{
    uint8_t local[RX_UART_LEN];

    if (!cmd_ready)
    {
        return;
    }

    local[0] = cmd[0];
    local[1] = cmd[1];
    local[2] = cmd[2];
    cmd_ready = 0;
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);

    if (local[0] != RX_UART_HEAD || local[2] != RX_UART_TAIL)
    {
        return;
    }

    switch (local[1])
    {
        case RX_UART_OPEN_DSG:
            Only_Open_DSG();
            break;
        case RX_UART_CLOSE_DSG:
            Only_Close_DSG();
            break;
        case RX_UART_OPEN_CHG:
            Only_Open_CHG();
            break;
        case RX_UART_CLOSE_CHG:
            Only_Close_CHG();
            break;
        default:
            break;
    }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    tx_busy = 0; /* 发送完毕 */
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1)
    {
        return;
    }

    if (cmd_index == 0)
    {
        if (rx_byte == RX_UART_HEAD)
        {
            cmd[cmd_index++] = rx_byte;
        }
    }
    else
    {
        cmd[cmd_index++] = rx_byte;
        if (cmd_index == RX_UART_LEN)
        {
            cmd_index = 0;
            if (cmd[2] == RX_UART_TAIL)
            {
                cmd_ready = 1;
            }
        }
    }

    if (!cmd_ready)
    {
        HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1)
    {
        return;
    }

    tx_busy = 0;
    cmd_index = 0;
    if (!cmd_ready)
    {
        HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
    }
}

void Get_SOC(void)
{
    if (total_voltage > (4100 * 9))
    {
        SOC = 100;
    }
    else if ((total_voltage > (4100 * 9)) && (total_voltage < (4150 * 9)))
    {
        SOC = 95;
    }
    else if ((total_voltage > (4050 * 9)) && (total_voltage < (4100 * 9)))
    {
        SOC = 90;
    }
    else if ((total_voltage > (4000 * 9)) && (total_voltage < (4050 * 9)))
    {
        SOC = 88;
    }
    else if ((total_voltage > (3950 * 9)) && (total_voltage < (4000 * 9)))
    {
        SOC = 87;
    }
    else if ((total_voltage > (3900 * 9)) && (total_voltage < (3950 * 9)))
    {
        SOC = 86;
    }
    else if ((total_voltage > (3900 * 9)) && (total_voltage < (3950 * 9)))
    {
        SOC = 85;
    }
    else if ((total_voltage > (3850 * 9)) && (total_voltage < (3900 * 9)))
    {
        SOC = 84;
    }
    else if ((total_voltage > (3800 * 9)) && (total_voltage < (3850 * 9)))
    {
        SOC = 83;
    }
    else if ((total_voltage > (3750 * 9)) && (total_voltage < (3800 * 9)))
    {
        SOC = 82;
    }
    else if ((total_voltage > (3700 * 9)) && (total_voltage < (3750 * 9)))
    {
        SOC = 81;
    }
    else if ((total_voltage > (3650 * 9)) && (total_voltage < (3700 * 9)))
    {
        SOC = 80;
    }
    else if ((total_voltage > (3600 * 9)) && (total_voltage < (3650 * 9)))
    {
        SOC = 79;
    }
    else if ((total_voltage > (3550 * 9)) && (total_voltage < (3600 * 9)))
    {
        SOC = 78;
    }
    else if ((total_voltage > (3500 * 9)) && (total_voltage < (3550 * 9)))
    {
        SOC = 77;
    }
    else if ((total_voltage > (3450 * 9)) && (total_voltage < (3500 * 9)))
    {
        SOC = 40;
    }
    else if ((total_voltage > (3400 * 9)) && (total_voltage < (3450 * 9)))
    {
        SOC = 30;
    }
    else if ((total_voltage > (3300 * 9)) && (total_voltage < (3400 * 9)))
    {
        SOC = 20;
    }
    else if ((total_voltage > (3200 * 9)) && (total_voltage < (3300 * 9)))
    {
        SOC = 10;
    }
    else if ((total_voltage > (3100 * 9)) && (total_voltage < (3200 * 9)))
    {
        SOC = 5;
    }
}

void Get_TotalVoltage(void)
{
    char vi;

    total_voltage = 0;
    for (vi = 0; vi < 12; vi++)
    {
        cell_voltage[vi] = cell_codes[vi] / 10;
        total_voltage += cell_voltage[vi];
    }
}

void Get_Cell_Voltage_Max_Min(void)
{
    char i;

    /* 第6、11、12节未接，下标5、10、11不参与比较 */
    Max = cell_voltage[0];
    Min = cell_voltage[0];
    for (i = 0; i < 12; i++)
    {
        if (i == 5 || i == 10 || i == 11)
        {
            continue;
        }
        if (cell_voltage[i] > Max)
        {
            Max = cell_voltage[i];
        }
        if (cell_voltage[i] < Min)
        {
            Min = cell_voltage[i];
        }
    }
}

void Judge_CHG_DSG(void)
{
    if (Max > 4200) /* 电池最大电压大于4200mv */
    {
        Only_Close_CHG(); /* 关闭充电MOS管 */
        OV_FLAG = 1;
    }
    if (OV_FLAG == 1) /* 过压状态 */
    {
        if (Max < 4100) /* 电池最大电压小于4100mv */
        {
            Only_Open_CHG(); /* 打开充电MOS管 */
            OV_FLAG = 0;
        }
    }
    if (Min < 2800) /* 电池最小电压低于4200mv */
    {
        Only_Close_DSG(); /* 关闭放电MOS管 */
        UV_FLAG = 1;
    }
    if (UV_FLAG == 1) /* 欠压状态 */
    {
        if (Min > 3000) /* 电池最小电压大于3000mv */
        {
            Only_Open_DSG(); /* 打开放电MOS管 */
            UV_FLAG = 0;
        }
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
    Get_SOC();
    Get_Cell_Voltage_Max_Min();
    Judge_CHG_DSG();
}

void Os_Init(void)
{
    HAL_ADCEx_Calibration_Start(&hadc1); /* 校准 ADC */
	LTC6804_initialize();                /* LTC6804初始化配置 */
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1); /* 打开 USART1 接收 */
    Only_Open_CHG();                     /* 打开充电MOS管 */
    Only_Open_DSG();                     /* 打开放电MOS管 */

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
    /* 10 ms 任务里只发命令 */
    LTC6804_adcv();
    adcv_tick = HAL_GetTick();
    adcv_wait = 1;
    Current_CalcFromAverage();
}

static void Task_50ms(void)
{
}

static void Task_100ms(void)
{
}

static void Task_1000ms(void)
{
    LEDXToggle();
    Temp_SamplePush();
    if (tx_busy == 0)
    {
        USART1_SendShang(shang);
    }
}

static void SetModelInput(void)
{
}

static void SetModelOutput(void)
{
}
