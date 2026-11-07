#include "port.h"
#include "mb.h"
#include "mbport.h"
#include "usart.h"

extern UART_HandleTypeDef huart1;

void vMBPortClose(void)
{
}

void vMBPortSerialEnable(BOOL xRxEnable, BOOL xTxEnable)
{
    if (xRxEnable)
    {
        USART1->CR1 |= USART_CR1_RXNEIE;
    }
    else
    {
        USART1->CR1 &= ~USART_CR1_RXNEIE;
    }

    if (xTxEnable)
    {
        USART1->CR1 |= USART_CR1_TXEIE;
    }
    else
    {
        USART1->CR1 &= ~USART_CR1_TXEIE;
    }
}

BOOL xMBPortSerialInit(UCHAR ucPort, ULONG ulBaudRate, UCHAR ucDataBits, eMBParity eParity)
{
    (void)ucPort;
    (void)ulBaudRate;
    (void)ucDataBits;
    (void)eParity;

    if (huart1.Instance != USART1)
    {
        return FALSE;
    }
    return TRUE;
}

BOOL xMBPortSerialPutByte(CHAR ucByte)
{
    USART1->DR = (uint8_t)ucByte;
    return TRUE;
}

BOOL xMBPortSerialGetByte(CHAR *pucByte)
{
    uint32_t sr;

    sr = USART1->SR;
    *pucByte = (CHAR)(USART1->DR & 0xFFU);
    (void)sr;
    return TRUE;
}

void MBPort_USART1_IRQHandler(void)
{
    uint32_t sr = USART1->SR;
    uint32_t cr1 = USART1->CR1;

    if (((sr & USART_SR_RXNE) != 0U) && ((cr1 & USART_CR1_RXNEIE) != 0U))
    {
        if (pxMBFrameCBByteReceived != 0)
        {
            (void)pxMBFrameCBByteReceived();
        }
        else
        {
            (void)USART1->DR;
        }
    }
    else if ((sr & USART_SR_ORE) != 0U)
    {
        (void)USART1->DR;
    }

    if (((sr & USART_SR_TXE) != 0U) && ((cr1 & USART_CR1_TXEIE) != 0U))
    {
        if (pxMBFrameCBTransmitterEmpty != 0)
        {
            (void)pxMBFrameCBTransmitterEmpty();
        }
    }
}
