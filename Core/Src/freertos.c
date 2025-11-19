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
#include "ledTask.h"
#include "esp32.h"
#include "gpio.h"
#include "UITask.h"
#include "hx711.h"
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
	.priority = (osPriority) osPriorityNormal7,
};

//esp32 uart解析線程
osThreadId_t esp32RxHandlerTaskHandle;
const osThreadAttr_t esp32RxHandlerTask_attributes = {
	.name = "Esp32_RxTask",
	.stack_size = configMINIMAL_STACK_SIZE * 12,
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
	hx711_t hx711;
	float weight;

	init_weight(&hx711);

	for (;;) {
		weight = measure_weight(&hx711);
		printf("%-20s Weight: %d\r\n", "[hx711]", (size_t)(weight/1000));

		size_t free_heap = xPortGetFreeHeapSize();
		size_t used_heap = configTOTAL_HEAP_SIZE - free_heap;
		uint8_t usage_percent = (used_heap * 100) / configTOTAL_HEAP_SIZE;

		printf("%-20s Heap usage: %u%% (%u / %u bytes)\r\n", "[freertos.c]", usage_percent, (unsigned int) used_heap,
		       (unsigned int) configTOTAL_HEAP_SIZE);
		osDelay(500);
	}
}


/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
	/* 停用中斷，避免繼續運行造成更大破壞 */
	taskDISABLE_INTERRUPTS();

	/* 打印哪個任務溢出（需串口或日誌支持） */
	printf("%-20s Stack overflow in task: %s\n", pcTaskName);

	/* 可以在這裡打開 LED 快速閃爍作為錯誤提示 */
	// Error_LED_Blink();

	/* 進入死循環，等待看門狗或人工重啟 */
	for (;;);
}

/* USER CODE END Application */

/**
 * @brief  Weight Initialization Function
 * @retval None
 */
void init_weight(hx711_t *hx711){
	printf("%-20s HX711 initialization\r\n", "[hx711]");

	/* Initialize the hx711 sensors */
	hx711_init(hx711, GPIOB, GPIO_PIN_14, GPIOB, GPIO_PIN_15);

	/* Configure gain for each channel (see datasheet for details) */
	set_gain(hx711, 128, 32);

	/* Set HX711 scaling factor (see README for procedure) */
	set_scale(hx711, -44.25, -10.98);

	/* Tare weight */
	tare_all(hx711, 10);

	printf("%-20s HX711 module has been initialized\r\n", "[hx711]");
}

/**
 * @brief  讀取重量 (改良版)
 * @param  hx711: HX711 結構體指標
 * @return float: Channel A 的重量 (假設主要測量 A 通道)
 */
float measure_weight(hx711_t *hx711) { // 改為傳遞指標
	double weightA = 0;
	double weightB = 0;

	// 讀取 Channel A (使用驅動提供的 get_value 或 get_units)
	// 注意: 原驅動 get_weight 回傳的是扣除皮重後的值
	weightA = get_value(hx711, 1, CHANNEL_A);

	// 如果需要防止負數
	// weightA = (weightA < 0) ? 0 : weightA;

	/* 如果您沒有接 Channel B 的傳感器，建議註解掉下面這段以節省時間 */
	// weightB = get_value(hx711, 1, CHANNEL_B);

	// 這邊演示如何將 float 轉為較易閱讀的格式 (若需要 Debug)
	// printf("[HX711] Raw A: %.2f\r\n", weightA);

	return (float)weightA;
}