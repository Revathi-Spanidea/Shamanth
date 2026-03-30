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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* RCC */
#define RCC_BASE        0x40023800
#define RCC_AHB1ENR     (*(volatile uint32_t*)(RCC_BASE + 0x30))
#define RCC_APB1ENR     (*(volatile uint32_t*)(RCC_BASE + 0x40))

#define GPIOA_EN        (1<<0)
#define GPIOC_EN        (1<<2)
#define I2C3_EN         (1<<23)

/* GPIOA */
#define GPIOA_BASE      0x40020000
#define GPIOA_MODER     (*(volatile uint32_t*)(GPIOA_BASE+0x00))
#define GPIOA_OTYPER    (*(volatile uint32_t*)(GPIOA_BASE+0x04))
#define GPIOA_OSPEEDR   (*(volatile uint32_t*)(GPIOA_BASE+0x08))
#define GPIOA_PUPDR     (*(volatile uint32_t*)(GPIOA_BASE+0x0C))
#define GPIOA_AFRH      (*(volatile uint32_t*)(GPIOA_BASE+0x24))

/* GPIOC */
#define GPIOC_BASE      0x40020800
#define GPIOC_MODER     (*(volatile uint32_t*)(GPIOC_BASE+0x00))
#define GPIOC_OTYPER    (*(volatile uint32_t*)(GPIOC_BASE+0x04))
#define GPIOC_OSPEEDR   (*(volatile uint32_t*)(GPIOC_BASE+0x08))
#define GPIOC_PUPDR     (*(volatile uint32_t*)(GPIOC_BASE+0x0C))
#define GPIOC_AFRH      (*(volatile uint32_t*)(GPIOC_BASE+0x24))

/* I2C3 */
#define I2C3_BASE       0x40005C00
#define I2C3_CR1        (*(volatile uint32_t*)(I2C3_BASE+0x00))
#define I2C3_CR2        (*(volatile uint32_t*)(I2C3_BASE+0x04))
#define I2C3_DR         (*(volatile uint32_t*)(I2C3_BASE+0x10))
#define I2C3_SR1        (*(volatile uint32_t*)(I2C3_BASE+0x14))
#define I2C3_SR2        (*(volatile uint32_t*)(I2C3_BASE+0x18))
#define I2C3_CCR        (*(volatile uint32_t*)(I2C3_BASE+0x1C))
#define I2C3_TRISE      (*(volatile uint32_t*)(I2C3_BASE+0x20))

/* LIS2DU12 registers */
#define LIS2DU12_ADDR   0x18
#define CTRL1_REG       0x10
#define CTRL3_REG       0x12
#define CTRL4_REG       0x13
#define CTRL5_REG       0x14
#define OUTX_L_REG      0x28

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
int _write(int file, char *ptr, int len)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)ptr, len, HAL_MAX_DELAY);
    return len;
}

/* I2C START */
void I2C_Start()
{
    I2C3_CR1 |= (1<<8);
    while(!(I2C3_SR1 & (1<<0)));
}

/* STOP */
void I2C_Stop()
{
    I2C3_CR1 |= (1<<9);
}

/* ADDRESS */
void I2C_Address(uint8_t addr)
{
    I2C3_DR = addr;
    while(!(I2C3_SR1 & (1<<1)));
    (void)I2C3_SR2;
}

/* WRITE */
void I2C_Write(uint8_t data)
{
    while(!(I2C3_SR1 & (1<<7)));
    I2C3_DR = data;
    while(!(I2C3_SR1 & (1<<2)));
}

/* READ ACK */
uint8_t I2C_Read_ACK()
{
    I2C3_CR1 |= (1<<10);
    while(!(I2C3_SR1 & (1<<6)));
    return I2C3_DR;
}

/* READ NACK */
uint8_t I2C_Read_NACK()
{
    I2C3_CR1 &= ~(1<<10);
    while(!(I2C3_SR1 & (1<<6)));
    return I2C3_DR;
}

/* I2C3 Init */
void I2C3_Init()
{
    RCC_AHB1ENR |= GPIOA_EN | GPIOC_EN;
    RCC_APB1ENR |= I2C3_EN;

    /* PA8 SCL */
    GPIOA_MODER &= ~(3<<(8*2));
    GPIOA_MODER |=  (2<<(8*2));

    GPIOA_OTYPER |= (1<<8);
    GPIOA_PUPDR  |= (1<<(8*2));
    GPIOA_AFRH   |= (4<<0);

    /* PC9 SDA */
    GPIOC_MODER &= ~(3<<(9*2));
    GPIOC_MODER |=  (2<<(9*2));

    GPIOC_OTYPER |= (1<<9);
    GPIOC_PUPDR  |= (1<<(9*2));
    GPIOC_AFRH   |= (4<<4);

    I2C3_CR1 |= (1<<15);
    I2C3_CR1 &= ~(1<<15);

    I2C3_CR2 = 16;
    I2C3_CCR = 80;
    I2C3_TRISE = 17;

    I2C3_CR1 |= 1;
}

/* Write register */
void Sensor_Write(uint8_t reg,uint8_t value)
{
    I2C_Start();
    I2C_Address(LIS2DU12_ADDR<<1);
    I2C_Write(reg);
    I2C_Write(value);
    I2C_Stop();
}

/* Read multiple bytes */
void Sensor_Read(uint8_t reg,uint8_t *buf,uint8_t len)
{
    I2C_Start();
    I2C_Address(LIS2DU12_ADDR<<1);
    I2C_Write(reg);

    I2C_Start();
    I2C_Address((LIS2DU12_ADDR<<1)|1);

    for(int i=0;i<len-1;i++)
        buf[i] = I2C_Read_ACK();

    buf[len-1] = I2C_Read_NACK();

    I2C_Stop();
}

/* Sensor Init */
void LIS2DU12_Init()
{
    HAL_Delay(50);

    Sensor_Write(CTRL1_REG,0x37);
    Sensor_Write(CTRL3_REG,0x20);
    Sensor_Write(CTRL4_REG,0x08);
    Sensor_Write(CTRL5_REG,0x80);

    printf("Sensor Init Done\r\n");
}

/* Read Acceleration */
void LIS2DU12_ReadAccel()
{
    uint8_t buffer[6];
    int16_t raw_x,raw_y,raw_z;

    Sensor_Read(OUTX_L_REG | 0x80,buffer,6);

    raw_x = (buffer[1]<<8)|buffer[0];
    raw_y = (buffer[3]<<8)|buffer[2];
    raw_z = (buffer[5]<<8)|buffer[4];

    float ax = raw_x * 0.061f / 1000.0f;
    float ay = raw_y * 0.061f / 1000.0f;
    float az = raw_z * 0.061f / 1000.0f;

    printf("X: %.3f g  Y: %.3f g  Z: %.3f g\r\n",ax,ay,az);
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
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  printf("LIS2DU12 Accelerometer Test\r\n");

  I2C3_Init();
  LIS2DU12_Init();

  while(1)
  {
      LIS2DU12_ReadAccel();
      HAL_Delay(500);
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

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

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
