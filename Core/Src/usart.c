/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usart.c
  * @brief   This file provides code for the configuration
  * of the USART instances.
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "usart.h"
#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h" // 為了 xTaskGetSchedulerState()
#include "portmacro.h"

#define UART_TX_BUFFER_SIZE 128
#define UART_COUNT 3

typedef struct {
	UART_HandleTypeDef *huart;
	SemaphoreHandle_t mutex;
	uint8_t txBuf[UART_TX_BUFFER_SIZE];
} UartSync_t;

static UartSync_t gUartSync[UART_COUNT];
// uartRxBuf_TypeDef uartRxBuf;
uartRxBuf_TypeDef rxBufPool[RX_BUFFER_POOL_SIZE];
uartRxBuf_TypeDef *pCurrentRxBuf;
QueueHandle_t xFreeBufferQueue;

// 核心 DMA 傳輸函式 (內部使用)
static HAL_StatusTypeDef _UART_SendBuffer_DMA(UART_HandleTypeDef *huart, const uint8_t *data, size_t len);


UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;
DMA_HandleTypeDef hdma_usart1_tx;
DMA_HandleTypeDef hdma_usart1_rx;
DMA_HandleTypeDef hdma_usart2_tx;
DMA_HandleTypeDef hdma_usart2_rx;
DMA_HandleTypeDef hdma_usart3_tx;
DMA_HandleTypeDef hdma_usart3_rx;


/**
 * @brief 獲取 UART 同步對象的輔助函式
 */
static UartSync_t *_get_sync_handle(UART_HandleTypeDef *huart) {
	if (huart->Instance == USART1) {
		return &gUartSync[0];
	} else if (huart->Instance == USART2) {
		return &gUartSync[1];
	} else if (huart->Instance == USART3) {
		return &gUartSync[2];
	}
	return NULL;
}


void Uart_Sync_Init(void) {
	gUartSync[0].huart = &huart1;
	gUartSync[1].huart = &huart2;
	gUartSync[2].huart = &huart3;

	for (int i = 0; i < UART_COUNT; i++) {
		gUartSync[i].mutex = xSemaphoreCreateBinary();
		if (gUartSync[i].mutex == NULL) {
			printf("%-20s UART%d Sync Init Failed!\r\n", "usart.c", i + 1);
			// Error_Handler();
		} else {
			xSemaphoreGive(gUartSync[i].mutex);
		}
	}

	printf("%-20s uart thread safe inited.\r\n", "usart.c");
}

void Uart_Rx_Pool_Init(void) {
	xFreeBufferQueue = xQueueCreate(RX_BUFFER_POOL_SIZE, sizeof(uartRxBuf_TypeDef *));
	if (xFreeBufferQueue == NULL) {
		printf("%-20s FreeBufferQueue Init Failed!\r\n", "usart.c");
		Error_Handler();
	}

	// 將緩衝區加入池中 (保留第一個給初始接收使用)
	for (int i = 1; i < RX_BUFFER_POOL_SIZE; i++) {
		uartRxBuf_TypeDef *pBuf = &rxBufPool[i];
		xQueueSend(xFreeBufferQueue, &pBuf, 0);
	}

	pCurrentRxBuf = &rxBufPool[0];
	printf("%-20s Rx Pool inited.\r\n", "usart.c");
}

/* USART1 init function */
void MX_USART1_UART_Init(void) {
	huart1.Instance = USART1;
	huart1.Init.BaudRate = DEBUG_USART_BPS;
	huart1.Init.WordLength = UART_WORDLENGTH_8B;
	huart1.Init.StopBits = UART_STOPBITS_1;
	huart1.Init.Parity = UART_PARITY_NONE;
	huart1.Init.Mode = UART_MODE_TX_RX;
	huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart1.Init.OverSampling = UART_OVERSAMPLING_16;
	if (HAL_UART_Init(&huart1) != HAL_OK) {
		Error_Handler();
	}
}

/* USART2 init function */
void MX_USART2_UART_Init(void) {
	huart2.Instance = USART2;
	huart2.Init.BaudRate = ESP32_USART_BPS;
	huart2.Init.WordLength = UART_WORDLENGTH_8B;
	huart2.Init.StopBits = UART_STOPBITS_1;
	huart2.Init.Parity = UART_PARITY_NONE;
	huart2.Init.Mode = UART_MODE_TX_RX;
	huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart2.Init.OverSampling = UART_OVERSAMPLING_16;
	if (HAL_UART_Init(&huart2) != HAL_OK) {
		Error_Handler();
	}
}

/* USART3 init function */
void MX_USART3_UART_Init(void) {
	huart3.Instance = USART3;
	huart3.Init.BaudRate = PRINTER_USART_BPS;
	huart3.Init.WordLength = UART_WORDLENGTH_8B;
	huart3.Init.StopBits = UART_STOPBITS_1;
	huart3.Init.Parity = UART_PARITY_NONE;
	huart3.Init.Mode = UART_MODE_TX_RX;
	huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart3.Init.OverSampling = UART_OVERSAMPLING_16;
	if (HAL_UART_Init(&huart3) != HAL_OK) {
		Error_Handler();
	}
}

void HAL_UART_MspInit(UART_HandleTypeDef *uartHandle) {
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	if (uartHandle->Instance == USART1) {
		/* USER CODE BEGIN USART1_MspInit 0 */

		/* USER CODE END USART1_MspInit 0 */
		/* Peripheral clock enable */
		__HAL_RCC_USART1_CLK_ENABLE();
		__HAL_RCC_GPIOA_CLK_ENABLE();
		/**USART1 GPIO Configuration
		PA9     ------> USART1_TX
		PA10     ------> USART1_RX
		*/
		GPIO_InitStruct.Pin = GPIO_PIN_9;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
		HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

		GPIO_InitStruct.Pin = GPIO_PIN_10;
		GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

		/* USART1 DMA Init */
		/* USART1_TX Init */
		hdma_usart1_tx.Instance = DMA1_Channel4;
		hdma_usart1_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
		hdma_usart1_tx.Init.PeriphInc = DMA_PINC_DISABLE;
		hdma_usart1_tx.Init.MemInc = DMA_MINC_ENABLE;
		hdma_usart1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
		hdma_usart1_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
		hdma_usart1_tx.Init.Mode = DMA_NORMAL;
		hdma_usart1_tx.Init.Priority = DMA_PRIORITY_HIGH;
		if (HAL_DMA_Init(&hdma_usart1_tx) != HAL_OK) {
			Error_Handler();
		}
		__HAL_LINKDMA(uartHandle, hdmatx, hdma_usart1_tx);

		/* USART1_RX Init */
		hdma_usart1_rx.Instance = DMA1_Channel5;
		hdma_usart1_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
		hdma_usart1_rx.Init.PeriphInc = DMA_PINC_DISABLE;
		hdma_usart1_rx.Init.MemInc = DMA_MINC_ENABLE;
		hdma_usart1_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
		hdma_usart1_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
		hdma_usart1_rx.Init.Mode = DMA_NORMAL;
		hdma_usart1_rx.Init.Priority = DMA_PRIORITY_HIGH;
		if (HAL_DMA_Init(&hdma_usart1_rx) != HAL_OK) {
			Error_Handler();
		}
		__HAL_LINKDMA(uartHandle, hdmarx, hdma_usart1_rx);

		/* USART1 interrupt Init */
		HAL_NVIC_SetPriority(USART1_IRQn, 6, 0);
		HAL_NVIC_EnableIRQ(USART1_IRQn);
		/* USER CODE BEGIN USART1_MspInit 1 */

		/* USER CODE END USART1_MspInit 1 */
	} else if (uartHandle->Instance == USART2) {
		/* USER CODE BEGIN USART2_MspInit 0 */

		/* USER CODE END USART2_MspInit 0 */
		/* Peripheral clock enable */
		__HAL_RCC_USART2_CLK_ENABLE();
		__HAL_RCC_GPIOA_CLK_ENABLE();
		/**USART2 GPIO Configuration
		PA2     ------> USART2_TX
		PA3     ------> USART2_RX
		*/
		GPIO_InitStruct.Pin = GPIO_PIN_2;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
		HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

		GPIO_InitStruct.Pin = GPIO_PIN_3;
		GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

		/* USART2 DMA Init */
		/* USART2_TX Init */
		hdma_usart2_tx.Instance = DMA1_Channel7;
		hdma_usart2_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
		hdma_usart2_tx.Init.PeriphInc = DMA_PINC_DISABLE;
		hdma_usart2_tx.Init.MemInc = DMA_MINC_ENABLE;
		hdma_usart2_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
		hdma_usart2_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
		hdma_usart2_tx.Init.Mode = DMA_NORMAL;
		hdma_usart2_tx.Init.Priority = DMA_PRIORITY_HIGH;
		if (HAL_DMA_Init(&hdma_usart2_tx) != HAL_OK) {
			Error_Handler();
		}
		__HAL_LINKDMA(uartHandle, hdmatx, hdma_usart2_tx);

		/* USART2_RX Init */
		hdma_usart2_rx.Instance = DMA1_Channel6;
		hdma_usart2_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
		hdma_usart2_rx.Init.PeriphInc = DMA_PINC_DISABLE;
		hdma_usart2_rx.Init.MemInc = DMA_MINC_ENABLE;
		hdma_usart2_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
		hdma_usart2_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
		hdma_usart2_rx.Init.Mode = DMA_NORMAL;
		hdma_usart2_rx.Init.Priority = DMA_PRIORITY_HIGH;
		if (HAL_DMA_Init(&hdma_usart2_rx) != HAL_OK) {
			Error_Handler();
		}
		__HAL_LINKDMA(uartHandle, hdmarx, hdma_usart2_rx);

		/* USART2 interrupt Init */
		HAL_NVIC_SetPriority(USART2_IRQn, 5, 0);
		HAL_NVIC_EnableIRQ(USART2_IRQn);
		/* USER CODE BEGIN USART2_MspInit 1 */

		/* USER CODE END USART2_MspInit 1 */
	} else if (uartHandle->Instance == USART3) {
		/* USER CODE BEGIN USART3_MspInit 0 */

		/* USER CODE END USART3_MspInit 0 */
		/* Peripheral clock enable */
		__HAL_RCC_USART3_CLK_ENABLE();
		__HAL_RCC_GPIOB_CLK_ENABLE();
		/**USART3 GPIO Configuration
		PB10     ------> USART3_TX
		PB11     ------> USART3_RX
		*/
		GPIO_InitStruct.Pin = GPIO_PIN_10;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
		HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

		GPIO_InitStruct.Pin = GPIO_PIN_11;
		GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

		/* USART3 DMA Init */
		/* USART3_TX Init */
		hdma_usart3_tx.Instance = DMA1_Channel2;
		hdma_usart3_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
		hdma_usart3_tx.Init.PeriphInc = DMA_PINC_DISABLE;
		hdma_usart3_tx.Init.MemInc = DMA_MINC_ENABLE;
		hdma_usart3_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
		hdma_usart3_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
		hdma_usart3_tx.Init.Mode = DMA_NORMAL;
		hdma_usart3_tx.Init.Priority = DMA_PRIORITY_HIGH;
		if (HAL_DMA_Init(&hdma_usart3_tx) != HAL_OK) {
			Error_Handler();
		}
		__HAL_LINKDMA(uartHandle, hdmatx, hdma_usart3_tx);

		/* USART3_RX Init */
		hdma_usart3_rx.Instance = DMA1_Channel3;
		hdma_usart3_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
		hdma_usart3_rx.Init.PeriphInc = DMA_PINC_DISABLE;
		hdma_usart3_rx.Init.MemInc = DMA_MINC_ENABLE;
		hdma_usart3_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
		hdma_usart3_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
		hdma_usart3_rx.Init.Mode = DMA_NORMAL;
		hdma_usart3_rx.Init.Priority = DMA_PRIORITY_HIGH;
		if (HAL_DMA_Init(&hdma_usart3_rx) != HAL_OK) {
			Error_Handler();
		}
		__HAL_LINKDMA(uartHandle, hdmarx, hdma_usart3_rx);


		/* USART3 interrupt Init */
		HAL_NVIC_SetPriority(USART3_IRQn, 6, 0);
		HAL_NVIC_EnableIRQ(USART3_IRQn);
		/* USER CODE BEGIN USART3_MspInit 1 */

		/* USER CODE END USART3_MspInit 1 */
	}
}

void HAL_UART_MspDeInit(UART_HandleTypeDef *uartHandle) {
	if (uartHandle->Instance == USART1) {
		/* USER CODE BEGIN USART1_MspDeInit 0 */

		/* USER CODE END USART1_MspDeInit 0 */
		__HAL_RCC_USART1_CLK_DISABLE();
		HAL_GPIO_DeInit(GPIOA, GPIO_PIN_9 | GPIO_PIN_10);
		HAL_DMA_DeInit(uartHandle->hdmatx);
		HAL_DMA_DeInit(uartHandle->hdmarx);
		HAL_NVIC_DisableIRQ(USART1_IRQn);
		/* USER CODE BEGIN USART1_MspDeInit 1 */

		/* USER CODE END USART1_MspDeInit 1 */
	} else if (uartHandle->Instance == USART2) {
		/* USER CODE BEGIN USART2_MspDeInit 0 */

		/* USER CODE END USART2_MspDeInit 0 */
		__HAL_RCC_USART2_CLK_DISABLE();
		HAL_GPIO_DeInit(GPIOA, GPIO_PIN_2 | GPIO_PIN_3);
		HAL_DMA_DeInit(uartHandle->hdmatx);
		HAL_DMA_DeInit(uartHandle->hdmarx);
		HAL_NVIC_DisableIRQ(USART2_IRQn);
		/* USER CODE BEGIN USART2_MspDeInit 1 */

		/* USER CODE END USART2_MspDeInit 1 */
	} else if (uartHandle->Instance == USART3) {
		/* USER CODE BEGIN USART3_MspDeInit 0 */

		/* USER CODE END USART3_MspDeInit 0 */
		__HAL_RCC_USART3_CLK_DISABLE();
		HAL_GPIO_DeInit(GPIOB, GPIO_PIN_10 | GPIO_PIN_11);
		HAL_DMA_DeInit(uartHandle->hdmatx);
		HAL_DMA_DeInit(uartHandle->hdmarx);
		HAL_NVIC_DisableIRQ(USART3_IRQn);
		/* USER CODE BEGIN USART3_MspDeInit 1 */

		/* USER CODE END USART3_MspDeInit 1 */
	}
}

/**
  * @brief  Tx Transfer completed callback.
  * @note   DMA 傳輸完成後，此中斷被觸發，負責釋放 Mutex。
  */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;

	UartSync_t *pSync = _get_sync_handle(huart);
	if (pSync == NULL) {
		return;
	}

	xSemaphoreGiveFromISR(pSync->mutex, &xHigherPriorityTaskWoken);
	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* USER CODE BEGIN 1 */

/**
 * @brief 核心 DMA 傳輸函式 (非阻塞)
 * @note  這是唯一能存取 Mutex 和啟動 DMA 的函式。
 */
static HAL_StatusTypeDef _UART_SendBuffer_DMA(UART_HandleTypeDef *huart, const uint8_t *data, size_t len) {
	HAL_StatusTypeDef dmaStatus = HAL_ERROR;

	if (data == NULL || len == 0) {
		return HAL_OK;
	}

	if (len >= UART_TX_BUFFER_SIZE) {
		printf("_UART_SendBuffer_DMA: Data too long!\r\n");
		return HAL_ERROR;
	}

	UartSync_t *pSync = _get_sync_handle(huart);
	if (pSync == NULL || pSync->mutex == NULL) {
		return HAL_ERROR;
	}

	if (xSemaphoreTake(pSync->mutex, portMAX_DELAY) == pdTRUE) {
		// 複製資料到內部緩衝區，防止呼叫端緩衝區失效
		memcpy((char *) pSync->txBuf, data, len);

		dmaStatus = HAL_UART_Transmit_DMA(pSync->huart, (uint8_t *) pSync->txBuf, len);

		if (dmaStatus != HAL_OK) {
			// DMA 啟動失敗，必須立即釋放鎖，否則死鎖
			xSemaphoreGive(pSync->mutex);
			printf("_UART_SendBuffer_DMA: HAL_UART_Transmit_DMA Failed!\r\n");
		}
		// 成功：Mutex 將由 HAL_UART_TxCpltCallback 釋放
	} else {
		dmaStatus = HAL_ERROR;
	}

	return dmaStatus;
}


/**
 * @brief 重定向 printf 到 USART1
 * @note  【架構核心】
 * 判斷 RTOS 內核是否運行：
 * 1. 運行前：使用阻塞式 HAL_UART_Transmit，此時為單執行緒，安全。
 * 2. 運行後：使用非阻塞 DMA 函式，與 G-code 等傳輸安全排隊，解決死鎖。
 */
int _write(int fd, char *ptr, int len) {
	// 假設 UART1 (huart1) 是調試端口

	if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
		// RTOS 運行中：使用執行緒安全的 DMA 函式
		_UART_SendBuffer_DMA(&huart1, (const uint8_t *) ptr, len);
	} else {
		// RTOS 啟動前：使用阻塞式傳輸
		HAL_UART_Transmit(&huart1, (uint8_t *) ptr, len, HAL_MAX_DELAY);
	}
	return len;
}

/**
 * @brief (公共 API) 執行緒安全的非阻塞(Non-Blocking) DMA 傳輸字串
 * @note  這是 _UART_SendBuffer_DMA 的字串包裝函式。
 */
HAL_StatusTypeDef UART_SendString_DMA(UART_HandleTypeDef *huart, const char *str) {
	if (str == NULL) {
		return HAL_ERROR;
	}

	size_t len = strlen(str);
	return _UART_SendBuffer_DMA(huart, (const uint8_t *) str, len);
}

/* USER CODE END 1 */
