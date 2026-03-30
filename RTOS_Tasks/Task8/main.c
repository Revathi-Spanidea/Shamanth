/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
#include "main.h"
#include "cmsis_os.h"
#include <stdio.h>
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart2;

/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* USER CODE BEGIN PV */
osThreadId_t monitorTaskHandle;
osThreadId_t buttonTaskHandle;
osThreadId_t dynamicTaskHandles[5];
int activeTaskCount = 0;
int nextTaskId = 1;

// Semaphore for protecting the task array (CMSIS-RTOS V2)
osSemaphoreId_t taskArraySemaphore;

// Queue for task creation requests
osMessageQueueId_t taskRequestQueue;

// Structure for task creation requests
typedef struct {
    int taskId;
    int duration;  // How many iterations the task should run
} TaskRequest_t;

osThreadId_t buttonTaskHandle;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C1_Init(void);
void StartDefaultTask(void *argument);

/* USER CODE BEGIN PFP */
void MonitorTask(void *argument);
void DynamicTask(void *argument);
void ButtonTask(void *argument);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

int _write(int file, char *ptr, int len)
{
  HAL_UART_Transmit(&huart2, (uint8_t*)ptr, len, HAL_MAX_DELAY);
  return len;
}
void DynamicTask(void *argument)
{
    // Get the task info from the argument
    TaskRequest_t *info = (TaskRequest_t *)argument;
    int taskId = info->taskId;
    int duration = info->duration;

    printf("\r\n========================================\r\n");
    printf("TASK CREATED!\r\n");
    printf("Task ID: %d\r\n", taskId);
    printf("Will run for: %d iterations\r\n", duration);
    printf("Active tasks: %d\r\n", activeTaskCount + 1);
    printf("Free heap: %d bytes\r\n", xPortGetFreeHeapSize());
    printf("========================================\r\n\r\n");

    // Do the work (simple countdown)
    for(int i = duration; i > 0; i--) {
        printf("Task %d: Working... %d iterations remaining\r\n", taskId, i);

        // Toggle LED to show activity
        HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
        osDelay(500);  // Wait 500ms between iterations
    }

    printf("\r\nTask %d: Work completed!\r\n", taskId);

    // Remove this task from the tracking array
    // Use osSemaphoreAcquire instead of xSemaphoreTake
    if(osSemaphoreAcquire(taskArraySemaphore, osWaitForever) == osOK) {
        for(int i = 0; i < 5; i++) {
            if(dynamicTaskHandles[i] == osThreadGetId()) {
                dynamicTaskHandles[i] = NULL;
                activeTaskCount--;
                printf("Task %d removed. Active tasks: %d\r\n", taskId, activeTaskCount);
                break;
            }
        }
        osSemaphoreRelease(taskArraySemaphore);  // Release the semaphore
    }

    // Delete itself
    printf("Task %d: Deleting itself...\r\n", taskId);
    osThreadTerminate(osThreadGetId());

    // Should never reach here
    while(1);
}

void MonitorTask(void *argument)
{
    TaskRequest_t request;
    osStatus_t status;
    int totalTasksCreated = 0;

    printf("\r\n=== MONITOR TASK STARTED ===\r\n");
    printf("Monitoring system for task creation requests...\r\n");
    printf("Free heap at start: %d bytes\r\n\r\n", xPortGetFreeHeapSize());

    for(;;)
    {
        // Wait for task creation request (with 2 second timeout)
        status = osMessageQueueGet(taskRequestQueue, &request, NULL, 2000);

        if(status == osOK) {
            totalTasksCreated++;

            // Create a name for the task
            char taskName[20];
            sprintf(taskName, "Task_%d", request.taskId);

            // Define task attributes
            const osThreadAttr_t task_attr = {
                .name = taskName,
                .stack_size = 256 * 4,
                .priority = osPriorityNormal
            };

            // Allocate memory for task info
            TaskRequest_t *taskInfo = malloc(sizeof(TaskRequest_t));
            if(taskInfo != NULL) {
                memcpy(taskInfo, &request, sizeof(TaskRequest_t));

                // Create the dynamic task
                osThreadId_t newTask = osThreadNew(DynamicTask, taskInfo, &task_attr);

                if(newTask != NULL) {
                    // Store the task handle - use osSemaphoreAcquire
                    if(osSemaphoreAcquire(taskArraySemaphore, osWaitForever) == osOK) {
                        for(int i = 0; i < 5; i++) {
                            if(dynamicTaskHandles[i] == NULL) {
                                dynamicTaskHandles[i] = newTask;
                                activeTaskCount++;
                                break;
                            }
                        }
                        osSemaphoreRelease(taskArraySemaphore);
                    }

                    printf("✓ Created task: %s (ID: %d)\r\n", taskName, request.taskId);
                    printf("  Total tasks created so far: %d\r\n", totalTasksCreated);
                } else {
                    printf("✗ Failed to create task! Free heap: %d bytes\r\n",
                           xPortGetFreeHeapSize());
                    free(taskInfo);
                }
            } else {
                printf("✗ Failed to allocate memory!\r\n");
            }
        }

        // Print system status every 10 seconds
        static int counter = 0;
        if(++counter >= 5) {  // 5 * 2 seconds = 10 seconds
            counter = 0;
            printf("\r\n--- SYSTEM STATUS ---\r\n");
            printf("Active tasks: %d\r\n", activeTaskCount);
            printf("Free heap: %d bytes\r\n", xPortGetFreeHeapSize());

            if(activeTaskCount > 0) {
                printf("Active task slots: ");
                for(int i = 0; i < 5; i++) {
                    if(dynamicTaskHandles[i] != NULL) {
                        printf("[%d] ", i+1);
                    }
                }
                printf("\r\n");
            }
            printf("---------------------\r\n\r\n");
        }
    }
}

void ButtonTask(void *argument)
{
    TaskRequest_t request;
    int taskCounter = 1;

    printf("BUTTON TASK: Press user button to create a new task!\r\n");
    printf("Each task will run for 5 iterations (2.5 seconds)\r\n\r\n");

    for(;;)
    {
        // Wait for button notification
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // Create a new task request
        request.taskId = taskCounter++;
        request.duration = 5;  // Run for 5 iterations

        // Send request to monitor task
        if(osMessageQueuePut(taskRequestQueue, &request, 0, 0) == osOK) {
            printf("\r\n>>> BUTTON PRESSED! Requesting Task %d <<<\r\n", request.taskId);

            // Quick LED flash to show button press
            HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);
            osDelay(100);
            HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
        } else {
            printf("\r\n!!! Queue full! Cannot create more tasks !!!\r\n");
        }
    }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  // Create semaphore to protect task array
  taskArraySemaphore = osSemaphoreNew(1, 1, NULL);
  if(taskArraySemaphore == NULL) {
      Error_Handler();
  }
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* USER CODE BEGIN RTOS_QUEUES */
  // Create queue for task creation requests
  taskRequestQueue = osMessageQueueNew(10, sizeof(TaskRequest_t), NULL);
  if(taskRequestQueue == NULL) {
      Error_Handler();
  }
  /* USER CODE END RTOS_QUEUES */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  // Initialize task handle array
  for(int i = 0; i < 5; i++) {
      dynamicTaskHandles[i] = NULL;
  }

  // Create button task
  const osThreadAttr_t buttonTask_attr = {
      .name = "ButtonTask",
      .stack_size = 256 * 4,
      .priority = osPriorityHigh  // High priority for responsive button
  };
  buttonTaskHandle = osThreadNew(ButtonTask, NULL, &buttonTask_attr);

  // Create monitor task
  const osThreadAttr_t monitorTask_attr = {
      .name = "MonitorTask",
      .stack_size = 512 * 4,
      .priority = osPriorityNormal
  };
  osThreadNew(MonitorTask, NULL, &monitorTask_attr);

  // Keep default task
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

  /* Start scheduler */
  osKernelStart();
  /* We should never get here as control is now taken by the scheduler */
  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
/* USER CODE BEGIN 4 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if(GPIO_Pin == B1_Pin)  // User button
    {
        // Send notification to button task
        if(buttonTaskHandle != NULL)
        {
            vTaskNotifyGiveFromISR(buttonTaskHandle, &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    }
}
/* USER CODE END 4 */

/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN 5 */
    printf("\r\n=== SYSTEM READY ===\r\n");
    printf("Press the user button to create dynamic tasks!\r\n");
    printf("Each new task will run for 5 iterations (2.5 seconds)\r\n");
    printf("Maximum 5 tasks can run simultaneously\r\n");
    printf("Tasks automatically delete themselves when done\r\n\r\n");

    for(;;)
    {
        osDelay(1000);
    }
  /* USER CODE END 5 */
}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM2 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM2) {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
