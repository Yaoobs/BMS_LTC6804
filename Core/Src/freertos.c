/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "sample.h"
#include "LTC6804.h"
#include "os.h"
#include "protect.h"
#include "Ah_Integrator_Estimator.h"
#include "state_machine.h"
#include "balance.h"
#include "store.h"
#include "usart1.h"
#include "iwdg.h"
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct
{
    int16_t current;
    uint8_t count;
    uint32_t tick;
} CurrentMsg;

typedef struct
{
    uint16_t cell_mv[12];
    uint16_t total_mv;
    uint8_t pec_mask;
    uint32_t tick;
} VoltageMsg;

typedef struct
{
    uint16_t soc;
    uint16_t total_mv;
    int16_t current;
    uint8_t ov;
    uint8_t uv;
    uint8_t oc;
} ResultMsg;

typedef struct
{
    uint16_t soc;
    uint16_t total_mv;
    int16_t current;
} DataSnapshot;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define CMD_TEXT_LEN       32U
#define CTRL_FLAG_CMD      0x01U
#define CTRL_FLAG_RESULT   0x02U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
osMessageQueueId_t currentQueueHandle;
osMessageQueueId_t voltageQueueHandle;
osMessageQueueId_t resultQueueHandle;
osMessageQueueId_t cmdQueueHandle;
osMessageQueueId_t txQueueHandle;
osMutexId_t spiMutexHandle;
osMutexId_t dataMutexHandle;
osMutexId_t adcMutexHandle;
DataSnapshot dataSnapshot;
static volatile uint8_t currentAlive;
static volatile uint8_t voltageAlive;
static volatile uint8_t controlAlive;
/* 四组读完后再过这么久，电流点才入窗。0 表示还没发过电压 SPI */
#define SPI_SETTLE_MS 20U
static volatile uint32_t spiSampleAfter;
static osThreadId_t currentTaskHandle;
static osThreadId_t voltageTaskHandle;
static osThreadId_t dataTaskHandle;
/* USER CODE END Variables */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
static void CurrentTask(void *argument);
static void VoltageTask(void *argument);
static void DataTask(void *argument);
static void ControlTask(void *argument);
static void ReportTask(void *argument);
osThreadId_t controlTaskHandle;
/* USER CODE END FunctionPrototypes */

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  spiMutexHandle = osMutexNew(NULL);
  dataMutexHandle = osMutexNew(NULL);
  adcMutexHandle = osMutexNew(NULL);
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  currentQueueHandle = osMessageQueueNew(1, sizeof(CurrentMsg), NULL);
  voltageQueueHandle = osMessageQueueNew(1, sizeof(VoltageMsg), NULL);
  resultQueueHandle = osMessageQueueNew(1, sizeof(ResultMsg), NULL);
  cmdQueueHandle = osMessageQueueNew(4, CMD_TEXT_LEN, NULL);
  txQueueHandle = osMessageQueueNew(4, sizeof(TxFrame), NULL);
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* USER CODE BEGIN RTOS_THREADS */
  {
    const osThreadAttr_t currentTask_attributes = {
      .name = "current",
      .stack_size = 1024,
      .priority = osPriorityHigh,
    };
    currentTaskHandle = osThreadNew(CurrentTask, NULL, &currentTask_attributes);
  }
  {
    const osThreadAttr_t voltageTask_attributes = {
      .name = "voltage",
      .stack_size = 768,
      .priority = osPriorityNormal,
    };
    voltageTaskHandle = osThreadNew(VoltageTask, NULL, &voltageTask_attributes);
  }
  {
    const osThreadAttr_t dataTask_attributes = {
      .name = "data",
      .stack_size = 768,
      .priority = osPriorityBelowNormal,
    };
    dataTaskHandle = osThreadNew(DataTask, NULL, &dataTask_attributes);
  }
  {
    const osThreadAttr_t controlTask_attributes = {
      .name = "ctrl",
      .stack_size = 1024,
      .priority = osPriorityAboveNormal,
    };
    controlTaskHandle = osThreadNew(ControlTask, NULL, &controlTask_attributes);
  }
  {
    const osThreadAttr_t reportTask_attributes = {
      .name = "report",
      .stack_size = 1024,
      .priority = osPriorityLow,
    };
    osThreadNew(ReportTask, NULL, &reportTask_attributes);
  }
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
static void CurrentTask(void *argument)
{
    /* 1. 起始节拍，以及本窗口起点 */
    uint32_t tick = osKernelGetTickCount();
    uint32_t window = tick;
    CurrentMsg msg;
    CurrentMsg dropped;

    (void)argument;
    for (;;)
    {
        uint32_t now = osKernelGetTickCount();

        /* 2. 落后超过 1 ms 只对齐，不补采 */
        if ((int32_t)(now - tick) > 1)
        {
            tick = now;
        }
        /* 3. 下次在 1 ms 边界醒来 */
        tick += 1U;
        osDelayUntil(tick);

        /* 4. 先避开正在进行的 SPI。这次电压 SPI 结束前，以及结束后 20 ms 内，不入窗。1 ms 格子不挪 */
        osMutexAcquire(spiMutexHandle, osWaitForever);
        osMutexAcquire(adcMutexHandle, osWaitForever);
        if ((int32_t)(osKernelGetTickCount() - spiSampleAfter) >= 0)
        {
            Current_SamplePush();
        }
        osMutexRelease(adcMutexHandle);
        osMutexRelease(spiMutexHandle);

        /* 5. 置存活标志，报告线程喂狗后清掉 */
        currentAlive = 1U;

        /* 6. 未满 100 ms 继续采样 */
        now = osKernelGetTickCount();
        if ((int32_t)(now - window) < 100)
        {
            continue;
        }
        /* 7. 这一窗没有点，不入队 */
        if (Current_WindowCount() > 0U)
        {
            /* 8. 平均后放入队列，只留最新一条 */
            Current_CalcFromAverage();
            msg.current = Current;
            msg.count = Current_WindowCount();
            msg.tick = now;
            if (osMessageQueuePut(currentQueueHandle, &msg, 0, 0) != osOK)
            {
                (void)osMessageQueueGet(currentQueueHandle, &dropped, NULL, 0);
                (void)osMessageQueuePut(currentQueueHandle, &msg, 0, 0);
            }
            /* 9. 清窗口，下一窗只含新点 */
            Current_WindowClear();
        }
        window = now;
        /* 10. 回到步骤 2 */
    }
}

static void VoltageTask(void *argument)
{
    /* 1. 起始节拍。原始码跨周期保留，PEC 失败时沿用上一拍 */
    uint32_t tick = osKernelGetTickCount();
    static uint16_t cell_codes[12];
    VoltageMsg msg;
    VoltageMsg dropped;
    uint8_t group;
    uint8_t i;

    (void)argument;
    for (;;)
    {
        uint32_t now = osKernelGetTickCount();

        /* 2. 落后超过 100 ms 只对齐，不补发 ADCV */
        if ((int32_t)(now - tick) > 100)
        {
            tick = now;
        }
        /* 3. 下次在 100 ms 边界醒来 */
        tick += 100U;
        osDelayUntil(tick);

        /* 4. 发 ADCV。这次 SPI 没结束前，电流不入窗 */
        osMutexAcquire(spiMutexHandle, osWaitForever);
        spiSampleAfter = osKernelGetTickCount() + 100U;
        LTC6804_adcv();
        osMutexRelease(spiMutexHandle);

        /* 5. 等 4 ms，这段不持有锁 */
        osDelay(4);

        /* 6. 四组各读一次。失败的组保持旧码，并记下组号 */
        msg.pec_mask = 0U;
        for (group = 1U; group <= 4U; group++)
        {
            osMutexAcquire(spiMutexHandle, osWaitForever);
            if (LTC6804_rdcv_group(group, cell_codes) != 0)
            {
                msg.pec_mask |= (uint8_t)(1U << (group - 1U));
            }
            /* 四组读完：从这一拍再过 20 ms 才入窗 */
            if (group == 4U)
            {
                spiSampleAfter = osKernelGetTickCount() + SPI_SETTLE_MS;
            }
            osMutexRelease(spiMutexHandle);
        }

        /* 7. 换成毫伏后入队，只留最新一帧 */
        msg.total_mv = 0U;
        for (i = 0U; i < 12U; i++)
        {
            msg.cell_mv[i] = (uint16_t)(cell_codes[i] / 10U);
            msg.total_mv += msg.cell_mv[i];
        }
        msg.tick = osKernelGetTickCount();
        if (osMessageQueuePut(voltageQueueHandle, &msg, 0, 0) != osOK)
        {
            (void)osMessageQueueGet(voltageQueueHandle, &dropped, NULL, 0);
            (void)osMessageQueuePut(voltageQueueHandle, &msg, 0, 0);
        }

        /* 8. 置存活标志，报告线程喂狗后清掉 */
        voltageAlive = 1U;
        /* 9. 回到步骤 2 */
    }
}

/* 数据处理线程 */
static void DataTask(void *argument)
{
    VoltageMsg voltage;
    CurrentMsg current_msg;
    ResultMsg result;
    ResultMsg dropped;
    static int16_t last_current;
    uint8_t i;

    (void)argument;
    for (;;)
    {
        /* 1. 等一帧电压 */
        (void)osMessageQueueGet(voltageQueueHandle, &voltage, NULL, osWaitForever);

        /* 2. 取最新平均电流。没有就沿用上一拍 */
        if (osMessageQueueGet(currentQueueHandle, &current_msg, NULL, 0) == osOK)
        {
            last_current = current_msg.current;
        }

        /* 3. 写入现有全局量，供原来的估算和保护函数读取 */
        for (i = 0U; i < 12U; i++)
        {
            cell_voltage[i] = voltage.cell_mv[i];
        }
        total_voltage = voltage.total_mv;
        Current = last_current;

        /* 4. 总电压为 0 时函数直接返回，SOC 保持原值 */
        SOC_Estimator();

        /* 5. 过流看这一拍选定的电流 */
        OC_FLAG = (Current >= 2000 || Current <= -2000);

        /* 6. 过压、欠压仍由保护模型写 */
        protect_step();

        /* 7. 结果入队，只留最新一条 */
        result.soc = SOC;
        result.total_mv = total_voltage;
        result.current = Current;
        result.ov = OV_FLAG;
        result.uv = UV_FLAG;
        result.oc = OC_FLAG;
        if (osMessageQueuePut(resultQueueHandle, &result, 0, 0) != osOK)
        {
            (void)osMessageQueueGet(resultQueueHandle, &dropped, NULL, 0);
            (void)osMessageQueuePut(resultQueueHandle, &result, 0, 0);
        }

        /* 8. 抄一份给报告线程。sprintf 不在这把锁里 */
        osMutexAcquire(dataMutexHandle, osWaitForever);
        dataSnapshot.soc = SOC;
        dataSnapshot.total_mv = total_voltage;
        dataSnapshot.current = Current;
        osMutexRelease(dataMutexHandle);

        /* 9. 通知控制线程有一帧结果 */
        if (controlTaskHandle != NULL)
        {
            (void)osThreadFlagsSet(controlTaskHandle, CTRL_FLAG_RESULT);
        }
        /* 10. 回到步骤 1 */
    }
}

void Control_PostLine(const char *line)
{
    char msg[CMD_TEXT_LEN];
    uint8_t n;

    if (line == NULL || cmdQueueHandle == NULL || controlTaskHandle == NULL)
    {
        return;
    }
    for (n = 0U; n < (CMD_TEXT_LEN - 1U) && line[n] != '\0'; n++)
    {
        msg[n] = line[n];
    }
    if (line[n] != '\0')
    {
        return;
    }
    msg[n] = '\0';
    if (osMessageQueuePut(cmdQueueHandle, msg, 0, 0) != osOK)
    {
        return;
    }
    if (osKernelGetState() == osKernelRunning)
    {
        (void)osThreadFlagsSet(controlTaskHandle, CTRL_FLAG_CMD);
    }
}

/* 4. 状态和 MOS 确定后写均衡。SPI 锁只包住写配置 */
static void Control_WriteBalance(void)
{
    if (BMS_State == BMS_Standby)
    {
        balance_step();
        osMutexAcquire(spiMutexHandle, osWaitForever);
        send_balance_commend();
        osMutexRelease(spiMutexHandle);
        balance_on_flag = 1U;
    }
    else if (balance_on_flag == 1U)
    {
        osMutexAcquire(spiMutexHandle, osWaitForever);
        exit_balance_commend();
        osMutexRelease(spiMutexHandle);
        balance_on_flag = 0U;
    }
}

static void Control_HandleLine(const char *line)
{
    if (strcmp(line, "Clear") == 0)
    {
        (void)Store_Clear();
        return;
    }
    if (strcmp(line, "D") == 0)
    {
        StateRequest = Driving;
    }
    else if (strcmp(line, "C") == 0)
    {
        StateRequest = Charging;
    }
    else if (strcmp(line, "S") == 0)
    {
        StateRequest = Standby;
    }
    else
    {
        return;
    }

    /* 2. 落到待机且请求已是充电或放电时，同一次再走一步 */
    state_machine_step();
    if (BMS_State == BMS_Standby &&
        (StateRequest == Charging || StateRequest == Driving))
    {
        state_machine_step();
    }
    JudgeBMSState();
    if (strcmp(line, "S") == 0)
    {
        Store_SaveOnStandby();
    }
//    Control_WriteBalance();
}

static void Control_HandleResult(const ResultMsg *result)
{
    /* 3. 结果只步进一次 */
    CriticalFault = (boolean_T)(result->ov || result->uv || result->oc);
    state_machine_step();
    JudgeBMSState();
//    Control_WriteBalance();
}

/* 控制线程 */
static void ControlTask(void *argument)
{
    uint32_t flags;
    char line[CMD_TEXT_LEN];
    ResultMsg result;

    (void)argument;
    while (osMessageQueueGet(cmdQueueHandle, line, NULL, 0) == osOK)
    {
        Control_HandleLine(line);
    }
    for (;;)
    {
        /* 1. 命令或结果标志，有一个就醒 */
        flags = osThreadFlagsWait(CTRL_FLAG_CMD | CTRL_FLAG_RESULT,
                                  osFlagsWaitAny, osWaitForever);
        if ((int32_t)flags < 0)
        {
            continue;
        }

        /* 2. 先把已经排着的命令处理完 */
        if ((flags & CTRL_FLAG_CMD) != 0U)
        {
            while (osMessageQueueGet(cmdQueueHandle, line, NULL, 0) == osOK)
            {
                Control_HandleLine(line);
            }
        }

        /* 3. 再处理一帧保护结果 */
        if ((flags & CTRL_FLAG_RESULT) != 0U &&
            osMessageQueueGet(resultQueueHandle, &result, NULL, 0) == osOK)
        {
            Control_HandleResult(&result);
        }

        /* 5. 置存活标志，报告线程喂狗后清掉 */
        controlAlive = 1U;
        /* 6. 回到步骤 1 */
    }
}

#define STACK_MARGIN_BYTES 128U

/* 各业务线程栈的历史最小余量都不少于 128 字节才算通过 */
static uint8_t StackMarginOk(void)
{
    osThreadId_t thread[5];
    uint8_t i;

    thread[0] = currentTaskHandle;
    thread[1] = voltageTaskHandle;
    thread[2] = dataTaskHandle;
    thread[3] = controlTaskHandle;
    thread[4] = osThreadGetId();
    for (i = 0U; i < 5U; i++)
    {
        if (thread[i] == NULL || osThreadGetStackSpace(thread[i]) < STACK_MARGIN_BYTES)
        {
            return 0U;
        }
    }
    return 1U;
}

/* 报告线程 */
static void ReportTask(void *argument)
{
    /* 1. 起始节拍 */
    uint32_t tick = osKernelGetTickCount();
    DataSnapshot snap;

    (void)argument;
    for (;;)
    {
        uint32_t now = osKernelGetTickCount();

        /* 2. 落后超过 1 s 只对齐，不补发 */
        if ((int32_t)(now - tick) > 1000)
        {
            tick = now;
        }
        /* 3. 下次在 1 s 边界醒来 */
        tick += 1000U;
        osDelayUntil(tick);

        /* 4. 采温度。先避开 SPI，再和电流线程共用 adc 锁 */
        osMutexAcquire(spiMutexHandle, osWaitForever);
        osMutexAcquire(adcMutexHandle, osWaitForever);
        Temp_SamplePush();
        osMutexRelease(adcMutexHandle);
        osMutexRelease(spiMutexHandle);

        /* 5. 翻转 LED */
        LEDXToggle();

        /* 6. 锁内只复制快照。拼串和读 MOS 在锁外，放进发送队列 */
        osMutexAcquire(dataMutexHandle, osWaitForever);
        snap = dataSnapshot;
        osMutexRelease(dataMutexHandle);
        USART1_SendTxStr(snap.soc, snap.total_mv, snap.current);

        /* 7. 三条存活标志都在，且五条业务线程栈还剩至少 128 字节，才喂狗并清掉 */
        if (currentAlive != 0U && voltageAlive != 0U && controlAlive != 0U &&
            StackMarginOk() != 0U)
        {
            HAL_IWDG_Refresh(&hiwdg);
            currentAlive = 0U;
            voltageAlive = 0U;
            controlAlive = 0U;
        }
        /* 8. 回到步骤 2 */
    }
}
/* USER CODE END Application */

