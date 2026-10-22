#include "switch.h"
//uint16_t Max, Min;

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

//void Get_Cell_Voltage_Max_Min(void)
//{
//    uint8_t i;

//    /* 第6、11、12节未接，下标5、10、11不参与比较 */
//    Max = cell_voltage[0];
//    Min = cell_voltage[0];
//    for (i = 0; i < 12; i++)
//    {
//        if (i == 5 || i == 10 || i == 11)
//        {
//            continue;
//        }
//        if (cell_voltage[i] > Max)
//        {
//            Max = cell_voltage[i];
//        }
//        if (cell_voltage[i] < Min)
//        {
//            Min = cell_voltage[i];
//        }
//    }
//}

//void Judge_CHG_DSG(void)
//{
	
//    if (Max > 4200) /* 电池最大电压大于4200mv */
//    {
//        Only_Close_CHG(); /* 关闭充电MOS管 */
//        OV_FLAG = 1;
//    }
//    if (OV_FLAG == 1) /* 过压状态 */
//    {
//        if (Max < 4100) /* 电池最大电压小于4100mv */
//        {
//            Only_Open_CHG(); /* 打开充电MOS管 */
//            OV_FLAG = 0;
//        }
//    }
//    if (Min < 2800) /* 电池最小电压低于4200mv */
//    {
//        Only_Close_DSG(); /* 关闭放电MOS管 */
//        UV_FLAG = 1;
//    }
//    if (UV_FLAG == 1) /* 欠压状态 */
//    {
//        if (Min > 3000) /* 电池最小电压大于3000mv */
//        {
//            Only_Open_DSG(); /* 打开放电MOS管 */
//            UV_FLAG = 0;
//        }
//    }
//}
