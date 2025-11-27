/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
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

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "esp32.h"
#include "UITask.h"
#include "hx711.h"
#include "fileTask.h"
#include "printerController.h"
#include "usart.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

void init_weight(hx711_t *hx711);
float measure_weight(hx711_t *hx711);

//UI主線程
osThreadId_t UITaskHandle;
const osThreadAttr_t UI_Task_attributes = {
	.name = "UI_Task",
	.stack_size = configMINIMAL_STACK_SIZE * 20,
	.priority = (osPriority) osPriorityAboveNormal,
};

//觸控檢測線程
osThreadId_t TouchTaskHandle;
const osThreadAttr_t Touch_Task_attributes = {
	.name = "Touch_Task",
	.stack_size = configMINIMAL_STACK_SIZE * 4,
	.priority = (osPriority) osPriorityNormal,
};

//esp32 uart解析線程
osThreadId_t esp32RxHandlerTaskHandle;
const osThreadAttr_t esp32RxHandlerTask_attributes = {
	.name = "Esp32_RxTask",
	.stack_size = configMINIMAL_STACK_SIZE * 16,
	.priority = (osPriority) osPriorityHigh,
};

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
	.name = "defaultTask",
	.stack_size = configMINIMAL_STACK_SIZE * 10,
	.priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
	/* USER CODE BEGIN Init */
	// printf("%-20s %-30s free heap: %d bytes\r\n", "[freertos.c]", "rtos initing...", xPortGetFreeHeapSize());
	/* USER CODE END Init */

	/* USER CODE BEGIN RTOS_MUTEX */
	/* add mutexes, ... */
	/* USER CODE END RTOS_MUTEX */

	/* USER CODE BEGIN RTOS_SEMAPHORES */
	/* add semaphores, ... */
	/* USER CODE END RTOS_SEMAPHORES */

	/* USER CODE BEGIN RTOS_TIMERS */
	/* start timers, add new ones, ... */
	/* USER CODE END RTOS_TIMERS */

	/* USER CODE BEGIN RTOS_QUEUES */
	/* add queues, ... */
	/* USER CODE END RTOS_QUEUES */

	/* Create the thread(s) */
	/* creation of defaultTask */
	defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);
	/* USER CODE BEGIN RTOS_THREADS */
	UITaskHandle = osThreadNew(GUI_Task, NULL, &UI_Task_attributes);
	TouchTaskHandle = osThreadNew(touchTask, NULL, &Touch_Task_attributes);
	esp32RxHandlerTaskHandle = osThreadNew(ESP32_CmdHandler_Task, NULL, &esp32RxHandlerTask_attributes);
	//任務建立結果檢查
#ifdef DEBUG
	if (defaultTaskHandle == NULL) {
		printf("%-20s defaultTaskHandle created failed!!\r\n", "[freertos.c]");
	}
	if (UITaskHandle == NULL) {
		printf("%-20s UITaskHandle created failed!!\r\n", "[freertos.c]");
	}
	if (TouchTaskHandle == NULL) {
		printf("%-20s TouchTaskHandle created failed!!\r\n", "[freertos.c]");
	}
	if (esp32RxHandlerTaskHandle == NULL) {
		printf("%-20s esp32RxHandlerTaskHandle created failed!!\r\n", "[freertos.c]");
	}
#endif
	/* USER CODE END RTOS_THREADS */

	/* USER CODE BEGIN RTOS_EVENTS */
	/* add events, ... */
	/* USER CODE END RTOS_EVENTS */
	printf("%-20s %-30s free heap: %d bytes\r\n", "[freertos.c]", "tasks initialized.", xPortGetFreeHeapSize());
}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument) {
	FileTask_Init(); // 初始化檔案任務佇列
	ESP32_Init();
	Hx711_Init(&hx711);
	
	// 初始化印表機通訊 (需要在 RTOS 啟動後)
	printerRxSemaphore = xSemaphoreCreateBinary();
	if (printerRxSemaphore == NULL) {
		printf("%-20s Failed to create printerRxSemaphore\r\n", "[freertos.c]");
	}
	__HAL_UART_ENABLE_IT(&huart3, UART_IT_IDLE);

	for (;;) {
		size_t free_heap = xPortGetFreeHeapSize();
		size_t used_heap = configTOTAL_HEAP_SIZE - free_heap;
		uint8_t usage_percent = (used_heap * 100) / configTOTAL_HEAP_SIZE;
		printf("%-20s Heap usage: %u%% (%u / %u bytes)\r\n", "[freertos.c]", usage_percent, (unsigned int) used_heap,
		       (unsigned int) configTOTAL_HEAP_SIZE);
		PC_Param_Polling();
		osDelay(1000);
	}
}


/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
	/* 先打印哪個任務溢出（在禁用中斷之前） */
	printf("%-20s Stack overflow in task: %s\r\n", "[freertos.c]", pcTaskName);
	
	/* 停用中斷，避免繼續運行造成更大破壞 */
	taskDISABLE_INTERRUPTS();

	/* 可以在這裡打開 LED 快速閃爍作為錯誤提示 */
	// Error_LED_Blink();

	/* 進入死循環，等待看門狗或人工重啟 */
	for (;;);
}

/* USER CODE END Application */