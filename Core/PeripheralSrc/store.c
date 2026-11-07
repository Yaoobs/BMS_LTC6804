#include "store.h"
#include "iwdg.h"
#include "usart1.h"
#include <string.h>

static StoreUser store_user;

static uint8_t Store_IsBlank(const StoreUser *flash)
{
    const uint8_t *byte = (const uint8_t *)flash;
    uint32_t i;

    for (i = 0; i < sizeof(StoreUser); i++)
    {
        if (byte[i] != 0xFFU)
        {
            return 0U;
        }
    }
    return 1U;
}

int8_t Store_Init(void)
{
    const StoreUser *flash = (const StoreUser *)STORE_FLASH_ADDR;

    if (Store_IsBlank(flash))
    {
        return -1;
    }
    memcpy(&store_user, flash, sizeof(store_user));
    /* USART1_SendText("\xB6\xC1\xC8\xA1\xB3\xC9\xB9\xA6"); */
    return 0;
}

StoreUser *Store_Data(void)
{
    return &store_user;
}

HAL_StatusTypeDef Store_Save(void)
{
    FLASH_EraseInitTypeDef erase;
    uint32_t page_error = 0U;
    const uint16_t *src;
    uint32_t addr;
    uint32_t i;
    uint32_t words;
    HAL_StatusTypeDef status;

    HAL_IWDG_Refresh(&hiwdg);
    __disable_irq();
    status = HAL_FLASH_Unlock();
    if (status == HAL_OK)
    {
        erase.TypeErase = FLASH_TYPEERASE_PAGES;
        erase.PageAddress = STORE_FLASH_ADDR;
        erase.NbPages = 1U;
        status = HAL_FLASHEx_Erase(&erase, &page_error);
    }
    if (status == HAL_OK)
    {
        src = (const uint16_t *)&store_user;
        words = sizeof(StoreUser) / sizeof(uint16_t);
        addr = STORE_FLASH_ADDR;
        for (i = 0; i < words; i++)
        {
            status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr, src[i]);
            if (status != HAL_OK)
            {
                break;
            }
            addr += 2U;
        }
    }
    HAL_FLASH_Lock();
    __enable_irq();
    if (status != HAL_OK)
    {
        return status;
    }
    if (memcmp(&store_user, (const void *)STORE_FLASH_ADDR, sizeof(store_user)) != 0)
    {
        return HAL_ERROR;
    }
    /* USART1_SendText("\xB1\xA3\xB4\xE6\xB3\xC9\xB9\xA6"); */
    return HAL_OK;
}

HAL_StatusTypeDef Store_Clear(void)
{
    FLASH_EraseInitTypeDef erase;
    uint32_t page_error = 0U;
    const uint8_t *byte;
    uint32_t i;
    HAL_StatusTypeDef status;

    HAL_IWDG_Refresh(&hiwdg);
    __disable_irq();
    status = HAL_FLASH_Unlock();
    if (status == HAL_OK)
    {
        erase.TypeErase = FLASH_TYPEERASE_PAGES;
        erase.PageAddress = STORE_FLASH_ADDR;
        erase.NbPages = 1U;
        status = HAL_FLASHEx_Erase(&erase, &page_error);
    }
    HAL_FLASH_Lock();
    __enable_irq();
    if (status != HAL_OK)
    {
        return status;
    }
    byte = (const uint8_t *)STORE_FLASH_ADDR;
    for (i = 0; i < STORE_PAGE_SIZE; i++)
    {
        if (byte[i] != 0xFFU)
        {
            return HAL_ERROR;
        }
    }
    /* USART1_SendText("\xC7\xE5\xB3\xFD\xB3\xC9\xB9\xA6"); */
    return HAL_OK;
}
