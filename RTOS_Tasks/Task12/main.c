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
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

// Simple message structure
typedef struct {
    char data[64];
    uint16_t length;
} UART_Message_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

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

// Mutex for UART access
osMutexId_t uartMutex;

// Queues for sending and receiving
osMessageQueueId_t sendQueue;
osMessageQueueId_t receiveQueue;

// Task handles
osThreadId_t sendTaskHandle;
osThreadId_t receiveTaskHandle;
osThreadId_t processTaskHandle;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C1_Init(void);
void StartDefaultTask(void *argument);

/* USER CODE BEGIN PFP */
void SendTask(void *argument);
void ReceiveTask(void *argument);
void ProcessTask(void *argument);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// Custom printf that uses mutex
int _write(int file, char *ptr, int len)
{
    if (uartMutex != NULL) {
        osMutexAcquire(uartMutex, osWaitForever);
        HAL_UART_Transmit(&huart2, (uint8_t*)ptr, len, HAL_MAX_DELAY);
        osMutexRelease(uartMutex);
    }
    return len;
}

// Task 1: Sends messages via UART
void SendTask(void *argument)
{
    uint32_t counter = 0;
    char msg[64];

    for(;;)
    {
        // Prepare message
        counter++;
        sprintf(msg, "Message #%lu from SendTask\r\n", counter);

        // Send via queue or directly
        UART_Message_t uart_msg;
        strcpy(uart_msg.data, msg);
        uart_msg.length = strlen(msg);

        // Put in queue for sending
        osMessageQueuePut(sendQueue, &uart_msg, 0, 0);

        printf("%s", msg);  // This will use mutex

        osDelay(2000);  // Send every 2 seconds
    }
}

// Task 2: Receives messages from UART (interrupt-driven)
void ReceiveTask(void *argument)
{
    uint8_t rx_byte;
    static char rx_buffer[128];
    static uint16_t rx_index = 0;

    for(;;)
    {
        // Wait for a character
        if(HAL_UART_Receive(&huart2, &rx_byte, 1, 100) == HAL_OK)
        {
            if(rx_byte == '\r' || rx_byte == '\n')
            {
                if(rx_index > 0)
                {
                    rx_buffer[rx_index] = '\0';

                    // Put received message in queue
                    UART_Message_t rx_msg;
                    strcpy(rx_msg.data, rx_buffer);
                    rx_msg.length = rx_index;
                    osMessageQueuePut(receiveQueue, &rx_msg, 0, 0);

                    rx_index = 0;
                }
            }
            else if(rx_index < sizeof(rx_buffer) - 1)
            {
                rx_buffer[rx_index++] = rx_byte;
            }
        }

        osDelay(10);
    }
}

// Task 3: Processes received messages
void ProcessTask(void *argument)
{
    UART_Message_t rx_msg;
    uint32_t msg_count = 0;

    for(;;)
    {
        // Wait for a received message
        if(osMessageQueueGet(receiveQueue, &rx_msg, NULL, portMAX_DELAY) == osOK)
        {
            msg_count++;

            // Process and respond
            printf("\r\n[ProcessTask] Received #%lu: %s\r\n", msg_count, rx_msg.data);

            // Echo back
            if(strcmp(rx_msg.data, "hello") == 0)
            {
                printf("[ProcessTask] Hello back to you!\r\n");
            }
            else if(strcmp(rx_msg.data, "status") == 0)
            {
                printf("[ProcessTask] System is running. Received %lu messages\r\n", msg_count);
            }
            else if(strcmp(rx_msg.data, "clear") == 0)
            {
                printf("\033[2J\033[H");  // Clear screen
                printf("[ProcessTask] Screen cleared!\r\n");
            }
            else
            {
                printf("[ProcessTask] Echo: %s\r\n", rx_msg.data);
            }
        }
    }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  */
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();
    MX_I2C1_Init();

    printf("\r\n========================================\r\n");
    printf("MULTI-TASK UART COMMUNICATION\r\n");
    printf("========================================\r\n");
    printf("Tasks:\r\n");
    printf("  - SendTask: Sends messages every 2 seconds\r\n");
    printf("  - ReceiveTask: Listens for incoming messages\r\n");
    printf("  - ProcessTask: Processes received messages\r\n");
    printf("========================================\r\n");
    printf("Try typing: hello, status, clear\r\n");
    printf("========================================\r\n\n");

    osKernelInitialize();

    /* USER CODE BEGIN RTOS_MUTEX */

    // Create mutex for UART
    uartMutex = osMutexNew(NULL);

    /* USER CODE END RTOS_MUTEX */

    /* USER CODE BEGIN RTOS_QUEUES */

    // Create queues
    sendQueue = osMessageQueueNew(10, sizeof(UART_Message_t), NULL);
    receiveQueue = osMessageQueueNew(10, sizeof(UART_Message_t), NULL);

    /* USER CODE END RTOS_QUEUES */

    /* Create the thread(s) */
    defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

    /* USER CODE BEGIN RTOS_THREADS */

    // Create the 3 tasks
    const osThreadAttr_t task_attr = {
        .stack_size = 256 * 4,
        .priority = osPriorityNormal
    };

    osThreadAttr_t send_attr = task_attr;
    send_attr.name = "SendTask";
    sendTaskHandle = osThreadNew(SendTask, NULL, &send_attr);

    osThreadAttr_t recv_attr = task_attr;
    recv_attr.name = "ReceiveTask";
    receiveTaskHandle = osThreadNew(ReceiveTask, NULL, &recv_attr);

    osThreadAttr_t proc_attr = task_attr;
    proc_attr.name = "ProcessTask";
    processTaskHandle = osThreadNew(ProcessTask, NULL, &proc_attr);

    /* USER CODE END RTOS_THREADS */

    /* Start scheduler */
    osKernelStart();

    while (1)
    {
    }
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
