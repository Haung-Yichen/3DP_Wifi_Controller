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

#include "bsp_led.h"
#include "ff.h"
#include "sdio_test.h"

#include "bsp_xpt2046_lcd.h"
#include "printerController.h"
#include "Fatfs_SDIO.h"
#include "GUI.h"
#include "hx711.h"

void setUART2HighZ();
void SystemClock_Config(void);
void MX_FREERTOS_Init(void);
uint8_t ILI9341_Read_MADCTL(void);
void Sanitize_SD_Bus(void);

#define KNOWN_WEIGHT_VALUE 910.0f
hx711_t hx711;

void perform_calibration(hx711_t *my_hx711) {

	hx711_init(my_hx711, GPIOB, GPIO_PIN_14, GPIOB, GPIO_PIN_15);
	
	// Check if HX711 is responsive - wait for LOW state with strict timeout
	printf("檢查 HX711 狀態...\r\n");
	uint32_t check_timeout = 0;
	uint32_t max_check = 5000000; // ~5 seconds
	
	while (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_15) == GPIO_PIN_SET) {
		check_timeout++;
		if (check_timeout > max_check) {
			printf("[警告] HX711 無響應！DOUT 持續為 HIGH\r\n");
			printf("DT電壓異常 (應為 0V 或 3.3V)，可能原因：\r\n");
			printf("  1) VCC/GND 未正確供電\r\n");
			printf("  2) 稱重傳感器未連接\r\n");
			printf("  3) HX711 模組損壞\r\n");
			printf("跳過校準程序，使用默認值繼續啟動...\r\n");
			// 設置默認值以便系統繼續運行
			set_scale(my_hx711, 1.0f, 1.0f);
			set_offset(my_hx711, 0, CHANNEL_A);
			return;
		}
		// Print progress every 500k iterations
		if (check_timeout % 500000 == 0) {
			printf(".");
		}
	}
	
	printf("\r\nHX711 已響應 (DOUT=LOW)，開始校準...\r\n");
	
	// 1. 初始化 Scale 為 1，避免除以零或錯誤計算
	set_scale(my_hx711, 1.0f, 1.0f);

	// 2. 歸零 (Tare) - 設定空秤時的 Offset
	// 讀取 10 次取平均，確保穩定
	printf("請清空秤盤，準備歸零...\r\n");
	HAL_Delay(2000); // 等待穩定
	tare(my_hx711, 10, CHANNEL_A);
	printf("歸零完成。Offset: %ld\r\n", my_hx711->Aoffset);

	// 3. 測量已知重量
	printf("請放上 %.2f單位的砝碼...\r\n", KNOWN_WEIGHT_VALUE);
	HAL_Delay(5000); // 給予足夠時間放上砝碼並等待數值穩定

	// 4. 獲取扣除 Offset 後的原始數值 (get_value 會回傳 Raw - Offset)
	double raw_diff = get_value(my_hx711, 10, CHANNEL_A);

	// 5. 計算比例因子 (Scale Factor)
	float new_scale = (float)(raw_diff / KNOWN_WEIGHT_VALUE);

	// 6. 設定新的 Scale
	set_scale(my_hx711, new_scale, 1.0f); // 假設只校正 Channel A

	printf("校正完成！計算出的 Scale: %f\r\n", new_scale);
	printf("現在可以使用 get_weight() 獲取準確重量。\r\n");
}

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
	// perform_calibration(&hx711);
	Sanitize_SD_Bus();
	SDIO_FatFs_init();
	GUI_Init();
	printf("%-20s GUI 初始化完成\r\n", "[main.c]");
	/*-----------------RTOS INIT-----------------*/
	osKernelInitialize();
	MX_FREERTOS_Init();
	Uart_Sync_Init();
	osKernelStart();
	while (1) {
	}
}



/* -------------------------------------------------------------------------- */
/* 函式名稱：Sanitize_SD_Bus                                                 */
/* 功能：手動重置 SD 卡匯流排，解除 Busy 鎖死與同步狀態機                      */
/* -------------------------------------------------------------------------- */
void Sanitize_SD_Bus(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	// 1. 確保 SDMMC 周邊時脈已關閉，避免硬體衝突
	// 注意：請根據您的具體型號修改 (如 __HAL_RCC_SDIO_CLK_DISABLE)
	__HAL_RCC_SDIO_CLK_DISABLE();

	// 2. 啟用 GPIO 時脈 (請依據實際板子線路修改 Port)
	__HAL_RCC_GPIOC_CLK_ENABLE(); // 假設 D0-D3 與 CK 在 Port C
	__HAL_RCC_GPIOD_CLK_ENABLE(); // 假設 CMD 在 Port D

	// 3. 將 SDMMC 引腳配置為通用推挽輸出 (Output Push-Pull)
	// CK (PC12), D0 (PC8) 為例
	GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	// CMD (PD2) 為例
	GPIO_InitStruct.Pin = GPIO_PIN_2;
	HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

	// 4. 將 CMD 與 Data 線拉高 (Idle State)
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_SET); // CMD High
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11, GPIO_PIN_SET); // Data High

	// 5. 手動翻轉 CK 引腳，產生 80 個時脈週期 (Bit-Bang)
	// 這是為了讓 SD 卡內部狀態機推進，並釋放 Busy (D0 Low) 狀態
	for(int i = 0; i < 80; i++)
	{
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_12, GPIO_PIN_SET);   // CK High
		HAL_Delay(1); // 延遲 1ms，確保頻率極低 (<400kHz)，安全第一
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_12, GPIO_PIN_RESET); // CK Low
		HAL_Delay(1);
	}

	// 6. 釋放 GPIO，將引腳重置，讓後續的 HAL_SD_Init 接手配置為 Alternate Function
	HAL_GPIO_DeInit(GPIOC, GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12);
	HAL_GPIO_DeInit(GPIOD, GPIO_PIN_2);
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
