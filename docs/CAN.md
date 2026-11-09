# CAN 通信报文方案

## 1. 现状分析

### 1.1 硬件与外设配置

| 项目 | 值 | 来源 |
|------|-----|------|
| CAN 外设 | CAN1 | [can.c:40](file:///d:/BaiduNetdiskDownload/matlab/bms/BMS_LTC6804/Core/Src/can.c#L40) |
| 引脚 | PA11(RX) / PA12(TX) | [can.c:76-77](file:///d:/BaiduNetdiskDownload/matlab/bms/BMS_LTC6804/Core/Src/can.c#L76-L77) |
| 波特率 | 500 kbps | Prescaler=4, BS1=9TQ, BS2=8TQ → 36M/(4×18)=500k |
| 模式 | CAN_MODE_NORMAL | [can.c:42](file:///d:/BaiduNetdiskDownload/matlab/bms/BMS_LTC6804/Core/Src/can.c#L42) |
| 自动总线恢复 | AutoBusOff=ENABLE | [can.c:47](file:///d:/BaiduNetdiskDownload/matlab/bms/BMS_LTC6804/Core/Src/can.c#L47) |
| 接收中断 | FIFO0 挂起中断，NVIC 优先级 5 | [can.c:90-91](file:///d:/BaiduNetdiskDownload/matlab/bms/BMS_LTC6804/Core/Src/can.c#L90-L91) |
| 初始化时机 | `Os_Init()` 中调用 `CAN_Init()`，在调度器启动前 | [os.c:166](file:///d:/BaiduNetdiskDownload/matlab/bms/BMS_LTC6804/Core/PeripheralSrc/os.c#L166) |

### 1.2 现有软件结构

- [can1.c](file:///d:/BaiduNetdiskDownload/matlab/bms/BMS_LTC6804/Core/PeripheralSrc/can1.c) 已实现：
  - `CAN_Filter_Config()` — 滤波器**只放行 0x123**（ID=0x123, Mask=0x7FF）
  - `CAN_Init()` — 配置滤波器 + `HAL_CAN_Start` + 激活 FIFO0 接收中断
  - `CAN_Send(StdId, pData, Len)` — 通用发送函数（成功分支为空）
  - `HAL_CAN_RxFifo0MsgPendingCallback()` — 读入 `CAN_RxBuf[8]`，**未做任何处理**
- `CAN_RxBuf[8]` 全局接收缓冲，无消费者

### 1.3 相关全局数据（0x100 上报来源）

| 变量 | 类型 | 含义 | 写入者 |
|------|------|------|--------|
| `total_voltage` | uint16_t | 总电压 (mV) | dataTask |
| `Current` | int16_t | 电流 (mA) | dataTask |
| `SOC` | uint16_t | 电量 (0~10000 = 0~100.00%) | dataTask |
| `BMS_State` | BMS_State_Enum | 0=Standby, 1=Charging, 2=Driving, 3=Fault | state_machine_step |

### 1.4 控制命令现有通路

- `Control_PostLine(line)` → `cmdQueue`（char[32]）→ `osThreadFlagsSet(CTRL_FLAG_CMD)`
- `ControlTask` 收到标志后从 `cmdQueue` 取出文本，调用 `Control_HandleLine`：
  - `"D"` → `StateRequest = Driving`
  - `"C"` → `StateRequest = Charging`
  - `"S"` → `StateRequest = Standby`，并 `Store_SaveOnStandby()`
  - 随后 `state_machine_step()` → `JudgeBMSState()`

---

## 2. 报文定义

### 2.1 0x100 — 周期上报帧（BMS → 主控）

- **方向**：发送
- **周期**：100 ms
- **ID**：0x100（标准帧）
- **DLC**：8

| 字节 | 信号 | 类型 | 范围/单位 | 字节序 |
|------|------|------|-----------|--------|
| 0~1 | 总电压 | uint16 | mV，大端（高字节在前） | Big-Endian |
| 2~3 | 电流 | int16 | mA，大端 | Big-Endian |
| 4 | SOC | uint8 | 0~100 (%)，即 SOC/100 | — |
| 5 | 状态 | uint8 | 0=待机, 1=充电, 2=放电, 3=故障 | — |
| 6~7 | 预留 | uint8[2] | 填 0x00 | — |

### 2.2 0x200 — 控制命令帧（主控 → BMS）

- **方向**：接收
- **ID**：0x200（标准帧）
- **DLC**：1（只取第 1 字节）

| 字节 | 信号 | 值 | 含义 |
|------|------|-----|------|
| 0 | 命令 | 0x00 | 待机 (Standby) |
| 0 | 命令 | 0x01 | 充电 (Charging) |
| 0 | 命令 | 0x02 | 放电 (Driving) |
| 0 | 命令 | 其他 | 忽略 |

---

## 3. 实施方案

### 3.1 发送侧（0x100）

**方案：新增 `CanTask`，100 ms 周期发送。**

理由：
- `voltageTask` 虽为 100 ms 周期，但职责是 LTC6804 SPI 采样，且 SOC/状态由 `dataTask` 在其后更新，电压任务内发送拿到的是上一拍数据。
- 单独任务职责清晰，便于后续扩展报文。

任务属性：
- 栈大小：256 字节（仅打包 8 字节 + 调用 `CAN_Send`，栈极浅）
- 优先级：`osPriorityLow`（上报不影响控制实时性）
- 周期：`osDelayUntil` 100 ms

发送流程：
1. 读取 `total_voltage`、`Current`、`SOC`、`BMS_State`（这些是 8/16 位变量，Cortex-M3 单次读写原子，可不加锁；若要严格一致可持有 `dataMutex`）
2. 打包到 `uint8_t buf[8]`：
   - `buf[0] = total_voltage >> 8; buf[1] = total_voltage & 0xFF;`
   - `buf[2] = Current >> 8; buf[3] = Current & 0xFF;`
   - `buf[4] = (uint8_t)(SOC / 100U);`（0~100）
   - `buf[5] = BMS_State;`
   - `buf[6] = 0; buf[7] = 0;`
3. 调用 `CAN_Send(0x100, buf, 8)`

> 备选方案：若不想新增任务，可在 `reportTask`（1s）内以计数方式每 10 个 tick 发一次，精度略差但省一个任务栈。**推荐新增任务**，堆充裕。

### 3.2 接收侧（0x200）

**中断收帧 → 队列 → 控制线程处理。**

#### 3.2.1 滤波器修改

将 [can1.c:19-22](file:///d:/BaiduNetdiskDownload/matlab/bms/BMS_LTC6804/Core/PeripheralSrc/can1.c#L19-L22) 的滤波器 ID 从 `0x123` 改为 `0x200`：

```
FilterIdHigh     = (0x200 << 5)
FilterMaskIdHigh = (0x7FF << 5)
```

只放行 0x200。若后续需要同时收多个 ID，可增加滤波器组（STM32F1 有 14 组）。

#### 3.2.2 中断回调

在 `HAL_CAN_RxFifo0MsgPendingCallback` 中：
1. `HAL_CAN_GetRxMessage` 读到 `CAN_RxBuf`
2. 判断 `rxHeader.StdId == 0x200 && rxHeader.DLC >= 1`
3. 取 `CAN_RxBuf[0]` 作为命令字节
4. `osMessageQueuePut(canCmdQueue, &cmd, 0, 0)`（CMSIS-RTOS2 的 `osMessageQueuePut` 可在中断上下文调用）
5. `osThreadFlagsSet(controlTaskHandle, CTRL_FLAG_CAN)`

#### 3.2.3 控制线程集成

新增队列 `canCmdQueue`（深度 4，元素 1 字节），新增线程标志 `CTRL_FLAG_CAN = 0x04`。

`ControlTask` 的 `osThreadFlagsWait` 掩码加入 `CTRL_FLAG_CAN`。收到后：

```c
if ((flags & CTRL_FLAG_CAN) != 0U)
{
    while (osMessageQueueGet(canCmdQueue, &cmd, NULL, 0) == osOK)
    {
        Control_HandleCanCommand(cmd);
    }
}
```

新增 `Control_HandleCanCommand(uint8_t cmd)`，逻辑复用 `Control_HandleLine` 的状态机部分：

| cmd | StateRequest | Store_SaveOnStandby? |
|-----|--------------|----------------------|
| 0x00 | Standby | 是（落盘） |
| 0x01 | Charging | 否 |
| 0x02 | Driving | 否 |
| 其他 | — | 忽略 |

执行：`state_machine_step()` → 若 `BMS_State==Standby` 且请求为充/放电则再 step 一次 → `JudgeBMSState()`。

> 备选方案：不新增队列，在中断里把字节转成 `"C"/"D"/"S"` 文本直接放入现有 `cmdQueue`，复用 `CTRL_FLAG_CMD` 和 `Control_HandleLine`。代码更少，但中断里做字符串处理不够干净。**推荐新增独立队列与标志**。

---

## 4. 详细改动清单

| # | 文件 | 改动内容 |
|---|------|----------|
| 1 | [can1.c](file:///d:/BaiduNetdiskDownload/matlab/bms/BMS_LTC6804/Core/PeripheralSrc/can1.c) `CAN_Filter_Config` | 滤波器 ID 0x123 → 0x200 |
| 2 | can1.c `HAL_CAN_RxFifo0MsgPendingCallback` | 判断 ID 0x200，取首字节入 `canCmdQueue`，置 `CTRL_FLAG_CAN` 标志 |
| 3 | can1.h | 声明 `extern osMessageQueueId_t canCmdQueue`（或在 freertos.c 内声明） |
| 4 | [freertos.c](file:///d:/BaiduNetdiskDownload/matlab/bms/BMS_LTC6804/Core/Src/freertos.c) 变量区 | 新增 `osMessageQueueId_t canCmdQueueHandle`；新增 `#define CTRL_FLAG_CAN 0x04U` |
| 5 | freertos.c `MX_FREERTOS_Init` | `canCmdQueueHandle = osMessageQueueNew(4, 1, NULL)`；创建 `CanTask` 线程 |
| 6 | freertos.c 新增 `CanTask` | 100 ms 周期，打包并 `CAN_Send(0x100, ...)` |
| 7 | freertos.c `ControlTask` | `osThreadFlagsWait` 掩码加入 `CTRL_FLAG_CAN`；新增该标志的处理分支，调用 `Control_HandleCanCommand` |
| 8 | freertos.c 新增 `Control_HandleCanCommand` | 字节 → `StateRequest` 映射 + 状态机步进 + `JudgeBMSState` + 待机落盘 |

> 不改动 `can.c`（CubeMX 生成的外设初始化），波特率/引脚/中断优先级保持不变。

---

## 5. 验证方法

1. **编译**：Keil 编译无错误、无新增警告。
2. **发送验证**：CAN 分析仪连接 PA11/PA12，观察是否每 100 ms 收到一帧 ID=0x100，DLC=8，字节随电压/电流/SOC/状态变化。
3. **接收验证**：分析仪发送 ID=0x200, DLC=1, 数据 0x01，观察 MOS 状态变为充电（CHG 开）；发 0x02 变放电；发 0x00 变待机并触发 Flash 落盘。
4. **队列/标志验证**：在 `Control_HandleCanCommand` 入口设断点，确认每次 0x200 帧都能触发且只处理一次。
5. **栈余量**：`CanTask` 栈 256 字节，跑 1 分钟后 `osThreadGetStackSpace` 应 ≥ 128 字节（`StackMarginOk` 当前未包含 CanTask，可后续加入）。
6. **异常帧**：发送 0x200 但数据为 0x05，确认被忽略，状态不变。

---

## 6. 注意事项

- **中断里调用 `osMessageQueuePut`**：CMSIS-RTOS2 封装内部会判断中断上下文并调用 `xQueueSendFromISR`，无需手动区分。但 `osThreadFlagsSet` 在 ISR 中同样安全。
- **`controlTaskHandle` 必须非 NULL**：`CAN_Init()` 在调度器启动前调用，但回调只在运行时触发，此时 `controlTaskHandle` 已创建，安全。
- **大端字节序**：0x100 的多字节字段统一高字节在前，符合车载 CAN 惯例。
- **SOC 缩放**：`SOC` 全局为 0~10000（0.01% 分辨率），上报字节 4 只取 0~100，丢失小数精度；如需更高精度可改为放 2 字节。
