#ifndef FF_PRINT_ERR_H
#define FF_PRINT_ERR_H

#include "ff.h"
#include "usart.h"

/**
  * @brief  顯示 FatFS 錯誤代碼對應訊息
  * @param  fresult: 錯誤代碼
  * @retval 無
  */
void printf_fatfs_error(FRESULT fresult);

#endif //FF_PRINT_ERR_H
