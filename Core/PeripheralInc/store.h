#ifndef STORE_H
#define STORE_H

#include "stm32f1xx_hal.h"
#include <stdint.h>

#define STORE_FLASH_ADDR  0x0800FC00UL
#define STORE_PAGE_SIZE   0x400U

typedef struct
{
    uint16_t soc;            /* 万分比 */
    uint16_t total_voltage;  /* 总电压，单位 mV */
} StoreUser;

int8_t Store_Init(void);
StoreUser *Store_Data(void);
HAL_StatusTypeDef Store_Save(void);
HAL_StatusTypeDef Store_Clear(void);

#endif
