#ifndef FATFS_SDIO_TEST_H
#define FATFS_SDIO_TEST_H

#include <stdio.h>
#include "ff.h"
#include "ff_gen_drv.h"
#include "sd_diskio.h"
#include "bsp_sdio_sdcard.h"
#include "usart.h"
#include "ff_print_err.h"

extern char SDPath[4];					/* SD卡邏輯裝置路徑 */
extern FATFS fs;						/* FatFs檔案系統物件 */
extern FIL file;						/* 檔案物件 */
extern FRESULT f_res;					/* 檔案操作結果 */
extern UINT fnum;						/* 成功讀寫的位元組數 */
extern BYTE ReadBuffer[1024];	        /* 讀取緩衝區 */

void SDIO_FatFs_init(void);

//用FATFS測試讀寫SD卡
void SDIO_FatFs_RW_Test();

#endif //FATFS_SDIO_TEST_H
