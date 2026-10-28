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
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
osMessageQueueId_t currentQueueHandle;
osMessageQueueId_t voltageQueueHandle;
osMutexId_t spiMutexHandle;
/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
static void CurrentTask(void *argument);
static void VoltageTask(void *argument);
/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);

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
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  {
    const osThreadAttr_t currentTask_attributes = {
      .name = "current",
      .stack_size = 512,
      .priority = osPriorityHigh,
    };
    osThreadNew(CurrentTask, NULL, &currentTask_attributes);
  }
  {
    const osThreadAttr_t voltageTask_attributes = {
      .name = "voltage",
      .stack_size = 768,
      .priority = osPriorityNormal,
    };
    osThreadNew(VoltageTask, NULL, &voltageTask_attributes);
  }
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartDefaultTask */
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

        /* 4. 采一个点。adc_mutex 等温度线程再加 */
        Current_SamplePush();

        /* 5. 存活标志等 1 s 线程喂狗时再置 */

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

        /* 4. 发 ADCV，随即放开 SPI 锁 */
        osMutexAcquire(spiMutexHandle, osWaitForever);
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

        /* 8. 存活标志等 1 s 线程喂狗时再置 */
        /* 9. 回到步骤 2 */
    }
}
/* USER CODE END Application */

