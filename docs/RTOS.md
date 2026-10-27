# 加入 RTOS 的改造方案

不再使用裸机 `os.c` 里的协作式调度。`Os_Period()`、`Os_Task()`、`TaskCount`、五个槽位和全部周期标志从工程里拿掉，`SysTick_Handler()` 里也不再调用 `Os_Period()`。周期用各线程的 `osDelayUntil()` 保持，不用软件定时器回调干活。事件由线程标志唤醒。内核用 CubeMX 自带的 FreeRTOS，接口用生成出来的 `osThreadNew`、`osDelayUntil`、`osThreadFlagsSet`、`osThreadFlagsWait`。

5 ms、10 ms、50 ms 在裸机里是空任务，RTOS 工程里不建对应线程，也不再建一组周期标志去模仿那五个槽位。

采样、总电压、`SOC_Estimator()`、`protect_step()`、`JudgeBMSState()` 仍是普通函数，由对应线程调用。上电的 5 s 空等、ADC 校准、`LTC6804_initialize()`、`USART1_Init()`、`Store_Init()` 留在 `osKernelStart()` 之前。

## 现在被挡在哪里

`main()` 的 `while (1)` 反复调用 `Os_Task()`。一次调用里先喂狗、处理串口行、读电压、走状态机并写 MOS，再按标志跑 1 ms 到 1 s 的任务。任务不返回，后面的任务就不跑。标志只有一位，挡住期间到期的节拍会丢掉。

和 RTOS 有关的现状：

| 位置 | 现在的做法 | 挡住谁 |
| --- | --- | --- |
| `Voltage_Poll()` | 发 ADCV 满 4 ms 后，一次读完四组寄存器，大约 1.4 ms，然后总电压、`SOC_Estimator()`、`protect_step()` | 排在它后面的 1 ms 电流采样。每 100 ms 可能少一个点 |
| `LTC6804_initialize()` | `HAL_Delay(1)` 唤醒，再 `HAL_Delay(10)` | 只在调度器启动前执行，可以保留 |
| `adc.c` | `HAL_ADC_PollForConversion()` 超时 10 ms，正常约 21 µs | 电流和温度共用 ADC1，现在靠主循环排队，不会同时改通道 |
| `USART1` | 中断收发，优先级 0。`SysTick` 优先级 15，里面调用 `Os_Period()` 再 `HAL_IncTick()` | 收字节可以打断节拍置位 |
| 看门狗 | 预分频 256、重载 1250，大约 8 s。每次进入 `Os_Task()` 都喂 | 主循环停住才会复位 |
| 状态机 | 每个主循环走一步。充电改放电会先进入待机并关掉两只 MOS，下一圈再进入新状态 | 这一间隔现在就是下一次主循环 |
| `Store_Save()` | 收到整行 `S` 时擦除最后一页，擦除期间关中断 | 主循环停在 Flash 操作里 |

`protect_step()`、安时积分、查表 SOC 不各自占一个线程。

## 目标

1 ms 电流采样在读四组电压寄存器时仍然执行。等 ADCV 的那 4 ms 里，电流任务继续跑。串口命令和过充过放仍能改 MOS。充电与放电切换时，两只全关只发生在同一次处理里，不拖成 100 ms。

SPI 传输时间还是大约 1.4 ms。RTOS 只是让更高优先级的电流任务可以插进来，不会把这次传输变短。

## 时基

`configTICK_RATE_HZ` 设为 1000，一个节拍是 1 ms。FreeRTOS 占用 SysTick。`HAL_GetTick()` 改到 TIM2 或 TIM4，在 CubeMX 里把 HAL 时基从 SysTick 换走。

`HAL_Delay()` 在任务里是空等，不会让出 CPU。调度器启动之后，任务里的等待用 `osDelay()` 或 `osDelayUntil()`。`osKernelStart()` 之前的 `HAL_Delay(5000)` 和 `LTC6804_initialize()` 可以留着。

USART1 现在的抢占优先级是 0。这个优先级高于 FreeRTOS 允许调用内核接口的上限，中断里不能调用 `osThreadFlagsSet`。把 USART1 改到数值更大的优先级，例如 6，并保证它低于 `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY` 所允许的那一档。中断里只置线程标志，不跑 `OnUartLine()`、`sprintf`、`state_machine_step()` 或 `protect_step()`。

## 软件定时器和 osDelayUntil

两种都能做周期调度。这个工程用 `osDelayUntil()`。

软件定时器是 `osTimerNew()` 加 `osTimerStart()`，周期到了在定时器服务任务里调用回调。FreeRTOS 只有这一个服务任务，所有定时器共用它的优先级和栈。回调里可以做很短的事。回调里再调用 `osDelay()`，服务任务停住，其它定时器在这段时间里都不会到期。

把现有工作直接放进回调时：

| 周期 | 回调里要做的事 | 结果 |
| --- | --- | --- |
| 1 ms | `Current_SamplePush()` | 能做。采样大约 21 µs |
| 100 ms | 发 ADCV，等 4 ms，读四组，总电压，SOC，保护 | 等 4 ms 只能空转或 `osDelay()`。空转期间 1 ms 回调不会进来。`osDelay()` 会把整个定时器服务停住 4 ms，1 ms 采样丢掉这 4 拍，读寄存器的大约 1.4 ms 再丢掉 1 拍 |
| 1 s | 温度、`sprintf`、发送 | `sprintf` 用的栈就是服务任务的栈，这个栈至少要 1024 字节。组帧期间 1 ms 和 100 ms 回调都排在后面 |

1 ms 和 100 ms 不能互相抢占，因为它们是同一个任务。电流采样就做不到在读寄存器和等 ADCV 时继续跑。这是软件定时器直接干活时过不去的一点。

定时器回调只发线程标志、活仍交给线程做，周期本身可以分开。这样仍然要电流、电压、1 s 三个线程，再加定时器服务任务和一条命令队列。1 ms 定时器每个节拍都往队列里送一次命令，服务任务醒来再唤醒电流线程，比 `osDelayUntil()` 多一次切换。服务任务若被别的回调占用，1 ms 命令会在队列里堆积。三个周期用这条路径，多出来的任务和队列没有换来当前线程做不到的事。

`osDelayUntil()` 是每个周期一个线程，各自保存绝对节拍。电压线程里的 `osDelay(4)` 只让出这个线程，优先级更高的电流线程照常每 1 ms 醒来。读四组寄存器时电流线程也能抢占。1 s 的 `sprintf` 只用 1 s 线程自己的栈。不建定时器服务任务，也不建定时器命令队列。

软件定时器适合数量多、每次只做很短的事，例如超时和消抖。这里只有三个周期，其中两拍里有等待和 SPI。用 `osDelayUntil()`。

## 周期调度

每个周期线程保存自己的绝对节拍，用 `osDelayUntil()` 醒来。下一次醒来时间是这次的目标节拍加上周期，不是“函数返回后再等一个周期”。读寄存器花掉的 1.4 ms、组帧花掉的时间，不会把周期越积越长。

```c
static void CurrentTask(void *argument)
{
    uint32_t tick = osKernelGetTickCount();

    for (;;)
    {
        tick += 1U;
        osDelayUntil(tick);
        Current_SamplePush();
    }
}
```

电压线程的周期是 100 ms。ADCV 发出后的 4 ms 转换等待用 `osDelay(4)`，它落在这一拍的 100 ms 里面。下一拍仍按原来的 `tick` 加 100 对齐，不把这 4 ms 再加到周期上。

```c
static void VoltageTask(void *argument)
{
    uint32_t tick = osKernelGetTickCount();

    for (;;)
    {
        tick += 100U;
        osDelayUntil(tick);
        Current_CalcFromAverage();
        LTC6804_adcv();
        osDelay(4);
        LTC6804_rdcv(cell_codes);
        Get_TotalVoltage();
        SOC_Estimator();
        protect_step();
        osThreadFlagsSet(control_id, VOLTAGE_READY);
    }
}
```

1 s 线程同样用 `osDelayUntil()`，周期 1000。

控制线程没有周期。它停在 `osThreadFlagsWait()` 上，串口收齐一行或电压线程发来 `VOLTAGE_READY` 时才醒来。

## 任务

四个线程。优先级数字越大越高。

| 线程 | 唤醒方式 | 优先级 | 做什么 |
| --- | --- | --- | --- |
| 电流 | `osDelayUntil()`，周期 1 ms | 高 | `Current_SamplePush()`。过流返回 -1 的判断仍是注释，启用后再在这里关 MOS |
| 控制 | `osThreadFlagsWait()`，串口行或 `VOLTAGE_READY` | 中 | 处理一行文本，写 `StateRequest`，置 `CriticalFault`，`state_machine_step()`，`JudgeBMSState()` |
| 电压 | `osDelayUntil()`，周期 100 ms | 低 | 电流窗口平均，发 ADCV，`osDelay(4)`，读四组，总电压，`SOC_Estimator()`，`protect_step()`，再通知控制线程 |
| 周期 | `osDelayUntil()`，周期 1 s | 最低 | 翻转 LED，采温度，`USART1_SendTxStr()`，条件满足时喂狗 |

电压线程里的 `osDelay(4)` 会让出 CPU，这 4 ms 期间电流线程照常 1 ms 采样。读四组寄存器时电流线程仍可抢占，因为 ADC 和 SPI 不是同一个外设。温度采样也要抢 ADC1，所以电流和温度必须共用一把 ADC 互斥锁。谁改通道、谁启动、谁读结果，整段放在锁里。

控制线程收到串口命令后，连续调用两次 `state_machine_step()` 的条件是：第一次结束后停在待机，而 `StateRequest` 已经是充电或放电。这样两只 MOS 全关只存在于这一次处理里。电压线程通知来的那一次走一步即可，过压欠压标志在这一步里已经是新值。

收到整行 `S` 时仍调用 `Store_SaveOnStandby()`，放在控制线程里、状态机之前。擦页会关中断几十毫秒，这段时间电流线程也不能运行。看门狗在擦除前已经喂过。

1 s 线程里的 `USART1_SendTxStr()` 和 `USART1_SendText()` 共用 `tx_data`。发送缓冲只许 `tx_busy == 0` 时改写。`Store` 的“读取成功”“保存成功”“清除成功”也走 `USART1_SendText()`，正在发送时这次提示不发。

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

1. CubeMX 打开 FreeRTOS，`configTICK_RATE_HZ` 设为 1000，HAL 时基改到定时器，USART1 优先级调到可调用内核接口的范围。`main()` 在外设和 `Store_Init()` 之后创建四个线程并 `osKernelStart()`，后面不再 `while (1)` 调用 `Os_Task()`。四个线程先只做 `osDelayUntil()`，确认能启动。
2. 电流线程接上 `Current_SamplePush()`，电压线程接上平均、ADCV、读取、总电压、`SOC_Estimator()` 和 `protect_step()`，加上 ADC 互斥锁。确认读四组寄存器时 1 ms 采样不再整拍丢失，100 ms 窗口仍是大约 100 个点。
3. 控制线程接串口标志和 `VOLTAGE_READY`。1 s 线程接 LED、温度、`USART1_SendTxStr()` 和看门狗。删掉 `Os_Period()`、`Os_Task()`、周期标志，以及 `SysTick_Handler()` 里对 `Os_Period()` 的调用。

## 这套调度解决不了的问题

保护模型里等于 0 mV 的节换成 9999 mV、PEC 失败不看返回值、`InitialSOC` 为 0 时平均电压不高于 3200 mV 不会开始积分、`int16_t` 电流溢出，都还在原来的函数里。收到 `S` 时擦除 Flash 会关中断，这几十毫秒里 1 ms 采样仍然停住。电流若要改成定时器触发加 DMA，那是 ADC 路径的改动，和要不要上 RTOS 分开做。
