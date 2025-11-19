#include "UITask.h"
#include "FramewinDLG.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"

void touchTask(void *argument) {
	while (1) {
		GUI_TOUCH_Exec();
		osDelay(10);
	}
}

void GUI_Task(void *argument) {
	CreateFramewin();       // �I�s GUIBuilder ���ͪ��D����
	// uint16_t i = 0;
	while (1) {
		GUI_CURSOR_Show();
		GUI_Exec();
		GUI_Delay(10);        // �w����s GUI
	}
}