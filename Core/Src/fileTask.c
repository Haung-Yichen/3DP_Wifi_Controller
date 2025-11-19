#include "cmsis_os.h"
#include "fileTask.h"
#include "usart.h"
#include "esp32.h"
#include "ff_print_err.h"

#define SD_RTY_TIMES			 2			//sd寫檔重試次數
#define USE_SHA256               1
#define FILE_QUEUE_LEN			 4


osThreadId_t gcodeRxTaskHandle = NULL;
SemaphoreHandle_t fileSemaphore = NULL;
uartRxBuf_TypeDef fileBuf;

QueueHandle_t xFileQueue = NULL;
StaticQueue_t fileQueue_s;
uint8_t fileQueueArea[FILE_QUEUE_LEN * sizeof(fileBuf)];

const osThreadAttr_t gcodeTask_attributes = {
	.name = "Gcode_Rx_Task",
	.stack_size = configMINIMAL_STACK_SIZE * 28,
	.priority = (osPriority) osPriorityHigh7,
};

char curFileName[FILENAME_SIZE] = {0};
volatile bool delete = false;
volatile bool isTransmittimg = false;

typedef struct {
	FIL file;						// 檔案物件
	FRESULT f_res;
	uint32_t stackHighWaterMark;	// 監測stack用量
	uint32_t fnumCount;				// 監測檔案寫入總位元
	uint32_t timer;					// 計算接收耗時
	SHA256_CTX sha256_ctx;
}transmittingCtx_TypeDef;

typedef enum {RECV_OK, RECV_FAIL} RECV_STATUS_TypeDef;

static void dumpArr(int *arr, int numOfArr);
static RECV_STATUS_TypeDef transmittingInitStage(transmittingCtx_TypeDef* ctx);
static RECV_STATUS_TypeDef transmittingStage(transmittingCtx_TypeDef* ctx);
static RECV_STATUS_TypeDef transmittingOverStage(transmittingCtx_TypeDef* ctx, void* _argument);


void Gcode_RxHandler_Task(void *argument) {
	transmittingCtx_TypeDef transmittingCtx;
	transmittingCtx.f_res = FR_OK;
	transmittingCtx.fnumCount = 0;
	transmittingCtx.timer = 0;
	transmittingCtx.stackHighWaterMark = 0;

	RECV_STATUS_TypeDef recvStatus = RECV_OK;

	if (argument == NULL) {
		printf("%-20s argument is NULL\r\n", "[fileTask.c]");
		goto Delete;
	}
	if (RECV_OK != transmittingInitStage(&transmittingCtx)) { goto Delete; }

	while (1) {
		/*    判斷是否該結束接收   */
		if (RECV_OK != transmittingStage(&transmittingCtx) || delete == true) {
			Delete: transmittingOverStage(&transmittingCtx, argument);
		}
	}
}

static RECV_STATUS_TypeDef transmittingInitStage(transmittingCtx_TypeDef* ctx) {
	isTransmittimg = true;
#if USE_SHA256
	sha256_init(&ctx->sha256_ctx);
#endif

	xFileQueue = xQueueCreateStatic(FILE_QUEUE_LEN,
									sizeof(fileBuf),
									fileQueueArea,
									&fileQueue_s);
	if (xFileQueue == NULL) {
		printf("%-20s fileQueue is NULL\r\n", "[fileTask.c]");
	}
	printf("%-20s creating %s... \r\n", "[fileTask.c]", curFileName);

	ctx->f_res = f_open(&ctx->file, curFileName, FA_CREATE_ALWAYS | FA_WRITE);
	if (ctx->f_res != FR_OK) {
		printf("%-20s %-20s \r\n", "[fileTask.c]", "Failed to open file:");
		printf_fatfs_error(ctx->f_res);
		return RECV_FAIL;
	}
	// 清空檔案
	if (f_truncate(&ctx->file) != FR_OK) {
		printf("%-20s %-30s %d \r\n", "[fileTask.c]", "Failed to truncate file:", ctx->f_res);
		f_close(&ctx->file); // 關閉檔案
		return RECV_FAIL;
	}
	vTaskDelay(ESP32_RECV_DELAY);
	UART_SendString_DMA(&ESP32_USART_PORT, "Name ok\n");
	printf("%-20s %-30s free heap: %d bytes \r\n",
		   "[fileTask.c]",
		   "Gcode_RxHandler_Task created!",
		   xPortGetFreeHeapSize());
	printf("%-20s %-20s \r\n", "[fileTask.c]", "Ready to receive.");

#ifdef DEBUG
	ctx->stackHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
#endif
	ctx->timer = xTaskGetTickCount();
	ctx->f_res = FR_OK;
	return RECV_OK;
}

static RECV_STATUS_TypeDef transmittingStage(transmittingCtx_TypeDef* ctx) {
	UINT fnum = 0;
	uint32_t packageNum = 0;		// 檔案接收次數計數器
	uint16_t timeoutCnt = 0;		// 超時檢查計數器
	bool received_data = false;

	received_data = xQueueReceive(xFileQueue, &fileBuf, pdMS_TO_TICKS(1000));

	/*========== 正常接收檔案 ==========*/
	if (received_data && fileBuf.len != 0) {
		timeoutCnt = 0;
		packageNum++;
		ctx->f_res = f_write(&ctx->file, fileBuf.data, fileBuf.len, &fnum);
		if (ctx->f_res != FR_OK) {
			printf_fatfs_error(ctx->f_res);
			return RECV_FAIL;
		} else {
			ctx->fnumCount += fnum;
			if (packageNum >= 10) { // 分析堆棧使用狀況
				packageNum = 0;
				if (uxTaskGetStackHighWaterMark(NULL) < ctx->stackHighWaterMark) {
					ctx->stackHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
				}
			}
#if USE_SHA256
			sha256_update(&ctx->sha256_ctx, fileBuf.data, fileBuf.len);
#endif
		}
		memset(fileBuf.data, 0, UART_RX_BUFFER_SIZE);
		fileBuf.len = 0;

	/*========== 檔案接收完畢 ==========*/
	} else if (received_data != true && fileBuf.len == 0) {
		return RECV_OK;

	/*========== 超時錯誤檢測 ==========*/
	} else {
		// f_sync(&ctx->file);
		timeoutCnt++;
		if (timeoutCnt >= 5) {
			printf("%-20s timeout waiting for uart\r\n", "[fileTask.c]");
			UART_SendString_DMA(&ESP32_USART_PORT, "reset\n");
			return RECV_FAIL;
		}
	}

	/*========== 正常不會跑到這裡 ==========*/
	return RECV_OK;
}

static RECV_STATUS_TypeDef transmittingOverStage(transmittingCtx_TypeDef* ctx, void* _argument) {
	uint32_t tmp = xTaskGetTickCount() - ctx->timer;

	char* srcHash = (char*)_argument;
#if USE_SHA256
	// 完成 SHA256 計算
	uint8_t hash_output[SHA256_BLOCK_SIZE];
	sha256_final(&ctx->sha256_ctx, hash_output);
	// 轉換為十六進位字串
	for (int j = 0; j < SHA256_BLOCK_SIZE; j++) {
		sprintf(_argument + (j * 2), "%02x", hash_output[j]);
	}
#else
	sprintf(_argument, "%d", ctx->fnumCount);
#endif

	f_close(&ctx->file);
	delete = false;
	isTransmittimg = false;
	printf("%-20s fnumCount: %d\r\n", "[fileTask.c]", ctx->fnumCount);
	printf("%-20s minimum stack size: %u\r\n", "[fileTask.c]", ctx->stackHighWaterMark);
	printf("%-20s total time: %dms\r\n", "[fileTask.c]", tmp);

	if (xFileQueue != NULL) {
		vQueueDelete(xFileQueue);
		xFileQueue = NULL;
	}
	vTaskDelete(NULL);
	return RECV_OK;
}


static void dumpArr(int *arr, int numOfArr) {
	if (arr == NULL) {
		return;
	}
	for (int i = 0; i < numOfArr; i++) {
		printf("%d ", arr[i]);
	}
	printf("\n");
}


void calFileHash(char* hashOutput) {
	FIL tmpFile;
	FRESULT f_res;
	UINT fnum;

	SHA256_CTX sha256_ctx;
	uint8_t hash_output[SHA256_BLOCK_SIZE];
	char fileBuf[256];

	if (hashOutput == NULL) {
		printf("%-20s arg err!!\r\n", "[printerController.c]");
		return;
	}

	f_res = f_open(&tmpFile, curFileName, FA_READ);
	if (f_res != FR_OK) {
		f_close(&tmpFile);
		printf("%-20s Failed to open file: %s\r\n", "[printerController.c]", curFileName);
		return;
	}
	if (f_size(&tmpFile) <= 0) {
		f_close(&tmpFile);
		printf("%-20s file has no content\r\n", "[printerController.c]");
		return;
	}
	sha256_init(&sha256_ctx);

	while (f_read(&tmpfile, fileBuf, sizeof(fileBuf), &fnum) == FR_OK) {
		sha256_update(&sha256_ctx, fileBuf, fnum);
	}
	sha256_final(&sha256_ctx, hash_output);
	f_close(&tmpFile);

	// 轉換為十六進位字串
	for (int j = 0; j < SHA256_BLOCK_SIZE; j++) {
		sprintf(hashOutput + (j * 2), "%02x", hash_output[j]);
	}
	hashOutput[SHA256_BLOCK_SIZE - 1] = '\0';
}
