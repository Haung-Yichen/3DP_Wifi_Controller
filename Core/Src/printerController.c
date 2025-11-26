#include "printerController.h"
#include <stdlib.h>
#include "cmsis_os.h"
#include "Fatfs_SDIO.h"
#include "esp32.h"
#include "hx711.h"
#include "fileTask.h"
#include "cmdList.h"
#include "ui_updater.h"


/*-----存放印表機各項參數-----*/
typedef struct {
	uint8_t hours;
	uint8_t minutes;
	uint8_t seconds;
} TimeStruct_t;

typedef struct {
	uint8_t nozzleTemp;
	uint8_t bedTemp;
	uint16_t filamentWeight;
	TimeStruct_t remainingTime;
	uint8_t progress;
} PC_Parameter_TypeDef;


static PC_Parameter_TypeDef pcParameter;
static PC_Status_TypeDef pcStatus = PC_INIT;


osThreadId_t pcTaskHandle = NULL;
const osThreadAttr_t pcTask_attributes = {
	.name = "PC_Print_Task",
	.stack_size = configMINIMAL_STACK_SIZE * 30,
	.priority = (osPriority) osPriorityAboveNormal,
};

volatile bool pause = false;
volatile bool stopRequested = false;

// 用於印表機通訊的緩衝區
static uint8_t pc_TxBuf[100] = {0};
static uint8_t pc_RxBuf[128] = {0};  // 增大緩衝區以容納完整的溫度回應

// 用於非阻塞 UART 通訊
SemaphoreHandle_t printerRxSemaphore = NULL;
volatile bool printerOkReceived = false;
char printerRxBuf[64] __attribute__((aligned(4)));
volatile uint16_t printerRxLen = 0;

static void PC_ParseRemainingTime(FIL *file);

void PC_init(void) {
	PC_RegCallback();
	pcParameter.nozzleTemp = 0;
	pcParameter.bedTemp = 0;
	pcParameter.filamentWeight = 0;
	pcParameter.progress = 0;
	pcParameter.remainingTime.hours = 0;
	pcParameter.remainingTime.minutes = 0;
	pcParameter.remainingTime.seconds = 0;
	
	// 注意：信號量在 RTOS 啟動後才能創建
	// 將在 PC_Print_Task 中延遲初始化
	printerRxSemaphore = NULL;
}

void PC_RegCallback(void) {
	register_command(CMD_Start_To_Print, StartToPrintHandler);
	register_command(CMD_Pause_Printing, PausePrintingHandler);
	register_command(CMD_Stop_printing, StopPrintingHandler);
	register_command(CMD_Go_Home, GoHomeHandler);
	register_command(CMD_Get_Remainning_time, GetRemainingTimeHandler);
	register_command(CMD_Get_Progress, GetProgressHandler);
	register_command(CMD_Get_Nozzle_Temp, GetNozzleTempHandler);
	register_command(CMD_Get_Bed_Temp, GetBedTempHandler);
	register_command(CMD_Set_Nozzle_Temp, SetNozzleTempHandler);
	register_command(CMD_Set_Bed_Temp, SetBedTempHandler);
	register_command(CMD_GetFilament_Weight, GetFilamentWeightHandler);
	register_command(CMD_Emergency_Stop, EmergencyStopHandler);
	register_command(CMD_GET_ALL_FILES, GetAllFilesHandler);
}

void PC_Print_Task(void *argument) {
	FIL file;
	FRESULT f_res;

	bool file_opened = false;
	__attribute__((aligned(4))) char gcode_line[128];
	uint32_t line = 0;
	DWORD file_size = 0;
	DWORD bytes_read = 0;
	TickType_t last_time_update = 0;
	uint32_t initial_total_seconds = 0;

	// 確保信號量已初始化
	if (printerRxSemaphore == NULL) {
		printerRxSemaphore = xSemaphoreCreateBinary();
		if (printerRxSemaphore == NULL) {
			printf("%-20s Failed to create semaphore\r\n", "[printerController.c]");
			goto CleanRes;
		}
	}

	//================ 錯誤處理 ================//
	if (strlen(curFileName) <= 0) {
		printf("%-20s no file selected\r\n", "[printerController.c]");
		goto CleanRes;
	}
	f_res = f_open(&file, curFileName, FA_READ);
	if (f_res != FR_OK) {
		printf("%-20s Failed to open file: %s\r\n", "[printerController.c]", curFileName);
		printf_fatfs_error(f_res);
		goto CleanRes;
	}
	file_opened = true;
	if (f_size(&file) <= 0) {
		printf("%-20s file has no content\r\n", "[printerController.c]");
		goto CleanRes;
	}

	PC_SetState(PC_BUSY);
	PC_ParseRemainingTime(&file); // 取得列印時間
	initial_total_seconds = pcParameter.remainingTime.hours * 3600 + 
	                        pcParameter.remainingTime.minutes * 60 + 
	                        pcParameter.remainingTime.seconds;
	file_size = f_size(&file); // 計算檔案大小用於進度追蹤


	//================ 開始列印 ================//
	UI_Update_Status("Printing...");
	f_lseek(&file, 0);
	pause = false;
	last_time_update = xTaskGetTickCount();
	while (1) {
		memset(gcode_line, 0, sizeof(gcode_line));
		if (f_gets(gcode_line, sizeof(gcode_line), &file) == NULL) {
			if (f_eof(&file)) {
				printf("\r\n%-20s printTask completed! line: %d file: %s\r\n", "[printerController.c]", line,
				       curFileName);
			} else if (f_error(&file)) {
				printf("\r\n%-20s file read err:", "[printerController.c]");
				printf_fatfs_error(f_error(&file));
			}
			break; // 正常列印完成
		}
		if (stopRequested) {
			printf("%-20s Stop requested by user. Terminating task.\r\n", "[printerController.c]");
			break;
		}
		while (pause) {
			osDelay(pdMS_TO_TICKS(10));
			if (stopRequested) {
				printf("%-20s Stop requested during pause. Terminating task.\r\n", "[printerController.c]");
				goto CleanRes;
			}
		}
		line++;
		bytes_read = f_tell(&file); // 更新進度
		if (file_size > 0) {
			pcParameter.progress = (uint8_t)((bytes_read * 100) / file_size);
		}

		TickType_t current_tick = xTaskGetTickCount(); // 更新剩餘時間 (每秒更新一次)
		if ((current_tick - last_time_update) >= pdMS_TO_TICKS(1000)) {
			last_time_update = current_tick;

			if (pcParameter.progress > 0 && initial_total_seconds > 0) {
				uint32_t remaining_seconds = (initial_total_seconds * (100 - pcParameter.progress)) / 100;
				pcParameter.remainingTime.hours = remaining_seconds / 3600;
				pcParameter.remainingTime.minutes = (remaining_seconds % 3600) / 60;
				pcParameter.remainingTime.seconds = remaining_seconds % 60;
			}
		}
		// 中止之前可能正在進行的接收
		HAL_UART_AbortReceive(&huart3);
		xSemaphoreTake(printerRxSemaphore, 0);
		memset(printerRxBuf, 0, sizeof(printerRxBuf));
		printerRxLen = 0;
		printerOkReceived = false;
		HAL_UART_Receive_DMA(&huart3, (uint8_t*)printerRxBuf, sizeof(printerRxBuf) - 1);
		__HAL_UART_ENABLE_IT(&huart3, UART_IT_IDLE);
		
		// 發送 G-code（阻塞式確保發送完成）
		HAL_StatusTypeDef uart_status = HAL_UART_Transmit(&huart3,
		                                                   (uint8_t*)gcode_line,
		                                                   strlen(gcode_line),
		                                                   1000);
		if (uart_status != HAL_OK) {
		    printf("%-20s UART TX failed: %d\r\n", "[printerController.c]", uart_status);
		    HAL_UART_AbortReceive(&huart3);
		    break;
		}
		
		// 等待印表機回應 (5 秒超時)
		if (xSemaphoreTake(printerRxSemaphore, pdMS_TO_TICKS(5000)) == pdTRUE) {
		    // 檢查是否收到 "ok"
		    if (!printerOkReceived) {
		        printf("%-20s Unexpected response: %s\r\n", "[printerController.c]", printerRxBuf);
		    }
		} else {
		    // 超時，中止接收
		    HAL_UART_AbortReceive(&huart3);
		    printf("%-20s Timeout at line %lu\r\n", "[printerController.c]", line);
		}
	}
CleanRes:
	if (file_opened) {
		f_close(&file);
	}
	if (!stopRequested) {
		pcParameter.progress = 100;
		pcParameter.remainingTime.hours = 0;
		pcParameter.remainingTime.minutes = 0;
		pcParameter.remainingTime.seconds = 0;
	}
	pause = false;
	PC_SetState(PC_IDLE);
	ESP32_SetState(ESP32_IDLE);
	UI_Update_Status("Idle");
	stopRequested = false;
	pcTaskHandle = NULL;
	vTaskDelete(NULL);
}

PC_Status_TypeDef PC_GetState(void) {
	return pcStatus;
}

void PC_SetState(PC_Status_TypeDef status) {
	pcStatus = status;
}

void PC_Param_Polling(void) {
	// 網頁沒有連接才需要自己輪詢
	if (isWebConnected) {
		return;
	}
	// 印表機列印中才需要更新時間及進度
	// 因為不需要回傳給esp32 故參數都給null
	if (PC_GetState() == PC_BUSY) {
		GetRemainingTimeHandler(NULL, NULL);
		GetProgressHandler(NULL, NULL);
	}
	GetFilamentWeightHandler(NULL, NULL);
	GetBedTempHandler(NULL, NULL);
	GetNozzleTempHandler(NULL, NULL);
}

void StartToPrintHandler(const char *args, ResStruct_t *_resStruct) {
	// 從參數中提取檔名
	if (!extract_parameter(args, curFileName, FILENAME_SIZE)) {
		printf("%-20s Invalid filename format\r\n", "[printerController.c]");
		return;
	}
	printf("%-20s Start printing: %s\r\n", "[printerController.c]", curFileName);
	
	stopRequested = false;
	pcTaskHandle = osThreadNew(PC_Print_Task, NULL, &pcTask_attributes);
	if (pcTaskHandle == NULL) {
		printf("%-20s Error creating pcPrintTask\r\n", "[printerController.c]");
		ESP32_SetState(ESP32_IDLE);
	}
}

void PausePrintingHandler(const char *args, ResStruct_t *_resStruct) {
	static bool i = 0;
	//通知印表機控制器暫停發送gcode
	i = !i;
	pause = i;
}

void StopPrintingHandler(const char *args, ResStruct_t *_resStruct) {
	pause = false;
	ESP32_SetState(ESP32_IDLE);

	if (pcTaskHandle != NULL) {
		printf("%-20s Sending stop request to print task...\r\n", "[printerController.c]");
		stopRequested = true;
	}
	UART_SendString_DMA(&PRINTING_USART_PORT, "G28\r\nM104 S0\r\nM140 S0\r\n");
}

void GoHomeHandler(const char *args, ResStruct_t *_resStruct) {
	/*       回原點       */
	UART_SendString_DMA(&PRINTING_USART_PORT, "G28\r\n");
}

void GetRemainingTimeHandler(const char *args, ResStruct_t *_resStruct) {
	int total_seconds = pcParameter.remainingTime.hours * 3600 + 
	                    pcParameter.remainingTime.minutes * 60 + 
	                    pcParameter.remainingTime.seconds;

	if (_resStruct != NULL) {
		sprintf(_resStruct->resBuf, "RemainingTime:%d\n", total_seconds);
	}
	UI_Update_RemainingTime(pcParameter.remainingTime.hours,
	                        pcParameter.remainingTime.minutes,
	                        pcParameter.remainingTime.seconds);
}

void GetProgressHandler(const char *args, ResStruct_t *_resStruct) {
	if (_resStruct != NULL) {
		sprintf(_resStruct->resBuf, "Progress:%d\n", pcParameter.progress);
	}
	UI_Update_Progress(pcParameter.progress);
}

void GetNozzleTempHandler(const char *args, ResStruct_t *_resStruct) {
	// 列印期間不查詢溫度，避免干擾 DMA 通訊
	if (PC_GetState() == PC_BUSY) {
		if (_resStruct != NULL) {
			sprintf(_resStruct->resBuf, "NozzleTemp:%d\n", pcParameter.nozzleTemp);
		}
		UI_Update_NozzleTemp(pcParameter.nozzleTemp);
		return;
	}

	if (printerRxSemaphore == NULL) {
		printerRxSemaphore = xSemaphoreCreateBinary();
		if (printerRxSemaphore == NULL) {
			if (_resStruct != NULL) {
				sprintf(_resStruct->resBuf, "NozzleTemp:%d\n", pcParameter.nozzleTemp);
			}
			UI_Update_NozzleTemp(pcParameter.nozzleTemp);
			return;
		}
	}

	// 中止任何正在進行的接收（清空印表機啟動信息等）
	HAL_UART_AbortReceive(&PRINTING_USART_PORT);
	
	// 清空信號量 (確保沒有殘留的信號)
	xSemaphoreTake(printerRxSemaphore, 0);
	
	// 準備接收緩衝區
	memset((void*)printerRxBuf, 0, sizeof(printerRxBuf));
	printerRxLen = 0;
	printerOkReceived = false;

	// 先發送 M105 命令 (阻塞式發送確保完成)
	HAL_UART_Transmit(&PRINTING_USART_PORT, (uint8_t*)"M105\r\n", 6, 100);

	// 啟動 DMA 接收並啟用 IDLE 中斷
	HAL_UART_Receive_DMA(&PRINTING_USART_PORT, (uint8_t*)printerRxBuf, sizeof(printerRxBuf) - 1);
	__HAL_UART_ENABLE_IT(&PRINTING_USART_PORT, UART_IT_IDLE);
	
	// 等待印表機回應 (使用信號量，最長等待 2 秒)
	if (xSemaphoreTake(printerRxSemaphore, pdMS_TO_TICKS(2000)) == pdTRUE) {
		// 複製到本地緩衝區進行解析（避免 DMA 緩衝區問題）
		char localBuf[64];
		memcpy(localBuf, (const void*)printerRxBuf, sizeof(localBuf));
		localBuf[sizeof(localBuf) - 1] = '\0';
		
		// 解析印表機回傳的溫度資料，格式如: "ok T:210.5 /210.0 B:60.2 /60.0"
		char *temp_pos = strstr(localBuf, "T:");
		if (temp_pos != NULL) {
			pcParameter.nozzleTemp = (uint8_t)atoi(temp_pos + 2);
		}
		char *bed_pos = strstr(localBuf, "B:");
		if (bed_pos != NULL) {
			pcParameter.bedTemp = (uint8_t)atoi(bed_pos + 2);
		}
	} else {
		HAL_UART_AbortReceive(&PRINTING_USART_PORT);
	}
	
	if (_resStruct != NULL) {
		sprintf(_resStruct->resBuf, "NozzleTemp:%d\n", pcParameter.nozzleTemp);
	}
	UI_Update_NozzleTemp(pcParameter.nozzleTemp);
}

void GetBedTempHandler(const char *args, ResStruct_t *_resStruct) {
	char bedTempStr[16];
	snprintf(bedTempStr, sizeof(bedTempStr), "%d", pcParameter.bedTemp);
	
	if (_resStruct != NULL) {
		sprintf(_resStruct->resBuf, "BedTemp:%d\n", pcParameter.bedTemp);
	}
	UI_Update_BedTemp(bedTempStr);
}

void GetFilamentWeightHandler(const char *args, ResStruct_t *_resStruct) {
	if (ESP32_GetState() == ESP32_BUSY) {
		return;
	}
	float weight_g = Hx711_GetWeight(&hx711, 3);
	pcParameter.filamentWeight = (int)weight_g;
	if (_resStruct != NULL) {
		sprintf(_resStruct->resBuf, "FilamentWeight:%d\n", pcParameter.filamentWeight);
	}
	UI_Update_FilamentWeight(pcParameter.filamentWeight);
}

void SetNozzleTempHandler(const char *args, ResStruct_t *_resStruct) {
	char tmp[10] = {0};
	char gcode_cmd[32] = {0};

	extract_parameter(args, tmp, sizeof(tmp) - 1);
	int temp = atoi(tmp);
	
	// 限制溫度範圍 (安全性)
	if (temp < 0) temp = 0;
	if (temp > 280) temp = 280;
	
	pcParameter.nozzleTemp = (uint8_t)temp;
	
	// 發送 M104 設定噴嘴溫度
	snprintf(gcode_cmd, sizeof(gcode_cmd), "M104 S%d\r\n", temp);
	UART_SendString_DMA(&PRINTING_USART_PORT, gcode_cmd);
	
	printf("%-20s set nozzle temp to %d deg.\r\n", "[printerController.c]", pcParameter.nozzleTemp);
}

void SetBedTempHandler(const char *args, ResStruct_t *_resStruct) {
	char tmp[10] = {0};
	char gcode_cmd[32] = {0};

	extract_parameter(args, tmp, sizeof(tmp) - 1);
	int temp = atoi(tmp);
	
	// 限制溫度範圍 (安全性)
	if (temp < 0) temp = 0;
	if (temp > 120) temp = 120;
	
	pcParameter.bedTemp = (uint8_t)temp;
	
	// 發送 M140 設定熱床溫度
	snprintf(gcode_cmd, sizeof(gcode_cmd), "M140 S%d\r\n", temp);
	UART_SendString_DMA(&PRINTING_USART_PORT, gcode_cmd);
	
	printf("%-20s set bed temp to %d deg.\r\n", "[printerController.c]", pcParameter.bedTemp);
}

void EmergencyStopHandler(const char *args, ResStruct_t *_resStruct) {
	// 發送 M112 緊急停止命令
	UART_SendString_DMA(&PRINTING_USART_PORT, "M112\r\n");
	
	// 停止列印任務
	stopRequested = true;
	pause = false;
	
	// 關閉加熱器
	UART_SendString_DMA(&PRINTING_USART_PORT, "M104 S0\r\nM140 S0\r\n");
	
	PC_SetState(PC_ERROR);
	printf("%-20s EMERGENCY STOP activated!\r\n", "[printerController.c]");
}

/**
 * @brief 解析印表機返回值到pcParameter
 * @param args
 */
static void extMrlResToMem(const char *args) {
}

/**
 * @brief 從 G-code 檔案開頭解析預估的列印時間
 * @param file 指向已開啟檔案的 FIL 物件指標
 * @note 會直接更新全域的 pcParameter.remainingTime
 */
static void PC_ParseRemainingTime(FIL *file) {
	char gcode_line[256] = {0};
	UINT fnum = 0;

	f_read(file, gcode_line, sizeof(gcode_line) - 1, &fnum);
	gcode_line[sizeof(gcode_line) - 1] = '\0';

	const char *search_string = ";Print time: ";
	char *pos = strstr(gcode_line, search_string);

	if (pos != NULL) {
		pos += strlen(search_string);
		// 解析時間格式，可能是 "HH:MM:SS" 或 "MM:SS" 或只有秒數
		int hours = 0, minutes = 0, seconds = 0;
		int parsed = sscanf(pos, "%d:%d:%d", &hours, &minutes, &seconds);
		if (parsed == 3) {
			pcParameter.remainingTime.hours = (uint8_t)hours;
			pcParameter.remainingTime.minutes = (uint8_t)minutes;
			pcParameter.remainingTime.seconds = (uint8_t)seconds;
		} else if (parsed == 2) {
			pcParameter.remainingTime.hours = 0;
			pcParameter.remainingTime.minutes = (uint8_t)hours;
			pcParameter.remainingTime.seconds = (uint8_t)minutes;
		} else {
			int total_seconds = atoi(pos);
			pcParameter.remainingTime.hours = total_seconds / 3600;
			pcParameter.remainingTime.minutes = (total_seconds % 3600) / 60;
			pcParameter.remainingTime.seconds = total_seconds % 60;
		}
		printf("%-20s remaining time: %02d:%02d:%02d\r\n", "[printerController.c]", 
		       pcParameter.remainingTime.hours, 
		       pcParameter.remainingTime.minutes, 
		       pcParameter.remainingTime.seconds);
	} else {
		printf("%-20s Did not find ';Print time: ' in G-code header\r\n", "[printerController.c]", 
		       pcParameter.remainingTime.hours, 
		       pcParameter.remainingTime.minutes, 
		       pcParameter.remainingTime.seconds);
	}
}

void GetAllFilesHandler(const char *args, ResStruct_t *_resStruct) {
	DIR dir;
	static FILINFO fno;  // 靜態避免堆疊溢出
	static char lfnBuf[_MAX_LFN + 1];  // 長檔名緩衝區
	FRESULT res;
	static char fileListBuf[512];  // 靜態緩衝區存放檔案列表
	uint16_t bufPos = 0;
	
	memset(fileListBuf, 0, sizeof(fileListBuf));
	
	// 設置長檔名緩衝區
	fno.lfname = lfnBuf;
	fno.lfsize = sizeof(lfnBuf);
	
	// 開啟根目錄
	res = f_opendir(&dir, "/");
	if (res != FR_OK) {
		printf("%-20s Failed to open root dir, err=%d\r\n", "[printerController.c]", res);
		if (_resStruct != NULL) {
			sprintf(_resStruct->resBuf, "Error\n");
		}
		return;
	}
	
	// 讀取目錄中的所有檔案
	for (;;) {
		memset(lfnBuf, 0, sizeof(lfnBuf));
		res = f_readdir(&dir, &fno);
		if (res != FR_OK) {
			printf("%-20s readdir err=%d\r\n", "[printerController.c]", res);
			break;
		}
		if (fno.fname[0] == 0) {
			break;  // 讀取結束
		}
		
		// 跳過目錄和隱藏檔案
		if (fno.fattrib & (AM_DIR | AM_HID | AM_SYS)) {
			continue;
		}
		
		// 優先使用長檔名，否則使用短檔名
		char *fileName = (fno.lfname[0] != 0) ? fno.lfname : fno.fname;
		
		// 檢查緩衝區是否還有空間
		size_t nameLen = strlen(fileName);
		if (bufPos + nameLen + 2 >= sizeof(fileListBuf)) {
			break;  // 緩衝區已滿
		}
		
		// 添加檔名（用空格分隔）
		if (bufPos > 0) {
			fileListBuf[bufPos++] = ' ';
		}
		strcpy(&fileListBuf[bufPos], fileName);
		bufPos += nameLen;
	}
	
	f_closedir(&dir);
	
	// 如果沒有檔案，返回空
	if (bufPos == 0) {
		strcpy(fileListBuf, "NoFiles\n");
		bufPos = strlen(fileListBuf);
	} else {
		// 添加換行符
		fileListBuf[bufPos++] = '\n';
		fileListBuf[bufPos] = '\0';
	}
	
	printf("%-20s Files: %s", "[printerController.c]", fileListBuf);
	
	// 使用 DMA 發送（與其他 ESP32 通訊一致）
	UART_SendString_DMA(&ESP32_USART_PORT, fileListBuf);
}
