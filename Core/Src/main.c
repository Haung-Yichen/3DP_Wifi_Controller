/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/*******   HAL LIB   *******/
#include "main.h"
#include "cmsis_os.h"
#include "dma.h"
#include "usart.h"
#include "gpio.h"
#include "stm32f1xx_ll_fsmc.h"

/*******   C LIB   *******/
#include <stdio.h>
#include <string.h>

/*******   Fire Bsp LIB   *******/
#include "bsp_led.h"
#include "bsp_sdio_sdcard.h"

/*******   SDIO LIB   *******/
#include "ff.h"
#include "ff_gen_drv.h"
#include "sd_diskio.h"
#include "sdio_test.h"

/*******   Custumize LIB   *******/
#include "bsp_ili9341_lcd.h"
#include "bsp_xpt2046_lcd.h"
#include "cmdHandler.h"
#include "printerController.h"
#include "Fatfs_SDIO.h"
#include "GUI.h"
#include "LCD.h"

void setUART2HighZ();

void SystemClock_Config(void);

void MX_FREERTOS_Init(void);

uint8_t ILI9341_Read_MADCTL(void);

/**
  * @brief  主程式進入點
  * @retval int
  */
int main(void) {
	/*------------BSP HAL INIT------------*/
	HAL_Init();
	SystemClock_Config();
	MX_GPIO_Init();
	MX_DMA_Init();
	// setUART2HighZ();
	MX_USART1_UART_Init();
	MX_USART2_UART_Init();
	MX_USART3_UART_Init();
	__HAL_RCC_CRC_CLK_ENABLE();

	/*------------CUSTOMIZE FUNC INIT------------*/
	LED_GPIO_Config();
	XPT2046_Init();
	ESP32_Init();
	PC_init();
	SDIO_FatFs_init();
	// SDIO_FatFs_RW_Test();
	GUI_Init();

	osKernelInitialize();
	MX_FREERTOS_Init();
	Uart_Sync_Init();
	osKernelStart();
	while (1) {
	}
}

void setUART2HighZ() {
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = GPIO_PIN_2;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	GPIO_InitStruct.Pin = GPIO_PIN_3;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	while (HAL_GPIO_ReadPin(USER_KEY_GPIO_PORT, USER_KEY_PIN) == GPIO_PIN_RESET) {
	}
}

/**
  * @brief 系統時鐘設定
  * @retval 無
  */
void SystemClock_Config(void) {
	RCC_OscInitTypeDef RCC_OscInitStruct = {0};
	RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
	RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_ON;
	RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
	RCC_OscInitStruct.HSIState = RCC_HSI_ON;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
	RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		Error_Handler();
	}

	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
	                              | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
		Error_Handler();
	}

	PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB;
	PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_PLL_DIV1_5;
	if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
		Error_Handler();
	}
}

/* USER CODE BEGIN 4 */


/* USER CODE END 4 */

/**
  * @brief 錯誤處理函式
  * @retval 無
  */
void Error_Handler(void) {
	__disable_irq();
	printf("fault\r\n<UNK>\r\n");
	while (1) {
	}
}
#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) {
	/* 使用者可自行顯示錯誤的檔名與行號 */
}
#endif /* USE_FULL_ASSERT */
