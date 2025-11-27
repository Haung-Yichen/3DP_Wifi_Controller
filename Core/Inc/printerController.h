/*********************************************************************
 * @file   printer_controller.h
 * @brief  印表機控制器
 * 為本項目核心模組，負責與印表機通訊，接收來自電腦端與esp32的命令，
 * 以及emWin GUI。每隔一段時間會主動獲取傳感器狀態，並提供接口讓外部訪問。
 *
 * @author Lin, YiChen
 * @date   2024.12.08
 *********************************************************************/

#ifndef _PRINTER_CONTROLLER_H_
#define _PRINTER_CONTROLLER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
#include <stdint.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include "cmdHandler.h"
#include "cmsis_os2.h"


extern SemaphoreHandle_t priOverSemaphore; //列印完成信號量
extern osThreadId_t pcTaskHandle;
extern const osThreadAttr_t pcTask_attributes;

/*--------印表機狀態---------*/
typedef enum {
	PC_INIT = 0,
	PC_IDLE,
	PC_BUSY,
	PC_ERROR
} PC_Status_TypeDef;

// 用於 UART3 中斷回調
extern SemaphoreHandle_t printerRxSemaphore;
extern volatile bool printerOkReceived;
#define PRINTER_RX_BUF_SIZE 256  // 增大緩衝區以容納多行回應
extern char printerRxBuf[PRINTER_RX_BUF_SIZE];
extern volatile uint16_t printerRxLen;

void PC_init(void);

/**
 * @brief 註冊回調函數
 */
void PC_RegCallback(void);

void PC_Print_Task(void *argument);

PC_Status_TypeDef PC_GetState(void);

/**
 * @brief 在defaultTask中輪詢，只在網頁斷線時輪詢
 */
void PC_SetState(PC_Status_TypeDef state);

/**
 * @brief 負責更新參數
 */
void PC_Param_Polling(void);

/**
 * @brief 查詢印表機溫度 (在背景任務中呼叫，會阻塞)
 */
void PC_QueryTemperature(void);

/**
 * @brief 查詢耗材重量 (在背景任務中呼叫，會阻塞)
 */
void PC_QueryFilamentWeight(void);

/************************************************
*                 定義回調函數                  *
************************************************/

/**
 * @brief 開始列印命令的處理函式
 */
void StartToPrintHandler(const char *args, ResStruct_t *_resStruct);

/**
 * @brief 暫停列印命令的處理函式
 */
void PausePrintingHandler(const char *args, ResStruct_t *_resStruct);

/**
 * @brief 停止列印命令的處理函式
 */
void StopPrintingHandler(const char *args, ResStruct_t *_resStruct);

/**
 * @brief 回到原點命令的處理函式
 */
void GoHomeHandler(const char *args, ResStruct_t *_resStruct);

/**
 * @brief 請求剩餘列印時間命令的處理函式
 */
void GetRemainingTimeHandler(const char *args, ResStruct_t *_resStruct);

/**
 * @brief 請求列印進度命令的處理函式
 */
void GetProgressHandler(const char *args, ResStruct_t *_resStruct);

/**
 * @brief 請求噴嘴溫度命令的處理函式
 */
void GetNozzleTempHandler(const char *args, ResStruct_t *_resStruct);

/**
 * @brief 請求熱床溫度命令的處理函式
 */
void GetBedTempHandler(const char *args, ResStruct_t *_resStruct);

/**
 * @brief 設置噴嘴溫度命令的處理函式
 */
void SetNozzleTempHandler(const char *args, ResStruct_t *_resStruct);

/**
 * @brief 設置熱床溫度命令的處理函式
 */
void SetBedTempHandler(const char *args, ResStruct_t *_resStruct);

/**
 * @brief 請求耗材重量命令的處理函式
 */
void GetFilamentWeightHandler(const char *args, ResStruct_t *_resStruct);

/**
 * @brief 緊急停止命令的處理函式
 */
void EmergencyStopHandler(const char *args, ResStruct_t *_resStruct);

/**
 * @brief 獲取SD卡所有檔案命令的處理函式
 */
void GetAllFilesHandler(const char *args, ResStruct_t *_resStruct);


#ifdef __cplusplus
}
#endif

#endif /* _PRINTER_CONTROLLER_H_ */
