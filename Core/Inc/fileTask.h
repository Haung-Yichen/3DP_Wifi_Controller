#ifndef FILETASK_H
#define FILETASK_H

#include <stdbool.h>
#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "ff.h"
#include "sha256.h"
#include "esp32.h"

#define FILENAME_SIZE			 _MAX_LFN
#define SHA256_HASH_SIZE         70


extern osThreadId_t gcodeRxTaskHandle;
extern const osThreadAttr_t gcodeTask_attributes;
extern QueueHandle_t xFileQueue;

extern char curFileName[FILENAME_SIZE];
extern volatile bool delete;
extern volatile bool isTransmittimg;


/**
 * @brief 檔案接收任務，接收來自ESP32 UART的GCODE
 *		  並存入SD卡，須按照ESP32的狀態來接收
 * @param argument
 */
void Gcode_RxHandler_Task(void *argument);

/**
 * @brief 計算檔案sha256哈希值
 * @param hashOutput
 * @return
 */
void calFileHash(char* hashOutput);


#endif //FILETASK_H
