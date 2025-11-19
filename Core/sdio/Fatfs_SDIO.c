#include "Fatfs_SDIO.h"

#define TEST_FILE_NAME "test file.txt"

char SDPath[4]; /* SD卡邏輯裝置路徑 */
FATFS fs; /* FatFs檔案系統物件 */
FIL file; /* 檔案物件 */
FRESULT f_res; /* 檔案操作結果 */
UINT fnum; /* 成功讀寫的位元組數 */
BYTE ReadBuffer[1024] = {0}; /* 讀取緩衝區 */
BYTE WriteBuffer[] = "STM32/SDIO/FATFS讀寫測試，目前波特率2,000,000，新建檔案系統測試檔案\r\n";

void SDIO_FatFs_init(void) {
	if (FATFS_LinkDriver(&SD_Driver, SDPath) == 0) {
		f_res = f_mount(&fs, (TCHAR const *) SDPath, 1);
		printf_fatfs_error(f_res);

		if (f_res == FR_NO_FILESYSTEM) {
			printf("%-20s 》SD卡還沒有檔案系統，即將進行格式化...\r\n", "[Fatfs_SDIO.c]");
			f_res = f_mkfs((TCHAR const *) SDPath, 0, 0);
			if (f_res == FR_OK) {
				printf("%-20s 》SD卡已成功格式化檔案系統。\r\n", "[Fatfs_SDIO.c]");
				f_mount(NULL, (TCHAR const *) SDPath, 1);
				f_mount(&fs, (TCHAR const *) SDPath, 1);
			} else {
				printf("%-20s 《《格式化失敗。》》\r\n", "[Fatfs_SDIO.c]");
				while (1);
			}
		} else if (f_res != FR_OK) {
			printf("%-20s ！！SD卡掛載檔案系統失敗。(%d)\r\n", "[Fatfs_SDIO.c]", f_res);
			printf_fatfs_error(f_res);
			while (1);
		} else {
			printf("%-20s 》檔案系統掛載成功\r\n", "[Fatfs_SDIO.c]");
		}
	}
}

void SDIO_FatFs_RW_Test() {
	printf("%-20s \r\r\n****** 這是一個SD卡 檔案系統實驗 ******\r\r\n", "[Fatfs_SDIO.c]");
	printf("%-20s ****** 即將進行檔案寫入測試... ******\r\n", "[Fatfs_SDIO.c]");
	f_res = f_open(&file, TEST_FILE_NAME, FA_CREATE_ALWAYS | FA_WRITE);
	if (f_res == FR_OK) {
		printf("%-20s 》開啟/創建 %s 檔案成功，向檔案寫入資料。\r\n", "[Fatfs_SDIO.c]", TEST_FILE_NAME);
		f_res = f_write(&file, WriteBuffer, sizeof(WriteBuffer), &fnum);
		if (f_res == FR_OK) {
			printf("%-20s 》檔案寫入成功，寫入位元組數據：%d\r\n", "[Fatfs_SDIO.c]", fnum);
			printf("%-20s 》寫入的資料為：\r\n%s\r\n", "[Fatfs_SDIO.c]", WriteBuffer);
		} else {
			printf("%-20s ！！檔案寫入失敗：(%d)\r\n", "[Fatfs_SDIO.c]", f_res);
		}
		f_close(&file);
	} else {
		printf("%-20s ！！開啟/創建檔案失敗。\r\n", "[Fatfs_SDIO.c]");
	}
	HAL_Delay(10); //不加延遲讀取不穩定
	printf("%-20s ****** 即將進行檔案寫入測試... ******\r\n", "[Fatfs_SDIO.c]");
	f_res = f_open(&file, TEST_FILE_NAME, FA_CREATE_ALWAYS | FA_WRITE);
	if (f_res == FR_OK) {
		printf("%-20s 》開啟/創建 %s 檔案成功，向檔案寫入資料。\r\n", "[Fatfs_SDIO.c]", TEST_FILE_NAME);
		f_res = f_write(&file, WriteBuffer, sizeof(WriteBuffer), &fnum);
		if (f_res == FR_OK) {
			printf("%-20s 》檔案寫入成功，寫入位元組數據：%d\r\n", "[Fatfs_SDIO.c]", fnum);
			printf("%-20s 》寫入的資料為：\r\n%s\r\n", "[Fatfs_SDIO.c]", WriteBuffer);
		} else {
			printf("%-20s ！！檔案寫入失敗：(%d)\r\n", "[Fatfs_SDIO.c]", f_res);
		}
		f_close(&file);
	} else {
		printf("%-20s ！！開啟/創建檔案失敗。\r\n", "[Fatfs_SDIO.c]");
	}
	HAL_Delay(10); //不加延遲讀取不穩定
	printf("%-20s ****** 即將進行檔案讀取測試... ******\r\n", "[Fatfs_SDIO.c]");
	f_res = f_open(&file, TEST_FILE_NAME, FA_OPEN_EXISTING | FA_READ);
	printf_fatfs_error(f_res);
	if (f_res == FR_OK) {
		printf("%-20s 》開啟檔案成功。\r\n", "[Fatfs_SDIO.c]");
		f_res = f_read(&file, ReadBuffer, sizeof(ReadBuffer), &fnum);
		if (f_res == FR_OK) {
			printf("%-20s 》檔案讀取成功，讀取位元組數據：%d\r\n", "[Fatfs_SDIO.c]", fnum);
			printf("%-20s 》讀取的檔案內容為：\r\n%s \r\n", "[Fatfs_SDIO.c]", ReadBuffer);
		} else {
			printf("%-20s ！！檔案讀取失敗：(%d)\r\n", "[Fatfs_SDIO.c]", f_res);
		}
	} else {
		printf("%-20s ！！開啟檔案失敗。\r\n", "[Fatfs_SDIO.c]");
	}
	f_close(&file);

	// f_mount(NULL, (TCHAR const *) SDPath, 1);
	// FATFS_UnLinkDriver(SDPath);
}
