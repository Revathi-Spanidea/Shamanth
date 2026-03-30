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
#define LIS2DU12_ADDR   (0x19 << 1)
#define WHO_AM_I_REG    0x0F
#define CTRL1_REG       0x10
#define CTRL3_REG       0x12
#define CTRL4_REG       0x13
#define CTRL5_REG       0x14
#define OUTX_L_REG      0x28

#define LIS2DU12_WHO_AM_I_VAL  0x47   // Expected WHO_AM_I response for LIS2DU12

#define OUT_T_L         0x2E
#define OUT_T_H         0x2F

/* ============================================================
 *  STEP 1: Select the I2C peripheral
 *  Options:  hi2c1  |  hi2c3
 * ============================================================ */
#define ACCEL_I2C   hi2c1

/* ============================================================
 *  STEP 2: Select the pin mapping for the chosen peripheral
 *
 *  hi2c1 has two possible mappings in STM32F401:
 *    I2C1_MAP_A  -->  PB6 (SCL)  /  PB7 (SDA)   [default remap]
 *    I2C1_MAP_B  -->  PB8 (SCL)  /  PB9 (SDA)   [alternate remap]
 *
 *  hi2c3 has one mapping:
 *    I2C3_MAP   -->   PA8 (SCL)  /  PC9 (SDA)
 *
 *  Uncomment exactly ONE of the options below that matches
 *  your CubeMX pin assignment.
 * ============================================================ */
//#define I2C1_MAP_A      /* PB6 SCL / PB7 SDA  (default)   */
 #define I2C1_MAP_B   /* PB8 SCL / PB9 SDA  (alternate) */
// #define I2C3_MAP     /* PA8 SCL / PC9 SDA               */

/* Auto-resolve bus name and pin labels — do not edit below */
#if defined(I2C1_MAP_A)
    #define ACCEL_I2C_NAME  "I2C1"
    #define ACCEL_I2C_SCL   "PB6"
    #define ACCEL_I2C_SDA   "PB7"
#elif defined(I2C1_MAP_B)
    #define ACCEL_I2C_NAME  "I2C1"
    #define ACCEL_I2C_SCL   "PB8"
    #define ACCEL_I2C_SDA   "PB9"
#elif defined(I2C3_MAP)
    #define ACCEL_I2C_NAME  "I2C3"
    #define ACCEL_I2C_SCL   "PA8"
    #define ACCEL_I2C_SDA   "PC9"
#else
    #error "No I2C pin mapping selected! Uncomment one of I2C1_MAP_A, I2C1_MAP_B, or I2C3_MAP."
#endif
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

const osThreadAttr_t processTask_attr = {
    .name = "ProcessTask",
    .stack_size = 512 * 4,
    .priority = osPriorityNormal
};

typedef struct
{
    float x;
    float y;
    float z;
} AccelData_t;

osMessageQueueId_t accelQueue;

// Software timer handles
osTimerId_t sensorReadTimer;
osTimerId_t loggingTimer;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C1_Init(void);
void StartDefaultTask(void *argument);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

int _write(int file, char *ptr, int len)
{
  HAL_UART_Transmit(&huart2, (uint8_t*)ptr, len, HAL_MAX_DELAY);
  return len;
}

void LIS2DU12_Init(void)
{
    osDelay(50);
    uint8_t data;
    uint8_t who_am_i = 0;

    /* --- I2C Bus Info --- */
    printf("========================================\r\n");
    printf("  %s Bus Configuration\r\n", ACCEL_I2C_NAME);
    printf("========================================\r\n");
    printf("  SDA Pin  : %s\r\n",   ACCEL_I2C_SDA);
    printf("  SCL Pin  : %s\r\n",   ACCEL_I2C_SCL);
    printf("  Speed    : Standard Mode (100 kHz)\r\n");
    printf("  Address  : 0x%02X (7-bit: 0x19)\r\n", LIS2DU12_ADDR >> 1);
    printf("========================================\r\n\r\n");

    /* --- WHO_AM_I Check --- */
    printf("[INIT] Reading WHO_AM_I register (0x%02X)...\r\n", WHO_AM_I_REG);
    HAL_StatusTypeDef status = HAL_I2C_Mem_Read(&ACCEL_I2C,
                                                  LIS2DU12_ADDR,
                                                  WHO_AM_I_REG,
                                                  I2C_MEMADD_SIZE_8BIT,
                                                  &who_am_i,
                                                  1,
                                                  100);
    if (status != HAL_OK) {
        printf("[ERROR] I2C communication failed! Check wiring.\r\n");
        return;
    }

    if (who_am_i == LIS2DU12_WHO_AM_I_VAL) {
        printf("[INIT] WHO_AM_I = 0x%02X --> LIS2DU12 detected OK\r\n\r\n", who_am_i);
    } else {
        printf("[WARN] WHO_AM_I = 0x%02X (expected 0x%02X) --> Unknown device!\r\n\r\n",
               who_am_i, LIS2DU12_WHO_AM_I_VAL);
    }

    /* --- Sensor Configuration --- */
    printf("[INIT] Applying sensor configuration...\r\n");

    // CTRL1: IF_ADD_INC + basic settings
    data = 0x37;
    HAL_I2C_Mem_Write(&ACCEL_I2C, LIS2DU12_ADDR, CTRL1_REG, I2C_MEMADD_SIZE_8BIT, &data, 1, 100);
    printf("  [CTRL1 @ 0x%02X] = 0x%02X --> IF_ADD_INC enabled, basic config set\r\n", CTRL1_REG, data);

    // CTRL3: High-performance mode
    data = 0x20;
    HAL_I2C_Mem_Write(&ACCEL_I2C, LIS2DU12_ADDR, CTRL3_REG, I2C_MEMADD_SIZE_8BIT, &data, 1, 100);
    printf("  [CTRL3 @ 0x%02X] = 0x%02X --> High-Performance mode enabled\r\n", CTRL3_REG, data);

    // CTRL4: BDU enable
    data = 0x08;
    HAL_I2C_Mem_Write(&ACCEL_I2C, LIS2DU12_ADDR, CTRL4_REG, I2C_MEMADD_SIZE_8BIT, &data, 1, 100);
    printf("  [CTRL4 @ 0x%02X] = 0x%02X --> BDU (Block Data Update) enabled\r\n", CTRL4_REG, data);

    // CTRL5: ODR = 100 Hz, FS = ±2g
    data = 0x80;
    HAL_I2C_Mem_Write(&ACCEL_I2C, LIS2DU12_ADDR, CTRL5_REG, I2C_MEMADD_SIZE_8BIT, &data, 1, 100);
    printf("  [CTRL5 @ 0x%02X] = 0x%02X --> ODR = 100 Hz | Full Scale = +/-2g\r\n", CTRL5_REG, data);

    osDelay(50);

    printf("\r\n[INIT] Sensor configuration summary:\r\n");
    printf("  Sensor        : LIS2DU12 (STMicroelectronics)\r\n");
    printf("  Interface     : %s @ 100 kHz\r\n", ACCEL_I2C_NAME);
    printf("  SDA / SCL     : %s / %s\r\n",      ACCEL_I2C_SDA, ACCEL_I2C_SCL);
    printf("  Mode          : High-Performance\r\n");
    printf("  Output Rate   : 100 Hz\r\n");
    printf("  Full Scale    : +/- 2g\r\n");
    printf("  Sensitivity   : 0.061 mg/LSB\r\n");
    printf("  BDU           : Enabled\r\n");
    printf("  Auto-Increment: Enabled\r\n");
    printf("\r\n[INIT] LIS2DU12 ready. Starting data acquisition...\r\n");
    printf("========================================\r\n\r\n");
}

HAL_StatusTypeDef LIS2DU12_GetAccel(AccelData_t *data)
{
    uint8_t buffer[6];
    int16_t raw_x, raw_y, raw_z;
    uint8_t reg = OUTX_L_REG | 0x80;

    HAL_StatusTypeDef status = HAL_I2C_Mem_Read(&ACCEL_I2C,
                                                 LIS2DU12_ADDR,
                                                 reg,
                                                 I2C_MEMADD_SIZE_8BIT,
                                                 buffer,
                                                 6,
                                                 100);

    if (status != HAL_OK) {
        return status;
    }

    raw_x = (int16_t)((buffer[1] << 8) | buffer[0]);
    raw_y = (int16_t)((buffer[3] << 8) | buffer[2]);
    raw_z = (int16_t)((buffer[5] << 8) | buffer[4]);

    // Debug: Print raw values
    printf("Raw: X=%d Y=%d Z=%d\r\n", raw_x, raw_y, raw_z);

    data->x = raw_x * 0.061f / 1000.0f;
    data->y = raw_y * 0.061f / 1000.0f;
    data->z = raw_z * 0.061f / 1000.0f;

    return HAL_OK;
}

// Timer callback for sensor reading (periodic)
void SensorReadCallback(void *argument)
{
    AccelData_t data;

    // Read sensor data
    HAL_StatusTypeDef status = LIS2DU12_GetAccel(&data);

    if (status == HAL_OK) {
        // Send to queue for processing
        if (osMessageQueuePut(accelQueue, &data, 0, 0) == osOK) {
            printf("Timer: Sensor data captured - X=%.3f Y=%.3f Z=%.3f\r\n",
                   data.x, data.y, data.z);
        } else {
            printf("Timer: Queue full!\r\n");
        }
    } else {
        printf("Timer: Sensor read failed with status %d\r\n", status);
    }
}

// Timer callback for logging (periodic)
void LoggingCallback(void *argument)
{
    static uint32_t log_count = 0;

    printf("=== Log Entry #%lu ===\r\n", ++log_count);
    printf("System Time: %lu ms\r\n", HAL_GetTick());
    printf("Queue Space Available: \r\n");
    printf("=====================\r\n\r\n");
}

// Process task - processes data from queue
void ProcessTask(void *argument)
{
    AccelData_t data;
    osStatus_t status;

    LIS2DU12_Init();

    printf("ProcessTask started! Waiting for sensor data...\r\n");

    for(;;)
    {
        status = osMessageQueueGet(accelQueue, &data, NULL, osWaitForever);

        if(status == osOK)
        {
            printf("ProcessTask: X=%.3f Y=%.3f Z=%.3f\r\n",
                   data.x, data.y, data.z);
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
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  // Create periodic timer for sensor reading (every 500ms = 2Hz)
  // osTimerPeriodic: repeats automatically
  // osTimerOnce: single-shot timer
  sensorReadTimer = osTimerNew(SensorReadCallback,     // Callback function
                               osTimerPeriodic,       // Type: Periodic
                               NULL,                  // Argument (none)
                               NULL);                 // Attributes

  if (sensorReadTimer != NULL) {
      // Start the timer with 500ms period
      osTimerStart(sensorReadTimer, 500);  // 500ms = 2Hz
      printf("Sensor timer started (500ms period)\r\n");
  } else {
      printf("ERROR: Failed to create sensor timer!\r\n");
  }

  // Create periodic timer for logging (every 5 seconds)
  loggingTimer = osTimerNew(LoggingCallback,    // Callback function
                            osTimerPeriodic,    // Type: Periodic
                            NULL,               // Argument (none)
                            NULL);              // Attributes

  if (loggingTimer != NULL) {
      // Start the timer with 5 second period
      osTimerStart(loggingTimer, 5000);  // 5000ms = 5 seconds
      printf("Logging timer started (5 second period)\r\n");
  } else {
      printf("ERROR: Failed to create logging timer!\r\n");
  }
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* USER CODE BEGIN RTOS_QUEUES */
  accelQueue = osMessageQueueNew(5, sizeof(AccelData_t), NULL);
  if (accelQueue == NULL) {
      printf("ERROR: Failed to create queue!\r\n");
  } else {
      printf("Queue created successfully!\r\n");
  }
  /* USER CODE END RTOS_QUEUES */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  // Create process task (handles data processing)
  osThreadNew(ProcessTask, NULL, &processTask_attr);

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
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
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
