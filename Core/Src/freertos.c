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
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct
{
    int16_t current;
    uint8_t count;
    uint32_t tick;
} CurrentMsg;
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
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  currentQueueHandle = osMessageQueueNew(1, sizeof(CurrentMsg), NULL);
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
/* USER CODE END Application */

