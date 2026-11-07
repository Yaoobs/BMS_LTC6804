# Modbus 方案

本机做 Modbus RTU 从机，走现有 USART1（PA9 发、PA10 收，115200 8N1）。保持寄存器上做两件事：功能码 `0x03` 读总电压、电流、SOC 和充放电 MOS；功能码 `0x06` 写一个控制命令，包含待机、充电、放电和 `Clear`。主站发一帧，从机回一帧，用来确认收发、CRC 和 3.5 字符间隔。

协议栈用目录里的 FreeModbus 1.6.0，只编 RTU、读保持寄存器和写单个保持寄存器。ASCII、TCP、线圈、离散量、输入寄存器、写多个寄存器这一步都不进工程。`demo/` 不编，里面的 `demo/BARE/demo.c` 是另一份示例主函数，串口和定时器也是空的。

## 和现在的串口

`usart1.c` 里的行解析和发送队列还在，调用已经注释：

- `Os_Init()` 不再调用 `USART1_Init()`，不会挂上 HAL 的单字节接收。
- 报告线程不再调用 `USART1_SendTxStr()`。
- `USART1_IRQHandler()` 调用 `MBPort_USART1_IRQHandler()`。原来的 `HAL_UART_IRQHandler()` 保持注释。

充电、放电、待机和 `Clear` 由 `0x06` 进入原来的控制线程。

## 编进工程的文件

协议栈：

| 文件 | 作用 |
| --- | --- |
| `freemodbus-1.6.0/modbus/mb.c` | `eMBInit`、`eMBEnable`、`eMBPoll` |
| `freemodbus-1.6.0/modbus/rtu/mbrtu.c` | RTU 收发状态机 |
| `freemodbus-1.6.0/modbus/rtu/mbcrc.c` | CRC16 |
| `freemodbus-1.6.0/modbus/functions/mbfuncholding.c` | 功能码 `0x03` 和 `0x06` |
| `freemodbus-1.6.0/modbus/functions/mbutils.c` | 异常码转换，保持寄存器会调用它 |

包含路径加上 `freemodbus-1.6.0/modbus/include` 和 `freemodbus-1.6.0/modbus/rtu`。

`mbconfig.h` 里这三项为 1：`MB_RTU_ENABLED`、`MB_FUNC_READ_HOLDING_ENABLED`、`MB_FUNC_WRITE_HOLDING_ENABLED`。`MB_FUNC_WRITE_HOLDING_ENABLED` 对应的是写单个寄存器（功能码 6），不是写多个。其余 `MB_ASCII_ENABLED`、`MB_TCP_ENABLED`、线圈、离散量、输入寄存器、写多个保持寄存器、读写多个保持寄存器、Report Slave ID 都改为 0。

`mbfuncholding.c` 读数量的低字节已改回 `|=`。原来的赋值保留为注释。

移植层在 `Core/PeripheralSrc`，已经加入 Keil 工程：

| 文件 | 内容 |
| --- | --- |
| `port.h` | `BOOL`、`UCHAR`、`USHORT`、`ULONG`，以及临界区宏 |
| `portevent.c` | 一格事件标志，照 `demo/BARE/port/portevent.c` |
| `portserial.c` | USART1 的 RXNE / TXE |
| `porttimer.c` | TIM2，超时单位 50 µs |
| `mbreg.c` | `eMBRegHoldingCB()`；线圈和离散量回调返回 `MB_ENOREG` |

临界区用保存再恢复 `PRIMASK` 的方式。`mbrtu.c` 在接收中断路径里也会进出临界区，不能用 `taskENTER_CRITICAL()`。

## 串口移植

`MX_USART1_UART_Init()` 已经把波特率、引脚和中断优先级配好，优先级是 5。`xMBPortSerialInit()` 发现 USART1 已初始化就返回 `TRUE`，不再改波特率。`eMBInit()` 的端口号参数固定传 0，移植层忽略它。

`vMBPortSerialEnable(rx, tx)` 只开关 `RXNEIE` 和 `TXEIE`。收到一字节时，中断里调用 `pxMBFrameCBByteReceived()`，协议栈接着调用 `xMBPortSerialGetByte()` 取走 `DR`。发送寄存器空时调用 `pxMBFrameCBTransmitterEmpty()`，协议栈再调用 `xMBPortSerialPutByte()` 写 `DR`。

溢出错误（ORE）要在中断里读 `SR` 再读 `DR` 清掉，否则后面的 RXNE 不再出现。不要同时用 `HAL_UART_Receive_IT()` / `HAL_UART_Transmit_IT()`，HAL 也会读 `DR`，一个字节会被两边抢走。

## 3.5 字符定时

115200 高于 19200。`mbrtu.c` 这时不用 3.5 个字符时间，固定向移植层要 35 个 50 µs，也就是 1750 µs。`xMBPortTimersInit(35)` 按这个长度配 TIM2。

TIM2 时钟是 72 MHz（APB1 是 36 MHz，定时器时钟再乘 2）。预分频 3599，计数频率 20 kHz，一个计数 50 µs。自动重装载写成 `usTimeOut50us - 1`。`vMBPortTimersEnable()` 把计数清零并打开更新中断；到期后关掉定时器，调用 `pxMBPortCBTimerExpired()`。更新中断优先级也设为 5。

TIM4 是 HAL 的 1 ms 时基，SysTick 是 FreeRTOS，这两处不动。`HAL_TIM_PeriodElapsedCallback()` 里已经判断了 `TIM4`，TIM2 的到期不要写进那个分支，在 `porttimer.c` 自己的中断里完成。

## 线程

`eMBInit(MB_RTU, 1, 0, 115200, MB_PAR_NONE)` 和 `eMBEnable()` 放在 Modbus 线程开头，调度器起来之后再开接收中断。从机地址用 1。

新线程名 `mb`，优先级 `osPriorityLow`，栈 1024 字节。循环里调用 `eMBPoll()`，然后 `osDelay(1)`。事件标志只有一格，轮询停太久，下一帧会把上一帧的事件盖掉；`mbrtu.c` 发现接收状态已经不是空闲，会放弃这次应答。1 ms 轮询对 1750 µs 的帧间隔够用。

这线程不加入看门狗的存活判断。电流、电压、控制、报告这几路照旧。

`eMBRegHoldingCB()` 在 `eMBPoll()` 里调用，已经离开串口中断。读寄存器只复制数值，MOS 直接读引脚。写命令不在这条线程里调用 `state_machine_step()` 或 `Store_Clear()`。

## 保持寄存器

`0x03` 和 `0x06` 都进 `eMBRegHoldingCB()`。帧内地址从 0 起，`mbfuncholding.c` 加 1 再交给回调。读写用参数 `eMode` 区分：`MB_REG_READ` 或 `MB_REG_WRITE`。数值都是大端。

| 帧内地址 | 回调地址 | 读 `0x03` | 写 `0x06` |
| --- | --- | --- | --- |
| 0 | 1 | `total_voltage`，单位 mV | 拒绝 |
| 1 | 2 | `Current`，按 `int16_t` 的位型 | 拒绝 |
| 2 | 3 | `SOC`，单位 0.01% | 拒绝 |
| 3 | 4 | 充电 MOS，PB8 高电平为 1 | 拒绝 |
| 4 | 5 | 放电 MOS，PB9 高电平为 1 | 拒绝 |
| 5 | 6 | 拒绝 | 控制命令 |

`0x03` 只接受落在地址 0～4 里的区间。从 0 读 5 个寄存器是这一步的读帧。读到地址 5，或写地址 0～4，回调返回 `MB_ENOREG`，应答异常码 02（非法地址）。MOS 为 1 表示引脚是高电平，和现在文本上报里的 `ON` 相同；为 0 表示 `OFF`。

控制命令和原来的文本一致，写的是帧内地址 5：

| 写入值 | 含义 | 交给控制线程的文本 |
| --- | --- | --- |
| 0 | 待机 | `S` |
| 1 | 充电 | `C` |
| 2 | 放电 | `D` |
| 3 | 擦除保存页 | `Clear` |

回调把对应字符串放进现有命令队列，由控制线程的 `Control_HandleLine()` 处理。待机、充电、放电仍写 `StateRequest`、步进状态机、切换 MOS；待机仍调用 `Store_SaveOnStandby()`。`Clear` 仍只调用 `Store_Clear()`，擦最后一页，不步进状态机。控制线程优先级高于 `mb`，命令会先被处理，再发出 `0x06` 的回显。队列满时返回 `MB_ETIMEDOUT`，应答异常码 06（从机忙）。

`Store_Clear()` 擦除时会关中断，回显要等擦除结束。`store.c` 里读取成功、保存成功、清除成功三处 `USART1_SendText()` 都已注释。结果只看 `0x06` 的回显。回显表示命令已经交给控制线程；`Control_HandleLine()` 不看 `Store_Clear()` 的返回值，擦除失败时回显仍是原请求。

其它取值返回 `MB_EINVAL`。这份协议栈把该错误报成异常码 04（从机设备故障）。这一步不改这个映射。

## 帧

读地址 0 起的 5 个保持寄存器，请求是：

```text
01 03 00 00 00 05 85 C9
```

`01` 是从机地址，`03` 是功能码，`00 00` 是起始地址，`00 05` 是数量，`85 C9` 是 CRC，低字节在前。五个寄存器都是 0 时，应答是：

```text
01 03 0A 00 00 00 00 00 00 00 00 00 00 24 B6
```

`0A` 是后面的数据字节数。总电压、电流、SOC 或 MOS 有值以后，十字节数据换成对应的大端数值，CRC 随之变化。

写待机、充电、放电和 `Clear`（地址 5，值 0、1、2、3）。`0x06` 成功时把原请求原样回显：

```text
01 06 00 05 00 00 99 CB
01 06 00 05 00 01 58 0B
01 06 00 05 00 02 18 0A
01 06 00 05 00 03 D9 CA
```

主站工具选 RTU、从机 1、115200 8N1。读用功能码 03，数量 5；写用功能码 06，地址 5。超时给到 100 ms 以上。
