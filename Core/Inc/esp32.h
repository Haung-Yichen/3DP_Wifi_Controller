/**
 * @file    esp32.h
 * @author  HAUNG YI CHEN
 * @date    2025/06/24
 * @brief   該模組負責處理解析ESP32傳來的資料
 *		    並實作所有命令的回調函數，回調函數
 *		    再去呼叫真正執行該邏輯的函數。
 */

#ifndef _ESP32_H_
#define _ESP32_H_

#include <stdint.h>
#include <string.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include "cmdHandler.h"
#include "fileTask.h"


#define ESP32_RECV_DELAY         100


typedef enum {
	ESP32_INIT = 0,
	ESP32_IDLE,
	ESP32_BUSY
} ESP32_STATE_TypeDef;

extern QueueHandle_t xCmdQueue;


/**
 * @brief 初始化 ESP32 模組，註冊回調函數，並等待直到esp32回報初始化完成
 */
void ESP32_Init(void);

/**
 * @brief 註冊回調函數
 */
void ESP32_RegCallback(void);

/**
 * @brief 傳回目前 ESP32 狀態
 */
ESP32_STATE_TypeDef ESP32_GetState(void);

/**
 * @brief 設定 ESP32 狀態
 */
void ESP32_SetState(ESP32_STATE_TypeDef state);

/**
 * @brief 解析UART資料，DMA中斷觸發
 */
void ESP32_CmdHandler_Task(void *argument);

/************************************************
*                 定義回調函數                  *
************************************************/

/**
 * @brief 命令 : 處理esp32 wifi狀態
 */
void WifiStatusHandler(const char *args, ResStruct_t *_resStruct);

/**
 * @brief 命令 : 準備接收dcode
 */
void StartTransmissionHandler(const char *args, ResStruct_t *_resStruct);

/**
 * @brief 命令：傳輸結束（command 觸發器）
 */
void TransmissionOverHandler(const char *args, ResStruct_t *_resStruct);

/**
 * @brief 命令： 設定檔名
 */
void SetFileNameHandler(const char *args, ResStruct_t *_resStruct);

HAL_StatusTypeDef sendString_to_Esp32(const char *txBuf);

#endif /* _ESP32_H_ */
