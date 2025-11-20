#include "esp32.h"
#include <stdlib.h>
#include "cmsis_os2.h"
#include "fileTask.h"
#include "cmdList.h"
#include "usart.h"


#define ESP32_OK				 "ok\n"              //用於與esp32同步狀態
#define ESP32_DISCONNECTED		 "wifi disconnected" //esp32 wifi異常會發送
#define ESP32_OVER				 0                   //用於檢查是否收到CMD_Transmisson_Over
#define CMD_BUF_SIZE		     100
#define WAIT_ESP32_READY_TIMEOUT 10                  //最大等待ESP32初始化時間


QueueHandle_t xCmdQueue = NULL;
StaticQueue_t cmdQueue_s;
uint8_t cmdQueueArea[10 * CMD_BUF_SIZE];
static ESP32_STATE_TypeDef currentState = ESP32_INIT;

void ESP32_Init(void) {
	Uart_Rx_Pool_Init();

	ESP32_SetState(ESP32_INIT);
	memset(pCurrentRxBuf->data, 0, UART_RX_BUFFER_SIZE);
	HAL_UART_Receive_DMA(&ESP32_USART_PORT, (uint8_t*)pCurrentRxBuf->data, sizeof(pCurrentRxBuf->data));
	__HAL_UART_ENABLE_IT(&ESP32_USART_PORT, UART_IT_IDLE);
	ESP32_RegCallback();
}

void ESP32_RegCallback(void) {
	register_command(CMD_WIFI_STATUS, WifiStatusHandler);
	register_command(CMD_Start_Transmisson, StartTransmissionHandler);
	register_command(CMD_Transmisson_Over, TransmissionOverHandler);
	register_command(CMD_SET_FILENAME, SetFileNameHandler);
}

ESP32_STATE_TypeDef ESP32_GetState(void) {
	return currentState;
}

void ESP32_SetState(ESP32_STATE_TypeDef state) {
	currentState = state;
}

void ESP32_CmdHandler_Task(void *argument) {
	char _cmdBuf[CMD_BUF_SIZE] = {0};
	static ResStruct_t resStruct; //回調函數返回結構體
	xCmdQueue = xQueueCreateStatic(10,
	                               CMD_BUF_SIZE,
	                               cmdQueueArea,
	                               &cmdQueue_s);
	if (xCmdQueue == NULL) {
		printf("%-20s rxTask init failed!\r\n", "[esp32.c]");
		Error_Handler();
	}

	while (1) {
		if (xQueueReceive(xCmdQueue, _cmdBuf, pdMS_TO_TICKS(1000))) {
			_cmdBuf[CMD_BUF_SIZE - 1] = '\0';
			if (isValidCmd(_cmdBuf) == CMD_OK) {
				execute_command(_cmdBuf, isReqCmd(_cmdBuf) ? &resStruct : NULL);
				if (strlen(resStruct.resBuf) != 0) {
					UART_SendString_DMA(&ESP32_USART_PORT, resStruct.resBuf);
					memset(resStruct.resBuf, 0, RESBUF_SIZE);
				}
			} else {
				printf("%-20s unvalid cmd\r\n", "[esp32.c]");
			}
			memset(_cmdBuf, 0, CMD_BUF_SIZE);
		} else {
			// 監控stack用量
			// printf("%-20s stack high water mark: %d\r\n", "[esp32.c]", uxTaskGetStackHighWaterMark(NULL));
		}
	}
}

void WifiStatusHandler(const char *args, ResStruct_t *_resStruct) {
	char wifiStatus[20] = {0};
	char ip[20] = {0};

	extract_parameter(args, wifiStatus, 20);
	if (wifiStatus[0] == '1') {
		strncpy(ip, wifiStatus + 1, 20);
		printf("%-20s Wifi connected @ %s\r\n", "[esp32.c]", ip);
		ESP32_SetState(ESP32_IDLE);
	} else {
		printf("%-20s Wifi disconnected\r\n", "[esp32.c]");
		ESP32_SetState(ESP32_INIT);
	}
}

void StartTransmissionHandler(const char *args, ResStruct_t *_resStruct) {
	ESP32_SetState(ESP32_BUSY);
	vTaskDelay(ESP32_RECV_DELAY);
	UART_SendString_DMA(&ESP32_USART_PORT, "STM ok\n");
}

char hashVal[SHA256_HASH_SIZE]; // 傳址給檔案接收任務

void SetFileNameHandler(const char *args, ResStruct_t *_resStruct) {
	curFileName[FILENAME_SIZE - 1] = '\0';

	if (false == extract_parameter(args, curFileName, FILENAME_SIZE)) {
		ESP32_SetState(ESP32_IDLE);
		printf("%-20s Invalid filename format\r\n", "[esp32.c]");
		return;
	}
	printf("%-20s %-30s %s\r\n", "[esp32.c]", "received file name :", curFileName);
	printf("%-20s %-30s free heap: %d bytes \r\n",
	       "[esp32.c]",
	       "ready to creat Gcode task",
	       xPortGetFreeHeapSize());

	gcodeRxTaskHandle = osThreadNew(Gcode_RxHandler_Task, hashVal, &gcodeTask_attributes);
	if (gcodeRxTaskHandle == NULL) {
		ESP32_SetState(ESP32_IDLE);
		printf("%-20s Error creating gcode task\r\n", "[esp32.c]");
	}
}

/**
 * @note 暫時改用接收與上傳行數驗證
 */
void TransmissionOverHandler(const char *args, ResStruct_t *_resStruct) {
	delete = true;
	uartRxBuf_TypeDef *pNullBuf = NULL;
	
	// 檢查佇列是否存在，避免在任務已因錯誤結束後存取空指標
	if (xFileQueue != NULL) {
		xQueueSend(xFileQueue, &pNullBuf, pdMS_TO_TICKS(10));
	}

	vTaskDelay(pdMS_TO_TICKS(10));
	ESP32_SetState(ESP32_IDLE);
	UART_SendString_DMA(&ESP32_USART_PORT, ESP32_OK);

	// 等待任務終止
	if (gcodeRxTaskHandle != NULL) {
		for (int i = 0; i < 1000; i++) {
			if (gcodeRxTaskHandle == NULL) {
				break;
			}
			vTaskDelay(pdMS_TO_TICKS(10));
		}
	}
	char srcHash[70] = {0};
	extract_parameter(args, srcHash, SHA256_HASH_SIZE);
	hashVal[SHA256_HASH_SIZE - 1] = '\0';

	if (strcmp(srcHash, hashVal) == 0) {
		printf("%-20s File %s verification succeeded\r\n", "[esp32.c]", curFileName);
	} else {
		// 驗證不過再驗證一次 再不對叫網頁重發一次
		memset(hashVal, 0, SHA256_HASH_SIZE);
		calFileHash(hashVal);
		if (strcmp(srcHash, hashVal) == 0) {
			printf("%-20s File %s reverification succeeded\r\n", "[esp32.c]", curFileName);
		} else {
			printf("%-20s File %s verification failed\r\n", "[esp32.c]", curFileName);
			// sendString_to_Esp32(ERROR_FILE_BROKEN);
		}
	}
	printf("%-20s \r\n======================TransMission Successed=====================\r\n", "[esp32.c]");
	ESP32_SetState(ESP32_IDLE);
	sendString_to_Esp32(ESP32_OK);
}

HAL_StatusTypeDef sendString_to_Esp32(const char *txBuf) {
	// if (HAL_UART_GetState(&ESP32_USART_PORT) == HAL_UART_STATE_BUSY_TX) {
	// 	return HAL_BUSY;
	// }
	HAL_StatusTypeDef hal_status = HAL_UART_Transmit(&ESP32_USART_PORT,
	                                                 (uint8_t *) txBuf,
	                                                 strlen(txBuf),
	                                                 HAL_TIMEOUT);
	if (hal_status != HAL_OK) {
		printf("%-20s failed to transmit %s\r\n", "[esp32.c]", txBuf);
	}
	return hal_status;
}
