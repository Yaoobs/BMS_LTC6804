#include "port.h"
#include "mb.h"
#include "mbport.h"
#include "main.h"

/* TIM2 clock is 72 MHz. PSC 3599 -> 20 kHz, one tick is 50 us. */

BOOL xMBPortTimersInit(USHORT usTim1Timerout50us)
{
    if (usTim1Timerout50us == 0U)
    {
        return FALSE;
    }

    __HAL_RCC_TIM2_CLK_ENABLE();
    TIM2->CR1 = TIM_CR1_OPM;
    TIM2->PSC = 3599U;
    TIM2->ARR = (uint32_t)usTim1Timerout50us - 1U;
    TIM2->EGR = TIM_EGR_UG;
    TIM2->SR = (uint16_t)~TIM_SR_UIF;
    TIM2->DIER = 0U;
    HAL_NVIC_SetPriority(TIM2_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);
    return TRUE;
}

void vMBPortTimersEnable(void)
{
    TIM2->CNT = 0U;
    TIM2->SR = (uint16_t)~TIM_SR_UIF;
    TIM2->DIER |= TIM_DIER_UIE;
    TIM2->CR1 |= TIM_CR1_CEN;
}

void vMBPortTimersDisable(void)
{
    TIM2->CR1 &= ~TIM_CR1_CEN;
    TIM2->DIER &= ~TIM_DIER_UIE;
    TIM2->SR = (uint16_t)~TIM_SR_UIF;
}

void vMBPortTimersDelay(USHORT usTimeOutMS)
{
    (void)usTimeOutMS;
}

void TIM2_IRQHandler(void)
{
    if ((TIM2->SR & TIM_SR_UIF) == 0U)
    {
        return;
    }
    TIM2->SR = (uint16_t)~TIM_SR_UIF;
    vMBPortTimersDisable();
    if (pxMBPortCBTimerExpired != 0)
    {
        (void)pxMBPortCBTimerExpired();
    }
}
