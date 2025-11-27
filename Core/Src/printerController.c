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
char printerRxBuf[PRINTER_RX_BUF_SIZE] __attribute__((aligned(4)));
volatile uint16_t printerRxLen = 0;

static void PC_ParseRemainingTime(FIL *file);
static void PC_ParseTemperatureFromResponse(const char *response);

// 預設超時時間 (毫秒)
#define GCODE_DEFAULT_TIMEOUT_MS     10000   // 一般命令 10 秒
#define GCODE_BLOCKING_TIMEOUT_MS   300000   // 阻塞命令 (M109/M190/G28) 5 分鐘

/**
 * @brief 判斷 G-code 命令是否為阻塞命令
 * @param gcode_line G-code 命令字串
 * @return true 為阻塞命令，需要更長的超時時間
 */
static bool PC_IsBlockingCommand(const char *gcode_line) {
	if (gcode_line == NULL) return false;
	
	// 跳過前導空白
	const char *p = gcode_line;
	while (*p == ' ' || *p == '\t') p++;
	
	// 跳過行號 (N123)
	if (*p == 'N' || *p == 'n') {
		while (*p && *p != ' ') p++;
		while (*p == ' ') p++;
	}
	
	// 檢查阻塞命令
	// M109 - 等待噴頭溫度
	// M190 - 等待熱床溫度
	// G28  - 歸零 (可能需要 30-60 秒)
	// M400 - 等待所有移動完成
	if ((p[0] == 'M' || p[0] == 'm') && p[1] == '1' && p[2] == '0' && p[3] == '9') return true;
	if ((p[0] == 'M' || p[0] == 'm') && p[1] == '1' && p[2] == '9' && p[3] == '0') return true;
	if ((p[0] == 'G' || p[0] == 'g') && p[1] == '2' && p[2] == '8') return true;
	if ((p[0] == 'M' || p[0] == 'm') && p[1] == '4' && p[2] == '0' && p[3] == '0') return true;
	
	return false;
}

/**
 * @brief 發送 G-code 並等待印表機回應 "ok"
 * @note  根據 Marlin 協議，必須等待 "ok" 才能發送下一個命令
 *        對於 M109/M190 等阻塞命令，期間會持續收到溫度報告 (以空格開頭)
 * @param gcode_line G-code 命令字串 (應包含換行符)
 * @return true 成功收到 "ok"，false 超時或錯誤
 */
static bool PC_SendGcodeAndWaitOk(const char *gcode_line) {
	if (gcode_line == NULL || strlen(gcode_line) == 0) return true;
	
	bool is_blocking = PC_IsBlockingCommand(gcode_line);
	uint32_t timeout_ms = is_blocking ? GCODE_BLOCKING_TIMEOUT_MS : GCODE_DEFAULT_TIMEOUT_MS;
	uint32_t single_wait_ms = 5000;  // 每次等待 5 秒
	uint32_t elapsed_ms = 0;
	
	// 準備接收
	HAL_UART_AbortReceive(&huart3);
	xSemaphoreTake(printerRxSemaphore, 0);  // 清除信號量
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
		return false;
	}
	
	// 等待印表機回應
	while (elapsed_ms < timeout_ms) {
		// 檢查停止請求
		if (stopRequested) {
			HAL_UART_AbortReceive(&huart3);
			return false;
		}
		
		if (xSemaphoreTake(printerRxSemaphore, pdMS_TO_TICKS(single_wait_ms)) == pdTRUE) {
			// 收到回應，先解析溫度 (Marlin 回應格式: "ok T:xxx B:xxx" 或 "T:xxx B:xxx")
			PC_ParseTemperatureFromResponse(printerRxBuf);
			
			if (printerOkReceived) {
				// 成功收到 "ok"
				return true;
			} else {
				// 收到其他回應 (可能是溫度報告或 echo 訊息)
				// 溫度已在上方解析
				
				// 對於阻塞命令，這是正常的，繼續等待
				if (is_blocking) {
					// 重新啟動接收，等待下一個回應
					memset(printerRxBuf, 0, sizeof(printerRxBuf));
					printerRxLen = 0;
					printerOkReceived = false;
					HAL_UART_Receive_DMA(&huart3, (uint8_t*)printerRxBuf, sizeof(printerRxBuf) - 1);
					__HAL_UART_ENABLE_IT(&huart3, UART_IT_IDLE);
				} else {
					// 非阻塞命令收到非 "ok" 回應，可能是多行回應
					// 溫度已在上方解析，繼續等待 "ok"
					memset(printerRxBuf, 0, sizeof(printerRxBuf));
					printerRxLen = 0;
					printerOkReceived = false;
					HAL_UART_Receive_DMA(&huart3, (uint8_t*)printerRxBuf, sizeof(printerRxBuf) - 1);
					__HAL_UART_ENABLE_IT(&huart3, UART_IT_IDLE);
				}
			}
		} else {
			// 本次等待超時
			elapsed_ms += single_wait_ms;
		}
	}
	
	// 總超時
	HAL_UART_AbortReceive(&huart3);
	printf("%-20s Timeout waiting for ok (cmd: %.20s...)\r\n", "[printerController.c]", gcode_line);
	return false;
}

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

		// 跳過空行 (只有換行符的行)
		if (gcode_line[0] == '\n' || gcode_line[0] == '\r') {
			continue;
		}

		// 發送 G-code 並等待 "ok" (根據 Marlin 協議)
		if (!PC_SendGcodeAndWaitOk(gcode_line)) {
			if (stopRequested) {
				printf("%-20s Stop requested, terminating.\r\n", "[printerController.c]");
			} else {
				printf("%-20s Failed at line %lu, continuing...\r\n", "[printerController.c]", line);
				// 可選：繼續列印還是停止？這裡選擇繼續
			}
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

/**
 * @brief 查詢印表機溫度 (在背景任務中呼叫)
 * @note  此函數會阻塞等待印表機回應，不應在命令處理任務中呼叫
 */
void PC_QueryTemperature(void) {
	// 列印期間不查詢溫度，避免干擾 G-code 發送
	if (PC_GetState() == PC_BUSY) {
		return;
	}
	
	if (printerRxSemaphore == NULL) {
		return;
	}

	// 中止任何正在進行的接收
	HAL_UART_AbortReceive(&PRINTING_USART_PORT);
	
	// 清空信號量
	xSemaphoreTake(printerRxSemaphore, 0);
	
	// 準備接收緩衝區
	memset((void*)printerRxBuf, 0, sizeof(printerRxBuf));
	printerRxLen = 0;
	printerOkReceived = false;

	// 發送 M105 命令
	HAL_UART_Transmit(&PRINTING_USART_PORT, (uint8_t*)"M105\r\n", 6, 100);

	// 啟動 DMA 接收
	HAL_UART_Receive_DMA(&PRINTING_USART_PORT, (uint8_t*)printerRxBuf, sizeof(printerRxBuf) - 1);
	__HAL_UART_ENABLE_IT(&PRINTING_USART_PORT, UART_IT_IDLE);
	
	// 等待印表機回應 (最長等待 2 秒)
	if (xSemaphoreTake(printerRxSemaphore, pdMS_TO_TICKS(2000)) == pdTRUE) {
		// 解析溫度資料，格式如: "ok T:210.5 /210.0 B:60.2 /60.0"
		char *temp_pos = strstr(printerRxBuf, "T:");
		if (temp_pos != NULL) {
			pcParameter.nozzleTemp = (uint8_t)atoi(temp_pos + 2);
		}
		char *bed_pos = strstr(printerRxBuf, "B:");
		if (bed_pos != NULL) {
			pcParameter.bedTemp = (uint8_t)atoi(bed_pos + 2);
		}
	} else {
		HAL_UART_AbortReceive(&PRINTING_USART_PORT);
	}
}

/**
 * @brief 從回應字串中解析溫度資料並更新參數
 * @param response 印表機回應字串
 * @note  溫度格式: "T:210.5 /210.0 B:60.2 /60.0" 或 "ok T:210.5 /210.0 B:60.2 /60.0"
 */
static void PC_ParseTemperatureFromResponse(const char *response) {
	if (response == NULL) return;
	
	// 解析噴頭溫度
	char *temp_pos = strstr(response, "T:");
	if (temp_pos != NULL) {
		pcParameter.nozzleTemp = (uint8_t)atoi(temp_pos + 2);
	}
	
	// 解析熱床溫度
	char *bed_pos = strstr(response, "B:");
	if (bed_pos != NULL) {
		pcParameter.bedTemp = (uint8_t)atoi(bed_pos + 2);
	}
}

/**
 * @brief 查詢耗材重量 (在背景任務中呼叫)
 * @note  此函數會阻塞等待 HX711 讀取，不應在命令處理任務中呼叫
 */
void PC_QueryFilamentWeight(void) {
	if (ESP32_GetState() == ESP32_BUSY) {
		return;
	}
	float weight_g = Hx711_GetWeight(&hx711, 3);
	pcParameter.filamentWeight = (int)weight_g;
}

void PC_Param_Polling(void) {
	PC_QueryTemperature();
	PC_QueryFilamentWeight();

	UI_Update_NozzleTemp(pcParameter.nozzleTemp);
	UI_Update_BedTemp_Int(pcParameter.bedTemp);
	UI_Update_FilamentWeight(pcParameter.filamentWeight);
	
	if (PC_GetState() == PC_BUSY) {
		UI_Update_RemainingTime(pcParameter.remainingTime.hours,
		                        pcParameter.remainingTime.minutes,
		                        pcParameter.remainingTime.seconds);
		UI_Update_Progress(pcParameter.progress);
	}
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
	// 直接回傳快取值（非阻塞）
	if (_resStruct != NULL) {
		int total_seconds = pcParameter.remainingTime.hours * 3600 + 
		                    pcParameter.remainingTime.minutes * 60 + 
		                    pcParameter.remainingTime.seconds;
		sprintf(_resStruct->resBuf, "RemainingTime:%d\n", total_seconds);
	}
}

void GetProgressHandler(const char *args, ResStruct_t *_resStruct) {
	// 直接回傳快取值（非阻塞）
	if (_resStruct != NULL) {
		sprintf(_resStruct->resBuf, "Progress:%d\n", pcParameter.progress);
	}
}

void GetNozzleTempHandler(const char *args, ResStruct_t *_resStruct) {
	// 直接回傳快取值（非阻塞）
	// 溫度會由 PC_QueryTemperature() 在背景定期更新
	if (_resStruct != NULL) {
		sprintf(_resStruct->resBuf, "NozzleTemp:%d\n", pcParameter.nozzleTemp);
	}
}

void GetBedTempHandler(const char *args, ResStruct_t *_resStruct) {
	// 直接回傳快取值（非阻塞）
	if (_resStruct != NULL) {
		sprintf(_resStruct->resBuf, "BedTemp:%d\n", pcParameter.bedTemp);
	}
}

void GetFilamentWeightHandler(const char *args, ResStruct_t *_resStruct) {
	// 直接回傳快取的重量值（非阻塞）
	// 重量會由 PC_QueryFilamentWeight() 在背景定期更新
	if (_resStruct != NULL) {
		sprintf(_resStruct->resBuf, "FilamentWeight:%d\n", pcParameter.filamentWeight);
	}
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
