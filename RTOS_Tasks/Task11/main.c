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
#include "event_groups.h"
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

typedef struct {
    float temperature;
    float humidity;
    uint32_t timestamp;
} TempData_t;

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

// Each bit represents an event
#define BIT_ACCEL_READY  (1 << 0)  // Bit 0
#define BIT_TEMP_READY   (1 << 1)  // Bit 1
#define BIT_BOTH_READY   (BIT_ACCEL_READY | BIT_TEMP_READY)


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
// Queue handles
osMessageQueueId_t accelQueue;
osMessageQueueId_t tempQueue;

// Storage buffers
#define BUFFER_SIZE 50
AccelData_t accelStorage[BUFFER_SIZE];
TempData_t tempStorage[BUFFER_SIZE];
int accelCount = 0;
int tempCount = 0;

// Statistics
float accel_min_x = 999, accel_max_x = -999;
float temp_min = 999, temp_max = -999;
float humidity_min = 999, humidity_max = -999;


// Task handles
osThreadId_t accelTaskHandle;
osThreadId_t tempTaskHandle;
osThreadId_t storageTaskHandle;
osThreadId_t reportTaskHandle;

osMutexId_t i2cMutex;

EventGroupHandle_t sensorEventGroup;

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

// Corrected CRC-8 for SHT40 (polynomial 0x31, init 0xFF)
uint8_t SHT40_CRC8(uint8_t *data, uint8_t len)
{
    uint8_t crc = 0xFF;

    for(uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for(uint8_t bit = 0; bit < 8; bit++) {
            if(crc & 0x80) {
                crc = (crc << 1) ^ 0x31;  // Polynomial 0x31
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

void SHT40_Init(void)
{
    printf("========================================\r\n");
    printf("  SHT40 Temperature/Humidity Sensor Init\r\n");
    printf("========================================\r\n");
    printf("  Address: 0x%02X\r\n", SHT40_ADDR);
    printf("========================================\r\n\r\n");

//    osMutexAcquire(i2cMutex, osWaitForever);

    // Send soft reset - FIXED ADDRESS
    uint8_t reset_cmd = SHT40_CMD_SOFT_RESET;
    HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(&ACCEL_I2C, SHT40_ADDR << 1, &reset_cmd, 1, 100);

    if(status == HAL_OK) {
        printf("[OK] Soft reset sent\r\n");
        osDelay(10);
    } else {
        printf("[ERROR] Soft reset failed! Status: %d\r\n", status);
//        osMutexRelease(i2cMutex);
        return;
    }

    // Read serial number - FIXED ADDRESS
    uint8_t serial_cmd = SHT40_CMD_READ_SERIAL;
    uint8_t serial_data[6];

    status = HAL_I2C_Master_Transmit(&ACCEL_I2C, SHT40_ADDR << 1, &serial_cmd, 1, 100);

    if(status == HAL_OK) {
        osDelay(10);
        status = HAL_I2C_Master_Receive(&ACCEL_I2C, SHT40_ADDR << 1, serial_data, 6, 100);

        if(status == HAL_OK) {
            uint8_t crc1 = SHT40_CRC8(serial_data, 2);
            uint8_t crc2 = SHT40_CRC8(&serial_data[3], 2);

            if(crc1 == serial_data[2] && crc2 == serial_data[5]) {
                uint32_t serial_num = (serial_data[0] << 24) | (serial_data[1] << 16) |
                                      (serial_data[3] << 8) | serial_data[4];
                printf("[OK] SHT40 detected! Serial: 0x%08lX\r\n", serial_num);
            } else {
                printf("[WARN] CRC mismatch\r\n");
            }
        } else {
            printf("[WARN] Could not read serial number\r\n");
        }
    } else {
        printf("[WARN] SHT40 not responding\r\n");
    }

//    osMutexRelease(i2cMutex);
    printf("\r\n");
}

HAL_StatusTypeDef SHT40_Read(TempData_t *data)
{
    uint8_t cmd = SHT40_CMD_HIGH_PRECISION;
    uint8_t buffer[6];
    uint16_t temp_raw, humidity_raw;

//    osMutexAcquire(i2cMutex, osWaitForever);

    // Send measurement command - FIXED ADDRESS
    HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(&ACCEL_I2C, SHT40_ADDR << 1, &cmd, 1, 100);

    if(status != HAL_OK) {
        printf("[ERROR] SHT40 transmit failed: %d\r\n", status);
//        osMutexRelease(i2cMutex);
        return status;
    }

    osDelay(15);  // Wait for measurement

    // Read data - FIXED ADDRESS
    status = HAL_I2C_Master_Receive(&ACCEL_I2C, SHT40_ADDR << 1, buffer, 6, 100);
//    osMutexRelease(i2cMutex);

    if(status != HAL_OK) {
        printf("[ERROR] SHT40 receive failed: %d\r\n", status);
        return status;
    }

    // Verify CRCs
    uint8_t temp_crc = SHT40_CRC8(buffer, 2);
    uint8_t hum_crc = SHT40_CRC8(&buffer[3], 2);

    if(temp_crc != buffer[2] || hum_crc != buffer[5]) {
        printf("[ERROR] SHT40 CRC mismatch\r\n");
        return HAL_ERROR;
    }

    // Convert values
    temp_raw = (buffer[0] << 8) | buffer[1];
    humidity_raw = (buffer[3] << 8) | buffer[4];

    data->temperature = -45.0f + (175.0f * temp_raw / 65535.0f);
    data->humidity = -6.0f + (125.0f * humidity_raw / 65535.0f);
    data->timestamp = HAL_GetTick();

    return HAL_OK;
}


// Accelerometer Task
void AccelTask(void *argument)
{
    AccelData_t data;

    osMutexAcquire(i2cMutex, osWaitForever);
    LIS2DU12_Init();
    osMutexRelease(i2cMutex);

    for(;;)
    {
        osMutexAcquire(i2cMutex, osWaitForever);
        if(LIS2DU12_GetAccel(&data) == HAL_OK) {
            osMessageQueuePut(accelQueue, &data, 0, 0);
            // Set event bit for accelerometer
            xEventGroupSetBits(sensorEventGroup, BIT_ACCEL_READY);
        }
        osMutexRelease(i2cMutex);
        osDelay(1000);
    }
}

// Temperature Task
void TempTask(void *argument)
{
    TempData_t data;

    osMutexAcquire(i2cMutex, osWaitForever);
    SHT40_Init();
    osMutexRelease(i2cMutex);

    for(;;)
    {
        osMutexAcquire(i2cMutex, osWaitForever);
        if(SHT40_Read(&data) == HAL_OK) {
            osMessageQueuePut(tempQueue, &data, 0, 0);
            // Set event bit for temperature
            xEventGroupSetBits(sensorEventGroup, BIT_TEMP_READY);
        }
        osMutexRelease(i2cMutex);
        osDelay(1000);
    }
}


// Storage Task - stores data in memory
void StorageTask(void *argument)
{
    AccelData_t accelData;
    TempData_t tempData;
    EventBits_t bits;

    for(;;)
    {
        // Wait for BOTH accelerometer AND temperature to be ready
        bits = xEventGroupWaitBits(sensorEventGroup,
                                   BIT_BOTH_READY,     // Bits to wait for
                                   pdTRUE,             // Clear bits after wait
                                   pdTRUE,             // Wait for ALL bits
                                   portMAX_DELAY);     // Wait forever

        if((bits & BIT_BOTH_READY) == BIT_BOTH_READY) {
            // Both sensors have new data
            if(osMessageQueueGet(accelQueue, &accelData, NULL, 0) == osOK) {
                if(accelCount < BUFFER_SIZE) {
                    accelStorage[accelCount] = accelData;
                    accelCount++;
                }
            }

            if(osMessageQueueGet(tempQueue, &tempData, NULL, 0) == osOK) {
                if(tempCount < BUFFER_SIZE) {
                    tempStorage[tempCount] = tempData;
                    tempCount++;
                }
            }

            printf("[SYNC] Stored pair #%d: Accel + Temp\r\n", accelCount);
        }
    }
}

// Report Task - displays stored data periodically
void ReportTask(void *argument)
{
    int reportCount = 0;

    printf("Report Task started - Will report every 30 seconds\r\n");

    for(;;)
    {
        osDelay(10000);  // Every 30 seconds

        printf("\r\n");
        printf("========================================\r\n");
        printf("STORED DATA REPORT #%d\r\n", ++reportCount);
        printf("========================================\r\n");
        printf("Accelerometer samples: %d / %d\r\n", accelCount, BUFFER_SIZE);
        printf("Temperature samples:   %d / %d\r\n", tempCount, BUFFER_SIZE);

        if(accelCount > 0) {
            printf("\r\nLast Accelerometer Reading:\r\n");
            printf("  X = %.3f g\r\n", accelStorage[accelCount-1].x);
            printf("  Y = %.3f g\r\n", accelStorage[accelCount-1].y);
            printf("  Z = %.3f g\r\n", accelStorage[accelCount-1].z);
            printf("  Time = %lu ms\r\n", accelStorage[accelCount-1].timestamp);
        }

        if(tempCount > 0) {
            printf("\r\nLast Temperature Reading:\r\n");
            printf("  Temperature = %.1f°C\r\n", tempStorage[tempCount-1].temperature);
            printf("  Humidity = %.1f%%\r\n", tempStorage[tempCount-1].humidity);
            printf("  Time = %lu ms\r\n", tempStorage[tempCount-1].timestamp);
        }

        printf("========================================\r\n\r\n");
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
  printf("MULTI-SENSOR RTOS DEMO\r\n");
  printf("========================================\r\n");

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

  // Create queues for communication
  accelQueue = osMessageQueueNew(20, sizeof(AccelData_t), NULL);
  tempQueue = osMessageQueueNew(20, sizeof(TempData_t), NULL);

  if(accelQueue == NULL || tempQueue == NULL) {
      printf("ERROR: Failed to create queues!\r\n");
  } else {
      printf("Queues created successfully!\r\n");
  }

  /* USER CODE BEGIN RTOS_QUEUES */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */

  // Create tasks
  const osThreadAttr_t accelTask_attr = {
      .name = "AccelTask",
      .stack_size = 256 * 4,
      .priority = osPriorityNormal
  };
  accelTaskHandle = osThreadNew(AccelTask, NULL, &accelTask_attr);

  const osThreadAttr_t tempTask_attr = {
      .name = "TempTask",
      .stack_size = 256 * 4,
      .priority = osPriorityNormal
  };
  tempTaskHandle = osThreadNew(TempTask, NULL, &tempTask_attr);

  const osThreadAttr_t storageTask_attr = {
      .name = "StorageTask",
      .stack_size = 512 * 4,
      .priority = osPriorityLow
  };
  storageTaskHandle = osThreadNew(StorageTask, NULL, &storageTask_attr);

  const osThreadAttr_t reportTask_attr = {
      .name = "ReportTask",
      .stack_size = 256 * 4,
      .priority = osPriorityLow
  };
  reportTaskHandle = osThreadNew(ReportTask, NULL, &reportTask_attr);

  // Create default task
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */

  sensorEventGroup = xEventGroupCreate();
  if(sensorEventGroup == NULL) {
      printf("ERROR: Failed to create event group!\r\n");
  }

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
    printf("SYSTEM READY!\r\n");
    printf("========================================\r\n");
    printf("Tasks Running:\r\n");
    printf("  - AccelTask:   10 Hz (reads accelerometer)\r\n");
    printf("  - TempTask:    1 Hz  (reads temperature/humidity)\r\n");
    printf("  - StorageTask: Stores data in RAM buffers\r\n");
    printf("  - ReportTask:  Reports every 30 seconds\r\n");
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
