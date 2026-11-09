#ifndef __BSP_CAN_H
#define __BSP_CAN_H

#include "main.h"
#include <stdio.h>	//使用 printf

/************ 控制线程标志 ************/
#define CTRL_FLAG_CAN      0x04U    // CAN 收到 0x200 控制命令

/************ 外部声明 ************/
extern uint8_t CAN_RxBuf[8];       // 接收缓冲区（最大 8 字节）

/************ CAN 外设引脚定义 ************/
#define CAN_RX_Pin        GPIO_PIN_11      // CAN 接收引脚 PA11
#define CAN_RX_GPIO_Port  GPIOA            // CAN RX 所属 GPIO 端口
#define CAN_TX_Pin        GPIO_PIN_12      // CAN 发送引脚 PA12
#define CAN_TX_GPIO_Port  GPIOA            // CAN TX 所属 GPIO 端口

/************ 函数声明 ************/
void CAN_Filter_Config(void);
void CAN_Init(void);
void CAN_Send(uint32_t StdId, uint8_t *pData, uint8_t Len);
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan);

#endif
