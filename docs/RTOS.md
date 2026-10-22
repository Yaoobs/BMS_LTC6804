# 加入 RTOS 的改造方案

针对当前 `Os_Task()` 协作式循环。内核用 CubeMX 自带的 FreeRTOS，接口用生成出来的 CMSIS-RTOS v2（`osThreadNew`、`osDelay`）。STM32F103C8T6 的 SRAM 是 20 KB，现在启动文件里的栈是 1 KB、堆是 512 字节。任务不要按现有槽位各建一个，5 ms、10 ms、50 ms 任务是空的，对应线程不建。

## 现在被挡在哪里

`main()` 的 `while (1)` 反复调用 `Os_Task()`。一次调用里先喂狗、处理串口行、读电压、走状态机并写 MOS，再按标志跑 1 ms 到 1 s 的任务。任务不返回，后面的任务就不跑。标志只有一位，挡住期间到期的节拍会丢掉。

和 RTOS 有关的现状：

| 位置 | 现在的做法 | 挡住谁 |
| --- | --- | --- |
| `Voltage_Poll()` | 发 ADCV 满 4 ms 后，一次读完四组寄存器，大约 1.4 ms | 排在它后面的 1 ms 电流采样。每 100 ms 可能少一个点 |
| `LTC6804_initialize()` | `HAL_Delay(1)` 唤醒，再 `HAL_Delay(10)` | 只在调度器启动前执行，可以保留 |
| `adc.c` | `HAL_ADC_PollForConversion()` 超时 10 ms，正常约 21 µs | 电流和温度共用 ADC1，现在靠主循环排队，不会同时改通道 |
| `USART1` | 中断收发，优先级 0。`SysTick` 优先级 15，里面调用 `Os_Period()` 再 `HAL_IncTick()` | 收字节可以打断节拍置位 |
| 看门狗 | 预分频 256、重载 1250，大约 8 s。每次进入 `Os_Task()` 都喂 | 主循环停住才会复位 |
| 状态机 | 每个主循环走一步。充电改放电会先进入待机并关掉两只 MOS，下一圈再进入新状态 | 这一间隔现在就是下一次主循环 |

`protect_step()`、安时积分、查表 SOC 仍是普通函数，不需要各自一个线程。

## 目标

1 ms 电流采样在读四组电压寄存器时仍然执行。等 ADCV 的那 4 ms 里，电流任务继续跑。串口命令和过充过放仍能在同一轮里改 MOS，充电与放电切换时两只全关的间隔保持一次函数调用，而不是一个周期。

SPI 传输时间还是大约 1.4 ms。RTOS 只是让更高优先级的电流任务可以插进来，不会把这次传输变短。

## 时基

FreeRTOS 占用 SysTick。`HAL_GetTick()` 改到 TIM2 或 TIM4，在 CubeMX 里把 HAL 时基从 SysTick 换走。`Os_Period()` 和六个布尔标志删掉。

`HAL_Delay()` 在任务里是空等，不会让出 CPU。调度器启动之后，任务里的等待用 `osDelay()`。`Os_Init()` 里上电空等 5 s 发生在 `osKernelStart()` 之前，这里的 `HAL_Delay(5000)` 可以留着。`LTC6804_initialize()` 也留在启动调度器之前。

USART1 现在的抢占优先级是 0。这个优先级高于 FreeRTOS 允许调用内核接口的上限，中断里不能调用 `osThreadFlagsSet` 一类函数。把 USART1 改到数值更大的优先级，例如 6，并保证它低于 `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY` 所允许的那一档。只做收发完成通知，不在中断里跑 `OnUartLine()`、`sprintf`、`state_machine_step()` 或 `protect_step()`。

## 任务

四个线程。优先级数字越大越高。

| 线程 | 周期或唤醒 | 优先级 | 做什么 |
| --- | --- | --- | --- |
| 电流 | 1 ms | 高 | `Current_SamplePush()`。过流返回 -1 的判断仍是注释，启用后再在这里关 MOS |
| 控制 | 串口行到达，或电压线程通知保护结果已更新 | 中 | 处理一行文本，写 `StateRequest`，置 `CriticalFault`，`state_machine_step()`，`JudgeBMSState()` |
| 电压 | 100 ms | 低 | 电流窗口平均，发 ADCV，`osDelay(4)`，读四组，总电压，`protect_step()`，`SOC_Estimator()`，再通知控制线程 |
| 周期 | 1 s | 最低 | 翻转 LED，采温度，`USART1_SendTxStr()` |

电压线程里的 `osDelay(4)` 会让出 CPU，这 4 ms 期间电流线程照常 1 ms 采样。读四组寄存器时电流线程仍可抢占，因为 ADC 和 SPI 不是同一个外设。温度采样也要抢 ADC1，所以电流和温度必须共用一把 ADC 互斥锁，谁改通道、谁启动、谁读结果，整段放在锁里。

控制线程收到串口命令后连续调用两次 `state_machine_step()` 的条件是：第一次结束后停在待机，而 `StateRequest` 已经是充电或放电。这样两只 MOS 全关只存在于这一次处理里，不会拖成 100 ms。电压线程通知来的那一次走一步即可，过压欠压标志在这一步里已经是新值。

1 s 线程里的 `USART1_SendTxStr()` 已经会看 `tx_busy`。发送缓冲只许这个线程在 `tx_busy == 0` 时改写。

看门狗不要放在最高优先级线程里。电流、电压、控制各自在跑完一轮后置一个存活标志。1 s 线程看到三个标志都在最近一个周期内置过，才 `HAL_IWDG_Refresh()`，然后清掉标志。某一线程停住后，大约 8 s 内复位。

## 堆栈和内存

不要用现在这 1 KB 的 MSP 去跑业务。建议起步大小：

| 线程 | 栈 |
| --- | --- |
| 电流 | 512 字节 |
| 控制 | 512 字节 |
| 电压 | 768 字节 |
| 周期（含 `sprintf`） | 1024 字节 |
| 空闲 | 256 字节 |
| FreeRTOS `heap_4` | 6 KB 起，用来放线程控制块和栈 |

合计大约 9 KB，20 KB SRAM 放得下。`configASSERT` 打开。每个线程的栈水位用 `uxTaskGetStackHighWaterMark()` 看一次，再把余量收到 128 字节左右。

`Current`、`cell_voltage`、`total_voltage`、`SOC`、`InitialSOC`、`OV_FLAG`、`UV_FLAG`、`BMS_State`、`StateRequest` 仍是全局量。电压线程写电压和 SOC，周期线程读它们组串口帧，读取前关一小段中断或用互斥锁抄出一份局部副本。`StateRequest` 只在控制线程里写。

## 分三步改

1. CubeMX 打开 FreeRTOS，HAL 时基改到定时器，USART1 优先级调到可调用内核接口的范围。先只建一个线程，里面仍调用现在的 `Os_Task()`，确认能启动、看门狗会喂、串口命令仍然开关 MOS。
2. 拆出电流线程和电压线程，加上 ADC 互斥锁。确认读四组寄存器时 1 ms 采样不再整拍丢失，100 ms 窗口仍是大约 100 个点。
3. 拆出控制线程和 1 s 线程，串口改为中断里通知。删掉 `Os_Period()`、周期标志和空的 5 ms、10 ms、50 ms 任务。

## 这套调度解决不了的问题

保护模型里等于 0 mV 的节换成 9999 mV、PEC 失败不看返回值、SOC 查表里到不了的档位、`int16_t` 电流溢出，都还在原来的函数里。电流若要改成定时器触发加 DMA，那是 ADC 路径的改动，和要不要上 RTOS 分开做。
