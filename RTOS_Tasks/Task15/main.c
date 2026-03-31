/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Priority Inversion Demo
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

// Shared resource structure
typedef struct {
    uint32_t data;
    uint32_t owner_task_id;
    uint32_t lock_count;
} SharedResource_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define LOW_PRIORITY_TASK_ID     1
#define MEDIUM_PRIORITY_TASK_ID  2
#define HIGH_PRIORITY_TASK_ID    3

#define TASK_DELAY_MS            500
#define RESOURCE_HOLD_TIME_MS    5000  // Low task holds resource for 5 seconds

/* USER CODE END PD */

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

// Mutex for shared resource (simulates a resource lock)
osMutexId_t resourceMutex;

// Task handles
osThreadId_t lowPriorityTaskHandle;
osThreadId_t mediumPriorityTaskHandle;
osThreadId_t highPriorityTaskHandle;

// Shared resource
SharedResource_t shared_resource = {0, 0, 0};

// For timing measurements
volatile uint32_t high_priority_start_time = 0;
volatile uint32_t high_priority_end_time = 0;
volatile uint32_t medium_priority_runs = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C1_Init(void);
void StartDefaultTask(void *argument);

/* USER CODE BEGIN PFP */
void LowPriorityTask(void *argument);
void MediumPriorityTask(void *argument);
void HighPriorityTask(void *argument);
void SafePrintf(const char *format, ...);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// Safe printf with mutex protection for UART
void SafePrintf(const char *format, ...)
{
    va_list args;
    char buffer[256];

    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    // Direct UART transmit (no mutex to avoid deadlock)
    HAL_UART_Transmit(&huart2, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);
}

int _write(int file, char *ptr, int len)
{
    HAL_UART_Transmit(&huart2, (uint8_t*)ptr, len, HAL_MAX_DELAY);
    return len;
}

// LOW PRIORITY TASK - Holds the shared resource for a long time
void LowPriorityTask(void *argument)
{
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(10000);  // Run every 10 seconds

    SafePrintf("[LOW] Task started (Priority: LOW)\r\n");
    SafePrintf("[LOW] Will hold resource for %d ms when acquired\r\n\n", RESOURCE_HOLD_TIME_MS);

    for(;;)
    {
        SafePrintf("\r\n[LOW] Attempting to acquire resource...\r\n");

        // Acquire the mutex (simulates locking a resource)
        if(osMutexAcquire(resourceMutex, 1000) == osOK) {
            shared_resource.owner_task_id = LOW_PRIORITY_TASK_ID;
            shared_resource.lock_count++;
            shared_resource.data += 10;

            SafePrintf("[LOW] *** RESOURCE ACQUIRED *** (Lock #%lu)\r\n", shared_resource.lock_count);
            SafePrintf("[LOW] Holding resource for %d ms...\r\n", RESOURCE_HOLD_TIME_MS);
            SafePrintf("[LOW] (Medium & High priority tasks are now blocked!)\r\n\n");

            // Hold the resource for a long time (SIMULATES PRIORITY INVERSION)
            osDelay(RESOURCE_HOLD_TIME_MS);

            // Release the resource
            osMutexRelease(resourceMutex);
            SafePrintf("\r\n[LOW] *** RESOURCE RELEASED ***\r\n");
            SafePrintf("[LOW] Resource value: %lu\r\n\n", shared_resource.data);
        } else {
            SafePrintf("[LOW] Failed to acquire resource!\r\n");
        }

        vTaskDelayUntil(&last_wake_time, period);
    }
}

// MEDIUM PRIORITY TASK - Does unrelated work but preempts low priority task
void MediumPriorityTask(void *argument)
{
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(1000);  // Run every 1 second

    SafePrintf("[MEDIUM] Task started (Priority: MEDIUM)\r\n");
    SafePrintf("[MEDIUM] This task does NOT need the shared resource\r\n");
    SafePrintf("[MEDIUM] But it will PREEMPT the low priority task!\r\n\n");

    for(;;)
    {
        medium_priority_runs++;

        SafePrintf("[MEDIUM] Running... (Execution #%lu)\r\n", medium_priority_runs);
        SafePrintf("[MEDIUM] Doing medium priority work...\r\n");

        // Simulate work
        osDelay(200);

        SafePrintf("[MEDIUM] Work complete.\r\n\n");

        vTaskDelayUntil(&last_wake_time, period);
    }
}

// HIGH PRIORITY TASK - Needs the shared resource (will be blocked)
void HighPriorityTask(void *argument)
{
    uint32_t wait_time = 0;
    uint32_t blocked_count = 0;

    SafePrintf("[HIGH] Task started (Priority: HIGH)\r\n");
    SafePrintf("[HIGH] This task NEEDS the shared resource!\r\n");
    SafePrintf("[HIGH] Will be blocked if resource is held by lower priority task\r\n\n");

    for(;;)
    {
        // Wait for the resource to become available
        SafePrintf("\r\n[HIGH] Attempting to acquire resource...\r\n");

        high_priority_start_time = HAL_GetTick();

        // This will BLOCK if resource is held by Low task
        if(osMutexAcquire(resourceMutex, 10000) == osOK) {
            high_priority_end_time = HAL_GetTick();
            wait_time = high_priority_end_time - high_priority_start_time;
            blocked_count++;

            SafePrintf("[HIGH] *** RESOURCE ACQUIRED *** (Attempt #%lu)\r\n", blocked_count);
            SafePrintf("[HIGH] Was BLOCKED for %lu ms! (Priority Inversion!)\r\n", wait_time);

            // Use the resource
            shared_resource.owner_task_id = HIGH_PRIORITY_TASK_ID;
            shared_resource.data += 100;

            SafePrintf("[HIGH] Using resource... New value: %lu\r\n", shared_resource.data);

            // Hold briefly
            osDelay(100);

            // Release
            osMutexRelease(resourceMutex);
            SafePrintf("[HIGH] Resource released\r\n\n");
        } else {
            SafePrintf("[HIGH] Failed to acquire resource (timeout)!\r\n");
        }

        // Wait before trying again
        osDelay(3000);
    }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();
    MX_I2C1_Init();

    printf("\r\n");
    printf("========================================\r\n");
    printf("   PRIORITY INVERSION DEMO\r\n");
    printf("========================================\r\n");
    printf("\r\n");
    printf("SCENARIO:\r\n");
    printf("----------\r\n");
    printf("1. Low task acquires resource (holds for 5 sec)\r\n");
    printf("2. High task needs same resource -> gets BLOCKED\r\n");
    printf("3. Medium task preempts Low task (doesn't need resource)\r\n");
    printf("4. High task stays blocked -> PRIORITY INVERSION!\r\n");
    printf("\r\n");
    printf("Task Priorities:\r\n");
    printf("  [HIGH]   - Priority 3 (needs resource)\r\n");
    printf("  [MEDIUM] - Priority 2 (CPU-bound, no resource)\r\n");
    printf("  [LOW]    - Priority 1 (holds resource)\r\n");
    printf("========================================\r\n\n");

    osKernelInitialize();

    // Create mutex for shared resource
    resourceMutex = osMutexNew(NULL);

    // Create tasks with DIFFERENT priorities
    const osThreadAttr_t low_attr = {
        .name = "LowPriority",
        .stack_size = 256 * 4,
        .priority = osPriorityLow      // Priority 1
    };
    lowPriorityTaskHandle = osThreadNew(LowPriorityTask, NULL, &low_attr);

    const osThreadAttr_t medium_attr = {
        .name = "MediumPriority",
        .stack_size = 256 * 4,
        .priority = osPriorityNormal    // Priority 2
    };
    mediumPriorityTaskHandle = osThreadNew(MediumPriorityTask, NULL, &medium_attr);

    const osThreadAttr_t high_attr = {
        .name = "HighPriority",
        .stack_size = 256 * 4,
        .priority = osPriorityHigh       // Priority 3
    };
    highPriorityTaskHandle = osThreadNew(HighPriorityTask, NULL, &high_attr);

    // Start scheduler
    osKernelStart();

    while(1)
    {
    }
}

// Rest of the generated functions (SystemClock_Config, MX_GPIO_Init, MX_USART2_UART_Init, etc.)
// Keep these as they are from your original code...

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
}
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
