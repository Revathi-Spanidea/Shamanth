# Shamanth - Ramp up Tasks

---

## Application Level

1. Write a program to interface **LED with GPIO** of STM32 microcontroller ✅ 
2. Write an application to create **LED chaser (Alternate LEDs should glow)** for STM32 microcontroller ✅
3. Write an application code to **control LED intensity using PWM** ✅
4. Write an application code to **interface Push Button and LED in polling method** ✅
5. Write an application to **Interface STM32 microcontroller I2C/SPI-based sensors like BMP280** ✅
6. Write an application to **read and Write data from the EEPROM memory using I2C in RPi** ❌
7. Write an application to **send and receive data via UART to a PC terminal** (e.g., Tera Term, PuTTY, Minicom) for RPi ✅
8. Write an application Using **MPU6050 or LSM6DS3 to get motion and tilt data in RPi** ✅

---

## Problem Description (Register-Level)

9. Write a program to interface **LED with GPIO** of STM32 microcontroller ✅
10. Write an application to create **LED chaser (Alternate LEDs should glow)** using STM32 microcontroller ✅
11. Write an application code for STM32 microcontroller to **control LED intensity using PWM** ✅
12. Write an application code to **interface Push Button and LED in polling method** for STM32 microcontroller ✅
13. Write an application to **Interface STM32 microcontroller I2C/SPI-based sensors like BMP280** ✅
14. Write an application to **read and Write data from the EEPROM memory using I2C** ❌
15. Write an application to **send and receive data via UART to a PC terminal** (e.g., Tera Term, PuTTY, Minicom) ✅
16. Write an application Using **MPU6050 or LSM6DS3 to get motion and tilt data** ❌

---

## Timers, Interrupts, and RTOS Basics – Problem Description

17. Write an application to **read potentiometer values via ADC** ❌
18. Write an application code to **measure power supply voltage via ADC** ✅
19. Write an application to **implement STOP and STANDBY modes for power optimization** ✅
20. Write an application to **enable hardware interrupt for controlling output** ✅
21. Write an application to **Control multiple LEDs with different ON/OFF intervals without using delay()** ✅
22. Write an application to **implement software or hardware debouncing for a button-controlled LED** ✅
23. Write an application to **modify the delay to make the LED blink faster (e.g., 100ms ON/OFF) using a timer** ✅
24. Write an application to **Change the LED color based on temperature sensor readings and create an interrupt to give a buzzing sound** ❌
25. Write an application to **Control the LED remotely via STM32 with BLE** ❌
26. Write an application to **Implement a traffic light sequence using Red, Yellow, and Green LEDs for STM32F401RE development board** ❌

---

## Real-Time Operating Systems (RTOS) – STM32F401RE

Introduction to RTOS  
Task Scheduling & Synchronization  
FreeRTOS Hands-on  
RTOS in Embedded Applications  

---

### Problem Description:

1. Write an application to **Create multiple FreeRTOS tasks to blink LEDs at different rates** ✅  
2. Write an application to **Use a button press to trigger an LED toggle via a binary semaphore** ✅  
3. Write an application to **Implement a round-robin scheduling where tasks execute in a cyclic manner with equal priority** ✅  
4. Write an application to **Implement multiple tasks with different priorities and observe preemption behavior** ✅  
5. Write an application to **Create two tasks: one task sends sensor data to a queue, and another task reads and processes it** ✅  
6. Write an application to **Use task notifications instead of semaphores to trigger tasks based on external inputs** ✅  
7. Write an application to **Create software timers that trigger periodic tasks (e.g., sensor reading, logging)** ✅  
8. Write an application to **Dynamically create and delete tasks based on system requirements** ✅  
9. Write an application to **Implement a low-power mode using the FreeRTOS idle task hook** ✅  
10. Write an application to **Read multiple sensors at different rates and store data in memory using scheduled tasks** ✅  
11. Write an application to **Use FreeRTOS event groups to synchronize multiple tasks based on multiple conditions** ✅  
12. Write an application to **Implement UART communication where multiple tasks can send and receive messages concurrently** ✅  
13. Write an application to **Use an external interrupt (e.g., button press) to wake up a FreeRTOS task** ✅  
14. Write an application to **Implement Producer-Consumer model with queues (sensor data generation and processing)** ✅  
15. Write an application to **Create a high-priority task waiting for a resource held by a low-priority task and observe priority inversion** ✅  


---

## ESP32 LyraT Mini – Task Implementation

### PlatformIO Tasks:

1. Write an application to **demonstrate ADC-based button inputs available on the board** ✅  
2. Write an application to **implement BLE beacon broadcasting** ✅  
3. Write an application to **control LED blinking via BLE using a smartphone** ✅  
4. Write an application to **host a Wi-Fi web server to control LED blinking** ✅  
5. Write an application to **run Wi-Fi and BLE simultaneously on the ESP32** ✅  
6. Write an application to **generate a beep sound using the audio jack output** ✅   

---

### ESP-IDF Tasks:

1. Write an application to **blink an LED using the ESP-IDF framework** ✅  
2. Write an application to **demonstrate FreeRTOS mutex usage in ESP-IDF** ✅  
3. Write an application to **demonstrate FreeRTOS semaphore mechanisms in ESP-IDF** ✅  
4. Write an application to **implement inter-task communication using FreeRTOS queues** ✅  
5. Write an application to **demonstrate task prioritization using UART print statements in FreeRTOS** ✅  
6. Write an application to **demonstrate the onboard microphone functionality** ✅ 
