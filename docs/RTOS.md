# RTOS 方案

周期仍是三档：1 ms 采电流，100 ms 产出电流平均和一整帧单体电压，1 s 做温度和上报。电流平均、单体电压各用一条队列交给 SOC。SOC 没有自己的周期，两条队列到齐才计算，算完再把结果放进下一条队列。命令、发送用同样的方式：谁产生谁入队，只有一个线程取出。

不把 `Os_Task()` 放进一个线程。`Os_Period()`、槽位和周期标志删掉。5 ms、10 ms、50 ms 在裸机里是空的，这里不建线程。状态机、过充过放、安时积分仍用原来的模型函数。上电 5 s 空等、ADC 校准、`LTC6804_initialize()`、`USART1_Init()`、`Store_Init()` 放在 `osKernelStart()` 之前。

内核是 CubeMX 的 FreeRTOS，接口用 CMSIS-RTOS v2。`configTICK_RATE_HZ = 1000`。FreeRTOS 占用 SysTick，`HAL_GetTick()` 改到 TIM2 或 TIM4。USART1 优先级调到允许在中断里入队的那一档。

## 数据怎么走

```text
1 ms 电流线程
    采样入本线程的窗口
    每 100 ms 做一次平均 ──► current_q ──┐
                                         │
100 ms 电压线程                           ├──► SOC 线程 ──► result_q ──► 控制线程
    发 ADCV，等 4 ms                      │                 （状态机、MOS、均衡）
    四组各读一次，合成一帧 ──► voltage_q ──┘
                                                          └── 抄一份给 1 s 上报

命令中断 ──► cmd_q ──► 控制线程
控制 / Store / 1 s ──► tx_q ──► 发送完成后再取下一条
```

| 队列 | 深度 | 一条消息里有什么 | 谁放入 | 谁取出 |
| --- | --- | --- | --- | --- |
| `current_q` | 1 | 平均电流、本窗口点数、节拍 | 电流线程，每 100 ms | SOC 线程 |
| `voltage_q` | 1 | 12 节毫伏、总电压、四组 PEC 掩码、节拍 | 电压线程，四组都读完 | SOC 线程 |
| `result_q` | 1 | SOC、总电压、电流、过压、欠压、过流 | SOC 线程 | 控制线程 |
| `cmd_q` | 4 | 一行文本，最多 32 字节 | USART1 中断 | 控制线程 |
| `tx_q` | 4 | 固定长度文本 | 1 s 上报、Store 成功提示 | 发送完成回调 |

深度为 1 的三条测量队列，放入时如果已满，先取出丢掉再放，只留最新一拍。SOC 或控制线程慢的时候，生产者不被堵在队列上，也没有旧拍堆积。

## 四组电压分四次读

要拆开读。入队仍是一帧。

现在 `LTC6804_rdcv()` 一次循环读完 RDCVA～RDCVD，大约 1.4 ms，SPI 锁会一直占着。拆成四次之后，每一组大约 0.3 ms：上锁、读一组、解锁。两组之间电流线程能完成一次 1 ms 采样，控制线程也能插入一次均衡写配置。等 ADCV 的 4 ms 用 `osDelay(4)`，这段不持有 SPI 锁。

队列里不放“半帧”。SOC 和过充过放要看 12 节里的最大、最小和总电压，一组 3 节到了还不能算。四组都解析完，才把 12 节毫伏和总电压放进 `voltage_q`。某一组 PEC 失败时，这一组留着上一次的毫伏，并在掩码里记下组号。整帧照样入队。

```c
osMutexAcquire(spiMutexHandle, osWaitForever);
LTC6804_adcv();
osMutexRelease(spiMutexHandle);
osDelay(4);
for (group = 1; group <= 4; group++) {
    osMutexAcquire(spiMutexHandle, osWaitForever);
    LTC6804_rdcv_group(group, cell_codes);
    osMutexRelease(spiMutexHandle);
    /* PEC 失败则保留这一组旧码，并在 pec_mask 里记下组号 */
}
/* 12 节换成毫伏并求和，再放入 voltageQueueHandle */
```

`LTC6804_rdcv_group()` 读一组并做 PEC 校验。通过才写入这一组 3 节原始码，失败返回 -1，原始码不动。`LTC6804_rdcv()` 仍是把四组连着读完，电压线程不调用它。运行步骤见下面的电压线程。

## SOC 用两条队列

电压消息是这一拍的触发。单体电压 100 ms 才有一帧，安时积分的步长也是 0.1 s。SOC 线程阻塞在 `voltage_q` 上。醒来后用超时 0 把 `current_q` 里的平均电流取出来；队列空就沿用上一拍电流。两条消息的节拍允许相差一个 100 ms 窗口。

然后做现在这些计算，顺序不变：

1. 用电压消息里的总电压做查表。`total_voltage == 0` 时不做估算。
2. `InitialSOC != 0` 时用电流消息里的平均电流走 `Ah_Integrator_Estimator_step()`。高于 80% 或低于 30%，并且和查表相差超过 10% 时，改用查表值并清积分量。
3. `InitialSOC == 0` 时按现有规则决定采用查表值还是留下当前 SOC，然后清积分量。
4. `protect_step()` 用这一帧的 12 节毫伏。过流看电流消息：`current >= 2000` 或 `current <= -2000`。
5. 把 SOC、总电压、电流、过压、欠压、过流放进 `result_q`，并抄进 1 s 线程要读的那份快照。

查表、积分和保护仍是原来的函数。SOC 线程只负责从两条队列取数、调用它们、再把结果送出去。

## 线程

优先级从高到低：电流、控制、电压、SOC、1 s。

| 线程 | 周期 | 做什么 |
| --- | --- | --- |
| 电流 | `osDelayUntil(1 ms)` | 采一个点进本线程窗口。墙钟满 100 ms 就平均并放入 `current_q`，然后清窗口。落后超过 1 ms 就把节拍对齐到现在，不补采 |
| 电压 | `osDelayUntil(100 ms)` | 发 ADCV，`osDelay(4)`，四组各读一次，合成一帧放入 `voltage_q` |
| SOC | 无 | 等 `voltage_q`，取最新电流，估算并保护，放入 `result_q` |
| 控制 | 无 | 先处理 `cmd_q`，再处理 `result_q`。走状态机，改 MOS，按状态写均衡 |
| 1 s | `osDelayUntil(1000 ms)` | 温度、LED、把快照组帧放入 `tx_q`，三个存活标志都在才喂狗 |

控制线程不能同时阻塞在两条队列上。命令中断和 SOC 线程各置一个线程标志，控制线程等标志，再去对应队列里取。先把已经排着的命令处理完，再处理保护结果。这样一行 `C` 或 `D` 不用等下一帧电压。

命令使状态落到待机，而请求已经是充电或放电时，同一次处理里再调用一次 `state_machine_step()`。两只 MOS 全关只留在这一次里。`S` 先走完状态机，再 `Store_SaveOnStandby()`，保存的是 SOC 线程已经发布的那一拍。

均衡写配置只在控制线程、状态机之后：仍是待机才计算并 `send_balance_commend()`，离开待机才 `exit_balance_commend()`。写的时候获取 `spi_mutex`，不在电压线程里写。

1 s 线程组帧读的是 SOC 发布的快照，用 `data_mutex` 只包住那一次拷贝。`sprintf` 在锁外面。温度采样和电流采样共用 `adc_mutex`，包住改通道、启动、读结果。

发送完成回调从 `tx_q` 取下一条再启动发送。队列里还有空位时，Store 的成功提示不会因为 `tx_busy` 被直接丢掉。队列满时丢掉新的提示。

看门狗放在 1 s 线程。电流、电压、控制每次做完自己的一拍，向 1 s 线程置一个存活标志。三个都看到才喂，然后清掉。某个线程停住后，大约 8 s 内复位。

### 电流线程

电流线程自己做平均，窗口不出本线程，不用为环形缓冲再加锁。1 ms 的单个采样点不进队列，只有平均结果出队。1000 条/秒的消息会把时间花在拷贝上。平均按 100 ms 墙钟，不按“凑满 100 个点”。漏采时点数会少于 100，消息里的 `count` 交给 SOC 知道这一拍窗口变短。积分步长仍按 0.1 s，不按点数缩放。

`CurrentTask` 的步骤：

1. 记下起始节拍 `tick`，再记下本窗口起点 `window`。
2. 每次循环先看现在的节拍。已经比 `tick` 晚了超过 1 ms，就把 `tick` 改成现在，不把漏掉的点补采成一串。
3. `tick` 加 1，`osDelayUntil(tick)`。醒来的间隔是 1 ms。
4. 获取 `adc_mutex`，调用 `Current_SamplePush()`，然后释放锁。返回时若 ADC 失败，窗口长度不变。
5. 给 1 s 线程置电流存活标志。
6. 现在距离 `window` 还不到 100 ms，回到第 2 步。
7. 满 100 ms 且 `current_mv_count` 为 0，不入队，只把 `window` 改成现在。
8. 点数大于 0 时调用 `Current_CalcFromAverage()`。把 `Current`、`Current_WindowCount()` 和当前节拍放进 `currentQueueHandle`。队列深度是 1，已经满就先取出丢掉再放。
9. `Current_WindowClear()` 把 `current_mv_pos` 和 `current_mv_count` 清 0，`window` 改成现在。下一窗只含新点。
10. 回到第 2 步。

毫伏换算仍在 `Current_SamplePush()` 和 `Current_CalcFromAverage()` 里。当前代码已经按第 1～3 步和第 6～10 步跑起来。第 4 步的 `adc_mutex`、第 5 步的存活标志要等温度线程和 1 s 喂狗接上再加，所以 `CurrentTask` 里这两步先空着。

### 电压线程

电压线程只发转换命令、分四组读回、合成一帧放进 `voltageQueueHandle`。查表、安时积分、保护、状态机和 MOS 不在这个线程里。`cell_codes[12]` 留在 `VoltageTask` 里，跨周期保留。原始码 1 LSB = 100 μV，毫伏是 `cell_codes / 10`。12 节都加进总电压。`0xFFFF` 仍会变成约 6553 mV，这一步不过滤。

线程名 `"voltage"`，栈 768 字节，优先级 `osPriorityNormal`，低于电流线程。

`VoltageTask` 的步骤：

1. 记下起始节拍 `tick`。`cell_codes[12]` 是静态数组，上电为 0。PEC 失败时沿用上一拍。
2. 每次循环先看现在的节拍。已经比 `tick` 晚了超过 100 ms，就把 `tick` 改成现在，不把漏掉的 ADCV 连着补发。
3. `tick` 加 100，`osDelayUntil(tick)`。醒来的间隔是 100 ms。
4. 获取 `spiMutexHandle`，调用 `LTC6804_adcv()`，然后释放锁。命令仍是 `0x0360`。
5. `osDelay(4)`。这 4 ms 不持有 SPI 锁。
6. 组号从 1 到 4：获取 `spiMutexHandle`，调用 `LTC6804_rdcv_group()`，然后释放锁。返回 0 才写入这一组 3 节原始码；返回 -1 则这三个原始码不动，并把 `pec_mask` 的对应位置 1。bit0 到 bit3 对应第 1 到第 4 组。
7. 四组都处理完，把 12 个原始码换成毫伏并求和。不管 PEC 是否失败都放进 `voltageQueueHandle`。队列深度是 1，已经满就先取出丢掉再放。节拍用读完时的 `osKernelGetTickCount()`。第一次上电、四组都失败时，毫伏和总电压是 0，也入队。
8. 给 1 s 线程置电压存活标志。
9. 回到第 2 步。

当前代码已经按第 1～7 步和第 9 步跑起来。第 8 步的存活标志要等 1 s 喂狗接上再加，所以 `VoltageTask` 里这一步先空着。

## 互斥量

| 锁 | 谁用 | 包住什么 |
| --- | --- | --- |
| `adc_mutex` | 电流线程、1 s 线程 | 一次 ADC 转换 |
| `spi_mutex` | 电压线程、控制线程 | 一次 ADCV、一组读取，或一次写配置。等 4 ms 时不持有 |
| `data_mutex` | SOC 线程、1 s 线程 | 发布或抄出 SOC、总电压、电流 |

`StateRequest`、`BMS_State` 和两只 MOS 只由控制线程写，不再加锁。

## 内存

| 对象 | 大小 |
| --- | --- |
| 电流栈 | 512 字节 |
| 电压栈 | 768 字节 |
| SOC 栈 | 768 字节 |
| 控制栈 | 512 字节 |
| 1 s 栈，含 `sprintf` | 1024 字节 |
| 空闲栈 | 256 字节 |
| `current_q` | 1 条平均结果 |
| `voltage_q` | 1 条，12 节毫伏加总电压 |
| `result_q` | 1 条结果 |
| `cmd_q` | 4 × 32 字节 |
| `tx_q` | 4 × 80 字节 |
| `heap_4` | 8 KB 起 |

20 KB SRAM 放得下。打开 `configASSERT`，用栈水位再把余量收到大约 128 字节。

## 分三步接

1. 建五条队列、三把锁和五个线程。电流只按 1 ms 空转，电压按 100 ms 空转，SOC 和控制阻塞在队列上。确认能启动。
2. 电流窗口和平均入队，电压四次读取后入队。SOC 取出两条消息，确认 100 ms 有一帧电压、一条平均电流，读一组寄存器时 1 ms 采样还在跑。
3. 接上结果队列、命令队列、状态机、MOS 和 1 s 上报。然后删掉 `Os_Period()`、`Os_Task()`，以及 `SysTick_Handler()` 里对 `Os_Period()` 的调用。

## 队列解决不了的部分

`0xFFFF` 换成约 6553 mV、等于 0 mV 的节在保护里换成 9999、`InitialSOC` 为 0 且平均电压不高于 3200 mV 时不开始积分、`int16_t` 电流绕回后过流被写成 0，仍在原来的计算函数里。队列只把这一拍的数送进去。

待机时模型仍是充电请求优先于故障。过流已经在结果里时，一条 `C` 仍会进入充电。这要改状态机模型。

擦除最后一页时中断关掉，1 ms 采样在这几十毫秒里仍然停止。分散加载文件仍把 `0x0800FC00` 划在程序区里。
