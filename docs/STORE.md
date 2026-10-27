# 内部 Flash 存储

用 STM32F103C8 的最后一页保存需要掉电保持的数据。页地址 `0x0800FC00`，页大小 1 KB，范围到 `0x0800FFFF`。读写放在 `Core/PeripheralSrc/store.c`，声明放在 `Core/PeripheralInc/store.h`。

Flash 里只放一个 `StoreUser`。SRAM 里放同名的一份。上电调用 `Store_Init()`：读到数据就抄进 SRAM 并返回 0；这一页全是 `0xFF` 时不改 SRAM，返回 -1。运行中只改 SRAM，需要落盘时再擦除这一页并按半字写回去。

## 地址

F103C8 的 Flash 是 64 KB，从 `0x08000000` 到 `0x0800FFFF`。中容量器件每页 1 KB，最后一页就是 `0x08000000 + 64 KB - 1 KB = 0x0800FC00`。

当前分散加载文件 `MDK-ARM/BMS_LTC6804/BMS_LTC6804.sct` 仍把 `ER_IROM1` 设成 `0x08000000`、长度 `0x00010000`。收到 `S` 时已经会调用 `Store_Save()`。启用保存前要把这一段改成：

```
LR_IROM1 0x08000000 0x0000FC00  {
  ER_IROM1 0x08000000 0x0000FC00  {
```

Keil 目标选项里的 IROM 大小同样改成 `0xFC00`。改完后看 `.map`，只读段的结束地址要小于 `0x0800FC00`。

## StoreUser

成员由使用的人增删。类型用 `uint16_t`、`int16_t`、`uint32_t`，整个结构保持偶数字节，方便按半字编程。改过成员布局后，调用 `Store_Clear()` 擦掉这一页，否则旧字节会按新成员去解释。

```c
typedef struct
{
    uint16_t soc;            /* 万分比 */
    uint16_t total_voltage;  /* 总电压，单位 mV */
} StoreUser;
```

接口：

```c
#define STORE_FLASH_ADDR  0x0800FC00UL
#define STORE_PAGE_SIZE   0x400U

int8_t Store_Init(void);
StoreUser *Store_Data(void);
HAL_StatusTypeDef Store_Save(void);
HAL_StatusTypeDef Store_Clear(void);
```

`Store_Data()` 返回 SRAM 里的 `StoreUser`。业务代码改完字段后调用 `Store_Save()`。

`Store_Init()` 从 `0x0800FC00` 直接读，不需要解锁。这一页从未写过时，擦除态是全 `0xFF`。全是 `0xFF` 返回 -1，不填默认值。否则把 `sizeof(StoreUser)` 字节复制到 SRAM，返回 0。返回 -1 时不要使用 `Store_Data()` 里的内容。

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

`StoreUser` 现在是两个半字，先写 `soc`，再写 `total_voltage`。掉电可能发生在只写完 `soc`、`total_voltage` 仍是 `0xFFFF` 的时候。下次上电不会把它当成空白页，会把 `65535` mV 当作有效总电压。

## 清除

`Store_Clear()` 只擦除 `0x0800FC00` 这一页，不再把 `StoreUser` 写回去。擦除完成后这一页 1024 字节都是 `0xFF`。下次上电 `Store_Init()` 看到全 `0xFF`，返回 -1。

步骤和 `Store_Save()` 的擦除相同：先喂狗，关中断，解锁，`HAL_FLASHEx_Erase()` 的 `NbPages = 1`，然后上锁并开中断。擦完后从 `STORE_FLASH_ADDR` 起检查 `STORE_PAGE_SIZE` 个字节。有任何一个不是 `0xFF` 就返回 `HAL_ERROR`。

这个函数不改 SRAM 里的 `store_user`，也不改正在使用的 `SOC` 和 `total_voltage`。清除之后如果再调用 `Store_Save()`，或再收到 `S`，运行中的值会重新写入这一页。不要在中断或 100 ms 任务里调用。

读取、保存、清除都只在成功后发一行。`Store_Init()` 返回 0 时发送 `读取成功`。`Store_Save()` 校验通过后发送 `保存成功`。`Store_Clear()` 确认整页都是 `0xFF` 后发送 `清除成功`。三行都加 `\r\n`，走 `USART1_SendText()`。它和 1 秒状态文本共用 `tx_data`，`tx_busy != 0` 时这次提示不发。保存和清除的发送放在开中断之后。源文件里这三句用 GBK 字节写出，和现有串口文本一致。

## store.h

```c
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
```

## store.c

```c
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
    USART1_SendText("\xB6\xC1\xC8\xA1\xB3\xC9\xB9\xA6");
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
    USART1_SendText("\xB1\xA3\xB4\xE6\xB3\xC9\xB9\xA6");
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
    USART1_SendText("\xC7\xE5\xB3\xFD\xB3\xC9\xB9\xA6");
    return HAL_OK;
}
```

`sizeof(StoreUser)` 必须是偶数。若以后加入 `uint8_t`，补一个 `uint8_t reserved`，让总长度仍是偶数。

把 `store.c` 加入 Keil 工程，包含路径里已有 `Core/PeripheralInc`。`Os_Init()` 在调度和业务之前调用 `Store_Init()`。返回 0 表示已从 Flash 装入 SRAM，返回 -1 表示这一页没有数据。需要保存时调用 `Store_Save()`，不要在中断里调用。

## 示例

上电时装入。返回 0 时只写入 `SOC` 和 `total_voltage`，不写 `InitialSOC`。返回 -1 时这两个量保持 0。第一次读到非 0 总电压时，`SOC_Estimator()` 再决定保留这个 `SOC` 还是改成查表值。

```c
if (Store_Init() == 0)
{
    user = Store_Data();
    SOC = user->soc;
    total_voltage = user->total_voltage;
}
```

收到整行 `S` 时保存。`OnUartLine()` 先写 `StateRequest`，再调用 `Store_SaveOnStandby()`。这次调用在同一次 `Os_Task()` 的 `Voltage_Poll()` 之前，写入的是上一拍的 `SOC` 和 `total_voltage`。

```c
else if (strcmp(line, "S") == 0)
{
    StateRequest = Standby;
    Store_SaveOnStandby();
}
```

```c
void Store_SaveOnStandby(void)
{
    StoreUser *user;

    user = Store_Data();
    user->soc = SOC;
    user->total_voltage = total_voltage;
    Store_Save();
}
```

`Store_Save()` 不要放进 100 ms 任务，也不要在中断里调用。返回值不是 `HAL_OK` 时，这一页的内容和 SRAM 不一致。下次上电 `Store_Init()` 仍可能返回 0，并装入写到一半的数据。

要丢掉这一页里已经保存的数据，调用 `Store_Clear()`。成功后这一页恢复成全 `0xFF`，下次上电 `Store_Init()` 返回 -1。当前这次运行里的 `SOC` 和 `total_voltage` 还在，之后再收到 `S` 会把它们重新写入。
