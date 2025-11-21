#include "UITask.h"
#include "FramewinDLG.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"
#include <stdio.h>

void show_boot_animation(void) {
	const int screen_x = LCD_GetXSize();
	const int screen_y = LCD_GetYSize();
	const int bar_width = 200;
	const int bar_height = 15;
	const int bar_x = (screen_x - bar_width) / 2;
	const int bar_y = (screen_y / 2) + 20;

	GUI_SetBkColor(GUI_BLUE);
	GUI_Clear();
	GUI_SetColor(GUI_WHITE);
	GUI_SetFont(&GUI_Font24_ASCII);
	GUI_DispStringHCenterAt("Booting...", screen_x / 2, (screen_y / 2) - 30);
	GUI_SetColor(GUI_WHITE);
	GUI_DrawRect(bar_x, bar_y, bar_x + bar_width, bar_y + bar_height);
	GUI_SetColor(GUI_WHITE);
	for (int i = 1; i <= bar_width - 4; i++) {
		GUI_FillRect(bar_x + 2, bar_y + 2, bar_x + 2 + i, bar_y + bar_height - 2);
		osDelay(10);
	}
	// 動畫完成後清屏為黑色，等待 GUI 任務繪製主界面
	GUI_SetBkColor(GUI_BLACK);
	GUI_Clear();
}

void touchTask(void *argument) {
	while (1) {
		GUI_TOUCH_Exec();
		osDelay(10);
	}
}

void GUI_Task(void *argument) {
	show_boot_animation();
	
	WM_HWIN hWin = CreateFramewin();
	if (hWin == 0) {
		while(1) { osDelay(1000); }
	}
	
	while (1) {
		MainTask();
	}
}