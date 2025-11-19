#include "ff_print_err.h"

void printf_fatfs_error(FRESULT fresult) {
	switch (fresult) {
		case FR_OK:
			printf("%-20s 》操作成功。\r\n", "[ff_printer_err.c]");
			break;
		case FR_DISK_ERR:
			printf("%-20s ！！硬體輸入輸出驅動錯誤。\r\n", "[ff_printer_err.c]");
			break;
		case FR_INT_ERR:
			printf("%-20s ！！斷言錯誤。\r\n", "[ff_printer_err.c]");
			break;
		case FR_NOT_READY:
			printf("%-20s ！！實體設備無法運作。\r\n", "[ff_printer_err.c]");
			break;
		case FR_NO_FILE:
			printf("%-20s ！！找不到檔案。\r\n", "[ff_printer_err.c]");
			break;
		case FR_NO_PATH:
			printf("%-20s ！！找不到路徑。\r\n", "[ff_printer_err.c]");
			break;
		case FR_INVALID_NAME:
			printf("%-20s ！！無效的路徑名稱。\r\n", "[ff_printer_err.c]");
			break;
		case FR_DENIED:
		case FR_EXIST:
			printf("%-20s ！！拒絕存取。\r\n", "[ff_printer_err.c]");
			break;
		case FR_INVALID_OBJECT:
			printf("%-20s ！！無效的檔案或路徑。\r\n", "[ff_printer_err.c]");
			break;
		case FR_WRITE_PROTECTED:
			printf("%-20s ！！裝置為寫入保護狀態。\r\n", "[ff_printer_err.c]");
			break;
		case FR_INVALID_DRIVE:
			printf("%-20s ！！無效的邏輯裝置。\r\n", "[ff_printer_err.c]");
			break;
		case FR_NOT_ENABLED:
			printf("%-20s ！！無效的工作區域。\r\n", "[ff_printer_err.c]");
			break;
		case FR_NO_FILESYSTEM:
			printf("%-20s ！！無效的檔案系統。\r\n", "[ff_printer_err.c]");
			break;
		case FR_MKFS_ABORTED:
			printf("%-20s ！！格式化操作因參數錯誤中止。\r\n", "[ff_printer_err.c]");
			break;
		case FR_TIMEOUT:
			printf("%-20s ！！操作逾時。\r\n", "[ff_printer_err.c]");
			break;
		case FR_LOCKED:
			printf("%-20s ！！檔案被鎖定。\r\n", "[ff_printer_err.c]");
			break;
		case FR_NOT_ENOUGH_CORE:
			printf("%-20s ！！無法取得足夠堆空間支援長檔名。\r\n", "[ff_printer_err.c]");
			break;
		case FR_TOO_MANY_OPEN_FILES:
			printf("%-20s ！！開啟檔案數量過多。\r\n", "[ff_printer_err.c]");
			break;
		case FR_INVALID_PARAMETER:
			printf("%-20s ！！無效的參數。\r\n", "[ff_printer_err.c]");
			break;
	}
}