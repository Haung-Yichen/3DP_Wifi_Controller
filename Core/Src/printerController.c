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

static void PC_ParseRemainingTime(FIL *file);

void PC_init(void) {
	PC_RegCallback();
	pcParameter.nozzleTemp = 30;
	pcParameter.bedTemp = 0;
	pcParameter.filamentWeight = 0;
	pcParameter.progress = 0;
	pcParameter.remainingTime.hours = 0;
	pcParameter.remainingTime.minutes = 0;
	pcParameter.remainingTime.seconds = 0;
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
}

void PC_Print_Task(void *argument) {
	FIL file;
	FRESULT f_res;

	bool file_opened = false;
	char printer_response[64] = {0};
	__attribute__((aligned(4))) char gcode_line[128];
	uint32_t line = 0;
	DWORD file_size = 0;
	DWORD bytes_read = 0;
	TickType_t last_time_update = 0;
	uint32_t initial_total_seconds = 0;

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

		if (gcode_line[0] == '\n' || gcode_line[0] == '\r' || gcode_line[0] == ';') {
		    continue;
		}
		HAL_StatusTypeDef uart_status = HAL_UART_Transmit(&huart3,
		                                                 (uint8_t*)gcode_line,
		                                                 strlen(gcode_line),
		                                                 1000);
		if (uart_status != HAL_OK) {
		    printf("UART transmission failed: %d\r\n", uart_status);
		    break;
		}
		memset(printer_response, 0, sizeof(printer_response)); // 等待印表機回復 "ok"
		uart_status = HAL_UART_Receive(&huart3,
		                              (uint8_t*)printer_response,
		                              sizeof(printer_response) - 1,
		                              5000);

		if (uart_status == HAL_OK || uart_status == HAL_TIMEOUT) {
		    printer_response[sizeof(printer_response) - 1] = '\0';

		    if (strstr(printer_response, "ok") == NULL) { // 檢查是否收到 "ok"
		        printf("Unexpected printer response: %s\r\n", printer_response);
		    }
		} else {
		    printf("Failed to receive printer response: %d\r\n", uart_status);
		    break;
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
	stopRequested = false;
	pcTaskHandle = osThreadNew(PC_Print_Task, NULL, &pcTask_attributes);
	if (pcTaskHandle == NULL) {
		printf("%-20s error creating pcPrintTask\r\n", "[printerController.c]");
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
	UART_SendString_DMA(&PRINTING_USART_PORT, "M105\r\n");
	memset(pc_RxBuf, 0, sizeof(pc_RxBuf));

	// HAL_StatusTypeDef status = HAL_UART_Receive(
	// 	&PRINTING_USART_PORT, pc_RxBuf, sizeof(pc_RxBuf), pdMS_TO_TICKS(1000));
	// if (status == HAL_OK) {
	// 	// 解析印表機回傳的溫度資料，格式如: "ok T:210.5 /210.0 B:60.2 /60.0"
	// 	char *temp_pos = strstr((char*)pc_RxBuf, "T:");
	// 	if (temp_pos != NULL) {
	// 		float temp = 0.0f;
	// 		if (sscanf(temp_pos + 2, "%f", &temp) == 1) {
	// 			pcParameter.nozzleTemp = (uint8_t)temp;
	// 		}
	// 	}
	// }
	if (_resStruct != NULL) {
		sprintf(_resStruct->resBuf, "NozzleTemp:%d\n", pcParameter.nozzleTemp);
	}
	UI_Update_NozzleTemp(pcParameter.nozzleTemp);
}

void GetBedTempHandler(const char *args, ResStruct_t *_resStruct) {
	if (_resStruct != NULL) {
		sprintf(_resStruct->resBuf, "BedTemp:%s\n", "N/A");
	}
	UI_Update_BedTemp("N/A");
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

	extract_parameter(args, tmp, 20);
	pcParameter.nozzleTemp = atoi(tmp);
	printf("%-20s set nozzle temp to %d deg.\r\n", "[pc.c]", pcParameter.nozzleTemp);
}

// unused
void SetBedTempHandler(const char *args, ResStruct_t *_resStruct) {
}

void EmergencyStopHandler(const char *args, ResStruct_t *_resStruct) {
	// TODO: 實作緊急停止邏輯
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
		printf("%-20s Did not find ';Print time: ' in G-code header\r\n", "[printerController.c]");
	}
}
