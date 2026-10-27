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
