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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
// Data structures
typedef struct {
    float x;
    float y;
    float z;
    uint32_t timestamp;
} AccelData_t;

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


#define SHT40_ADDR          0x44

// SHT40 Commands (from datasheet)
#define SHT40_CMD_HIGH_PRECISION    0xFD
#define SHT40_CMD_MED_PRECISION     0xF6
#define SHT40_CMD_LOW_PRECISION     0xE0
#define SHT40_CMD_READ_SERIAL       0x89
#define SHT40_CMD_SOFT_RESET        0x94


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
// Queue handle - only one queue needed
osMessageQueueId_t accelQueue;

// Storage buffer
#define BUFFER_SIZE 50
AccelData_t accelStorage[BUFFER_SIZE];
int accelCount = 0;

// Task handles
osThreadId_t producerTaskHandle;   // Was: accelTaskHandle
osThreadId_t consumerTaskHandle;   // Was: storageTaskHandle
osThreadId_t monitorTaskHandle;    // Was: reportTaskHandle

osMutexId_t i2cMutex;

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
//    printf("Raw: X=%d Y=%d Z=%d\r\n", raw_x, raw_y, raw_z);

    data->x = raw_x * 0.061f / 1000.0f;
    data->y = raw_y * 0.061f / 1000.0f;
    data->z = raw_z * 0.061f / 1000.0f;

    return HAL_OK;
}
// PRODUCER Task - Generates sensor data
void ProducerTask(void *argument)
{
    AccelData_t data;
    uint32_t produced_count = 0;

    // Initialize sensor
    osMutexAcquire(i2cMutex, osWaitForever);
    LIS2DU12_Init();
    osMutexRelease(i2cMutex);

    printf("\r\n[PRODUCER] Task started - Reading accelerometer at 2 Hz\r\n");
    printf("[PRODUCER] Generating data and sending to queue...\r\n\r\n");

    for(;;)
    {
        osMutexAcquire(i2cMutex, osWaitForever);
        if(LIS2DU12_GetAccel(&data) == HAL_OK) {
            data.timestamp = HAL_GetTick();
            produced_count++;

            // Send to queue (Producer -> Consumer)
            if(osMessageQueuePut(accelQueue, &data, 0, 100) == osOK) {
                printf("[PRODUCER] #%lu: X=%.2f Y=%.2f Z=%.2f (sent to queue)\r\n",
                       produced_count, data.x, data.y, data.z);
            } else {
                printf("[PRODUCER] ERROR: Queue full! Data lost!\r\n");
            }
        }
        osMutexRelease(i2cMutex);

        osDelay(500);  // Produce every 500ms (2 Hz)
    }
}

// CONSUMER Task - Processes data from queue
void ConsumerTask(void *argument)
{
    AccelData_t data;
    uint32_t consumed_count = 0;
    float sum_x = 0, sum_y = 0, sum_z = 0;

    printf("[CONSUMER] Task started - Waiting for data from queue...\r\n\r\n");

    for(;;)
    {
        // Wait for data from queue (blocking)
        if(osMessageQueueGet(accelQueue, &data, NULL, portMAX_DELAY) == osOK) {
            consumed_count++;

            // Process the data (Consumer work)
            sum_x += data.x;
            sum_y += data.y;
            sum_z += data.z;

            // Store in buffer
            if(accelCount < BUFFER_SIZE) {
                accelStorage[accelCount] = data;
                accelCount++;
            }

            printf("[CONSUMER] #%lu: X=%.2f Y=%.2f Z=%.2f | Queue space: %lu\r\n",
                   consumed_count, data.x, data.y, data.z,
                   osMessageQueueGetSpace(accelQueue));

            // Simulate processing time
            osDelay(100);
        }
    }
}


// MONITOR Task - Reports statistics
void MonitorTask(void *argument)
{
    int reportCount = 0;
    float avg_x = 0, avg_y = 0, avg_z = 0;

    printf("[MONITOR] Task started - Reporting every 10 seconds\r\n\r\n");

    for(;;)
    {
        osDelay(10000);  // Report every 10 seconds

        printf("\r\n========================================\r\n");
        printf("PRODUCER-CONSUMER REPORT #%d\r\n", ++reportCount);
        printf("========================================\r\n");
        printf("Queue status:\r\n");
        printf("  - Messages waiting: %lu\r\n", osMessageQueueGetCount(accelQueue));
        printf("  - Free space: %lu\r\n", osMessageQueueGetSpace(accelQueue));
        printf("\r\nProduction/Consumption:\r\n");
        printf("  - Samples produced: %d\r\n", accelCount);
        printf("  - Samples consumed: %d\r\n", accelCount);

        if(accelCount > 0) {
            // Calculate averages
            for(int i = 0; i < accelCount; i++) {
                avg_x += accelStorage[i].x;
                avg_y += accelStorage[i].y;
                avg_z += accelStorage[i].z;
            }
            avg_x /= accelCount;
            avg_y /= accelCount;
            avg_z /= accelCount;

            printf("\r\nStatistics:\r\n");
            printf("  - Avg X: %.3f g\r\n", avg_x);
            printf("  - Avg Y: %.3f g\r\n", avg_y);
            printf("  - Avg Z: %.3f g\r\n", avg_z);
            printf("\r\nLast sample:\r\n");
            printf("  - X = %.3f g\r\n", accelStorage[accelCount-1].x);
            printf("  - Y = %.3f g\r\n", accelStorage[accelCount-1].y);
            printf("  - Z = %.3f g\r\n", accelStorage[accelCount-1].z);
        }
        printf("========================================\r\n\r\n");

        // Reset averages for next report
        avg_x = avg_y = avg_z = 0;
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

  printf("\r\n========================================\r\n");
  printf("PRODUCER-CONSUMER DEMO WITH QUEUES\r\n");
  printf("========================================\r\n");
  printf("System Initializing...\r\n");

  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();

  /* USER CODE BEGIN RTOS_MUTEX */

  // Create I2C mutex
  i2cMutex = osMutexNew(NULL);
  if(i2cMutex == NULL) {
      printf("ERROR: Failed to create I2C mutex!\r\n");
  }

  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */

  // Create queue for Producer-Consumer communication
  accelQueue = osMessageQueueNew(10, sizeof(AccelData_t), NULL);  // Queue size 10

  if(accelQueue == NULL) {
      printf("ERROR: Failed to create queue!\r\n");
  } else {
      printf("Queue created successfully! (Size: 10 messages)\r\n");
  }

  /* USER CODE BEGIN RTOS_QUEUES */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */


  // Create Producer Task (generates data)
  const osThreadAttr_t producerTask_attr = {
      .name = "ProducerTask",
      .stack_size = 256 * 4,
      .priority = osPriorityNormal
  };
  producerTaskHandle = osThreadNew(ProducerTask, NULL, &producerTask_attr);

  // Create Consumer Task (processes data)
  const osThreadAttr_t consumerTask_attr = {
      .name = "ConsumerTask",
      .stack_size = 256 * 4,
      .priority = osPriorityNormal
  };
  consumerTaskHandle = osThreadNew(ConsumerTask, NULL, &consumerTask_attr);

  // Create Monitor Task (reports statistics)
  const osThreadAttr_t monitorTask_attr = {
      .name = "MonitorTask",
      .stack_size = 256 * 4,
      .priority = osPriorityLow
  };
  monitorTaskHandle = osThreadNew(MonitorTask, NULL, &monitorTask_attr);

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
    printf("\r\n========================================\r\n");
    printf("PRODUCER-CONSUMER DEMO WITH QUEUES\r\n");
    printf("========================================\r\n");
    printf("Architecture:\r\n");
    printf("  [Producer] -> [Queue] -> [Consumer]\r\n");
    printf("\r\nTasks:\r\n");
    printf("  - Producer: Reads accelerometer (2 Hz)\r\n");
    printf("  - Consumer: Processes data from queue\r\n");
    printf("  - Monitor:  Reports statistics (10 sec)\r\n");
    printf("========================================\r\n");
    printf("Queue size: 10 messages (blocking when full)\r\n");
    printf("========================================\r\n\r\n");

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
