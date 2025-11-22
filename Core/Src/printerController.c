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
	UINT fnum;

	bool file_opened = false;
	char printer_response[64] = {0};
	__attribute__((aligned(4))) char gcode_line[128];
	uint32_t line = 0;

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
	//================ 取得列印時間 ================//
	PC_ParseRemainingTime(&file);
	//================ 開始列印 ================//
	f_lseek(&file, 0);
	pause = false;
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
			break; // 收到停止請求，跳出迴圈
		}
		while (pause) {
			osDelay(pdMS_TO_TICKS(10));
			if (stopRequested) {
				printf("%-20s Stop requested during pause. Terminating task.\r\n", "[printerController.c]");
				goto CleanRes; // 在暫停時被要求停止，直接跳到清理
			}
		}

		line++;
		// 跳過空行和註解行
		// if (strlen(gcode_line) == 0 || strchr(gcode_line, ';') != NULL) {
		//     continue;
		// }

		// 發送 G-code 到印表機
		HAL_StatusTypeDef uart_status = HAL_UART_Transmit(&huart3,
		                                                 (uint8_t*)gcode_line,
		                                                 strlen(gcode_line),
		                                                 1000);

		if (uart_status != HAL_OK) {
		    printf("UART transmission failed: %d\r\n", uart_status);
		    break;
		}

		// 等待印表機回復 "ok"
		// memset(printer_response, 0, sizeof(printer_response));

		// 使用阻塞接收等待 "ok" 回應
		// uart_status = HAL_UART_Receive(&huart3,
		//                               (uint8_t*)printer_response,
		//                               sizeof(printer_response) - 1,
		//                               5000);  // 5秒超時

		// if (uart_status == HAL_OK || uart_status == HAL_TIMEOUT) {
		//     // 確保字串結尾
		//     printer_response[sizeof(printer_response) - 1] = '\0';
		//
		//     // 檢查是否收到 "ok"
		//     if (strstr(printer_response, "ok") != NULL) {
		//         printf("Printer responded: %s", printer_response);
		//     } else {
		//         printf("Unexpected printer response: %s\r\n", printer_response);
		//         // 繼續執行，不中斷列印
		//     }
		// } else {
		//     printf("Failed to receive printer response: %d\r\n", uart_status);
		//     // 可選擇是否中斷列印
		//     break;
		// }
		// 清空 gcode_line 準備下一行

		vTaskDelay(50);
	}
CleanRes:
	if (file_opened) {
		f_close(&file);
	}
	pause = false;
	PC_SetState(PC_IDLE);
	stopRequested = false; // 重置旗標
	pcTaskHandle = NULL;
	vTaskDelete(NULL);
	// 這行 printf 永遠不應該被執行
	// printf("%-20s !!! ERROR: Task did not terminate!\r\n", "[printerController.c]");
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

	// 僅讀取檔案開頭的 256 bytes，時間資訊通常在這裡
	f_read(file, gcode_line, sizeof(gcode_line) - 1, &fnum);
	gcode_line[sizeof(gcode_line) - 1] = '\0'; // 確保字串結尾

	const char *search_string = ";Print time: ";
	char *pos = strstr(gcode_line, search_string);

	if (pos != NULL) {
		// 將指標移動到搜尋字串之後
		pos += strlen(search_string);

		// 解析時間格式，可能是 "HH:MM:SS" 或 "MM:SS" 或只有秒數
		int hours = 0, minutes = 0, seconds = 0;
		int parsed = sscanf(pos, "%d:%d:%d", &hours, &minutes, &seconds);
		
		if (parsed == 3) {
			// Format: HH:MM:SS
			pcParameter.remainingTime.hours = (uint8_t)hours;
			pcParameter.remainingTime.minutes = (uint8_t)minutes;
			pcParameter.remainingTime.seconds = (uint8_t)seconds;
		} else if (parsed == 2) {
			// Format: MM:SS (hours is actually minutes, minutes is seconds)
			pcParameter.remainingTime.hours = 0;
			pcParameter.remainingTime.minutes = (uint8_t)hours;
			pcParameter.remainingTime.seconds = (uint8_t)minutes;
		} else {
			// Only seconds
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
