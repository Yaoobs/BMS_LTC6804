# 内部 Flash 存储

用 STM32F103C8 的最后一页保存需要掉电保持的数据。页地址 `0x0800FC00`，页大小 1 KB，范围到 `0x0800FFFF`。读写放在 `Core/PeripheralSrc/store.c`，声明放在 `Core/PeripheralInc/store.h`。

Flash 里只放一个 `StoreUser`。SRAM 里放同名的一份。上电从最后一页抄进 SRAM，运行中只改 SRAM，需要落盘时再擦除这一页并按半字写回去。

## 地址

F103C8 的 Flash 是 64 KB，从 `0x08000000` 到 `0x0800FFFF`。中容量器件每页 1 KB，最后一页就是 `0x08000000 + 64 KB - 1 KB = 0x0800FC00`。

当前分散加载文件 `MDK-ARM/BMS_LTC6804/BMS_LTC6804.sct` 把 `ER_IROM1` 设成 `0x08000000`、长度 `0x00010000`，程序可以排到最后一页。存储类启用前，把这一段改成：

```
LR_IROM1 0x08000000 0x0000FC00  {
  ER_IROM1 0x08000000 0x0000FC00  {
```

Keil 目标选项里的 IROM 大小同样改成 `0xFC00`。改完后看 `.map`，只读段的结束地址要小于 `0x0800FC00`。

## StoreUser

成员由使用的人增删。类型用 `uint16_t`、`int16_t`、`uint32_t`，整个结构保持偶数字节，方便按半字编程。改过成员布局后，先擦除这一页或整片重写一次，否则旧字节会按新成员去解释。

```c
typedef struct
{
    uint16_t soc; /* 示例：万分比。不需要的成员删掉，需要的加在这里 */
} StoreUser;
```

接口：

```c
#define STORE_FLASH_ADDR  0x0800FC00UL

void Store_Init(void);
StoreUser *Store_Data(void);
HAL_StatusTypeDef Store_Save(void);
```

`Store_Data()` 返回 SRAM 里的 `StoreUser`。业务代码改完字段后调用 `Store_Save()`。

`Store_Init()` 从 `0x0800FC00` 直接读，不需要解锁。这一页从未写过时，擦除态是全 `0xFF`。全是 `0xFF` 就调用 `Store_SetDefault()`，只填 SRAM。否则把 `sizeof(StoreUser)` 字节复制到 SRAM。

`Store_SetDefault()` 写在 `store.c` 里，初值由使用的人填。例如 `store_user.soc = 7800`。

## 写入

F103 的编程粒度是 16 位。`HAL_FLASH_Program()` 的类型用 `FLASH_TYPEPROGRAM_HALFWORD`，地址必须是偶数。擦除按页，一次擦掉整个 `0x0800FC00` 页。

1. `HAL_IWDG_Refresh()`。擦除大约几十毫秒，看门狗现在约 8 s，擦之前喂一次。
2. `__disable_irq()`。擦除和编程期间，从 Flash 取指会停住，SysTick 和串口中断在这一段里不响应。
3. `HAL_FLASH_Unlock()`。
4. `HAL_FLASHEx_Erase()`：`TypeErase = FLASH_TYPEERASE_PAGES`，`PageAddress = 0x0800FC00`，`NbPages = 1`。
5. 把 SRAM 里的 `StoreUser` 按 `uint16_t` 从低地址写到高地址，每次地址加 2。
6. `HAL_FLASH_Lock()`，`__enable_irq()`。
7. 再从该页读回，和 SRAM 比较。不一致则返回 `HAL_ERROR`。

写入函数只在需要保存时调用，例如串口命令、状态回到待机、或参数被修改之后。每页擦写寿命大约 1 万次。放进 100 ms 任务会在十几分钟内写穿。

`StoreUser` 只有一个半字时，一次编程要么完成要么没写上。成员变多以后，掉电可能发生在写完前几个半字、后面仍是 `0xFF` 的时候。下次上电会把这种混合内容当作有效数据。

## store.h

```c
#ifndef STORE_H
#define STORE_H

#include "stm32f1xx_hal.h"
#include <stdint.h>

#define STORE_FLASH_ADDR  0x0800FC00UL

typedef struct
{
    uint16_t soc;
} StoreUser;

void Store_Init(void);
StoreUser *Store_Data(void);
HAL_StatusTypeDef Store_Save(void);

#endif
```

## store.c

```c
#include "store.h"
#include "iwdg.h"
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

static void Store_SetDefault(void)
{
    memset(&store_user, 0, sizeof(store_user));
    store_user.soc = 7800U;
}

void Store_Init(void)
{
    const StoreUser *flash = (const StoreUser *)STORE_FLASH_ADDR;

    if (Store_IsBlank(flash))
    {
        Store_SetDefault();
    }
    else
    {
        memcpy(&store_user, flash, sizeof(store_user));
    }
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
    return HAL_OK;
}
```

`sizeof(StoreUser)` 必须是偶数。若以后加入 `uint8_t`，补一个 `uint8_t reserved`，让总长度仍是偶数。

把 `store.c` 加入 Keil 工程，包含路径里已有 `Core/PeripheralInc`。`Os_Init()` 在调度和业务之前调用 `Store_Init()`。需要保存时调用 `Store_Save()`，不要在中断里调用。
