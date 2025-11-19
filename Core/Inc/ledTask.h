#ifndef _LED_TASK_H
#define _LED_TASK_H

#include "bsp_led.h"
#include  "usart.h"

void ledBlinkTask(void *argument) {
	/* USER CODE BEGIN ledBlinkTask */
	while (1) {
		// static uint32_t i = 0;
		// LED_RED; // Turn on red LED
		// osDelay(500); // Delay for 500 ms
		// LED_RGBOFF; // Turn off all LEDs
		// osDelay(500); // Delay for 500 ms
		// printf("%i\r\n", i++);
	}
	/* USER CODE END ledBlinkTask */
}


#endif // !_LED_TASK_H
