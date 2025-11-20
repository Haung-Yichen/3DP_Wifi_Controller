/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usart.h
  * @brief   This file contains all the function prototypes for
  *          the usart.c file
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
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USART_H__
#define __USART_H__

#include "main.h"
#include "esp32.h"


extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart3;
extern DMA_HandleTypeDef hdma_usart1_tx;
extern DMA_HandleTypeDef hdma_usart1_rx;
extern DMA_HandleTypeDef hdma_usart2_rx;
extern DMA_HandleTypeDef hdma_usart2_tx;
extern DMA_HandleTypeDef hdma_usart3_tx;
extern DMA_HandleTypeDef hdma_usart3_rx;


#define DEBUG_USART_PORT            huart1
#define ESP32_USART_PORT            huart2
#define PRINTING_USART_PORT         huart3
#define DEBUG_USART_BPS             1000000
#define ESP32_USART_BPS             1000000
#define PRINTER_USART_BPS           250000
#define UART_RX_BUFFER_SIZE			2048
#define RX_BUFFER_POOL_SIZE         6


void MX_USART1_UART_Init(void);

void MX_USART2_UART_Init(void);

void MX_USART3_UART_Init(void);


typedef struct {
	char data[UART_RX_BUFFER_SIZE];
	uint16_t len;
} __attribute__((packed)) uartRxBuf_TypeDef;

// extern uartRxBuf_TypeDef uartRxBuf;
extern uartRxBuf_TypeDef *pCurrentRxBuf;
extern QueueHandle_t xFreeBufferQueue;

/**
 * @brief (公共 API) 執行緒安全的非阻塞(Non-Blocking) DMA 傳輸字串
 * @note  這是 _UART_SendBuffer_DMA_NonBlocking 的字串包裝函式。
 */
HAL_StatusTypeDef UART_SendString_DMA(UART_HandleTypeDef *huart, const char *str);

/**
 * @brief 初始化Uart同步機制
 */
void Uart_Sync_Init(void);

/**
 * @brief 初始化接收緩衝區池
 */
void Uart_Rx_Pool_Init(void);

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __USART_H__ */
