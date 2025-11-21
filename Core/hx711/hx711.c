/*
 * HX711.c
 *
 *  Created on: 16 nov. 2021
 *      Author: PCov3r
 *  Modified on: 21 nov. 2025
 *      - Reverted to floating-point arithmetic for simplicity and accuracy.
 *      - Simplified API, assuming single-channel usage.
 */

#include "HX711.h"
#include <stdio.h>
#include "cmsis_os.h"
#include "task.h"
#include <stdbool.h>

#include "gpio.h"

hx711_t hx711;

// Forward declaration
void Hx711_MspInit(hx711_t *hx711, GPIO_TypeDef *SCK_GPIOx, uint16_t SCK_GPIO_Pin, GPIO_TypeDef *DT_GPIOx, uint16_t DT_GPIO_Pin);

void Hx711_Init(hx711_t *hx711) {
    // Initialize GPIO pins
    Hx711_MspInit(hx711, GPIOB, GPIO_PIN_14, GPIOB, GPIO_PIN_15);

    // Set default gain
    Hx711_SetGain(hx711, 128);

    // Set default scale and offset, then perform initial tare
    Hx711_SetScale(hx711, 417.960449f); // Default scale, should be calibrated
    Hx711_SetOffset(hx711, 0.0f);

    printf("%-20s Auto-taring on initialization...\r\n", "[hx711.c]");
    
    // Check if HX711 is connected before attempting tare
    osDelay(2000); // Brief wait for stabilization
    if (Hx711_IsReady(hx711)) {
        Hx711_Tare(hx711, 5); // Reduced times for faster init
        printf("%-20s Initialization complete. Offset: %ld\r\n", "[hx711.c]", (long)hx711->Offset);
    } else {
        printf("%-20s HX711 not detected, skipping tare\r\n", "[hx711.c]");
    }
}

void Hx711_MspInit(hx711_t *hx711, GPIO_TypeDef *SCK_GPIOx, uint16_t SCK_GPIO_Pin, GPIO_TypeDef *DT_GPIOx, uint16_t DT_GPIO_Pin) {
    hx711->SCK_GPIOx = SCK_GPIOx;
    hx711->SCK_GPIO_Pin = SCK_GPIO_Pin;
    hx711->DT_GPIOx = DT_GPIOx;
    hx711->DT_GPIO_Pin = DT_GPIO_Pin;

    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Pin = hx711->SCK_GPIO_Pin;
    HAL_GPIO_Init(hx711->SCK_GPIOx, &gpio);

    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    gpio.Pin = hx711->DT_GPIO_Pin;
    HAL_GPIO_Init(hx711->DT_GPIOx, &gpio);
}

void Hx711_SetScale(hx711_t *hx711, float scale) {
    hx711->Scale = scale;
}

void Hx711_SetOffset(hx711_t *hx711, float offset) {
    hx711->Offset = offset;
}

void Hx711_SetGain(hx711_t *hx711, uint8_t gain) {
    switch (gain) {
        case 128:
            hx711->currentGain = 1;
            break;
        case 64:
            hx711->currentGain = 3;
            break;
        case 32:
            hx711->currentGain = 2;
            break;
        default:
            hx711->currentGain = 1; // Default to 128
            break;
    }
}

static uint8_t Hx711_ShiftIn(hx711_t *hx711) {
    uint8_t value = 0;
    for (int i = 0; i < 8; ++i) {
        HAL_GPIO_WritePin(hx711->SCK_GPIOx, hx711->SCK_GPIO_Pin, GPIO_PIN_SET);
        value |= HAL_GPIO_ReadPin(hx711->DT_GPIOx, hx711->DT_GPIO_Pin) << (7 - i);
        HAL_GPIO_WritePin(hx711->SCK_GPIOx, hx711->SCK_GPIO_Pin, GPIO_PIN_RESET);
    }
    return value;
}

bool Hx711_IsReady(hx711_t *hx711) {
    return HAL_GPIO_ReadPin(hx711->DT_GPIOx, hx711->DT_GPIO_Pin) == GPIO_PIN_RESET;
}

long Hx711_Read(hx711_t *hx711) {
    // Check if HX711 is connected by testing if DT line is responsive
    // If DT is stuck high for too long, assume device is disconnected
    uint16_t timeout_counter = 0;
    const uint16_t TIMEOUT_MS = 100;
    
    while (!Hx711_IsReady(hx711)) {
        osDelay(1);
        timeout_counter++;
        if (timeout_counter >= TIMEOUT_MS) {
            // Device not responding, return a special error value
            return 0x7FFFFFFF; // Max positive value to indicate error
        }
    }

    unsigned long value = 0;
    uint8_t data[3] = {0};

    taskENTER_CRITICAL();

    // Pulse the clock pin 24 times to read the data.
    for (int i = 2; i >= 0; i--) {
        data[i] = Hx711_ShiftIn(hx711);
    }

    // Set the gain for the next reading.
    for (unsigned int i = 0; i < hx711->currentGain; i++) {
        HAL_GPIO_WritePin(hx711->SCK_GPIOx, hx711->SCK_GPIO_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(hx711->SCK_GPIOx, hx711->SCK_GPIO_Pin, GPIO_PIN_RESET);
    }

    taskEXIT_CRITICAL();

    // Sign extension
    if (data[2] & 0x80) {
        value = 0xFF000000 | ((unsigned long)data[2] << 16) | ((unsigned long)data[1] << 8) | (unsigned long)data[0];
    } else {
        value = ((unsigned long)data[2] << 16) | ((unsigned long)data[1] << 8) | (unsigned long)data[0];
    }

    return (long)value;
}

static long Hx711_ReadAverage(hx711_t *hx711, uint8_t times) {
    long sum = 0;
    uint8_t valid_reads = 0;
    
    for (uint8_t i = 0; i < times; i++) {
        long reading = Hx711_Read(hx711);
        
        // Check if device returned error value
        if (reading == 0x7FFFFFFF) {
            return 0x7FFFFFFF; // Propagate error
        }
        
        sum += reading;
        valid_reads++;
        osDelay(5);
    }
    
    if (valid_reads == 0) {
        return 0x7FFFFFFF; // No valid reads
    }
    
    return sum / valid_reads;
}

long Hx711_GetValue(hx711_t *hx711, uint8_t times) {
    return Hx711_ReadAverage(hx711, times);
}

void Hx711_Tare(hx711_t *hx711, uint8_t times) {
    printf("%-20s Tare start...\r\n", "[hx711.c]");
    long sum = Hx711_ReadAverage(hx711, times);
    
    // Check if device is disconnected
    if (sum == 0x7FFFFFFF) {
        printf("%-20s Tare failed: HX711 not responding\r\n", "[hx711.c]");
        Hx711_SetOffset(hx711, 0.0f);
        return;
    }
    
    Hx711_SetOffset(hx711, (float)sum);
    printf("%-20s Tare done. Offset: %ld\r\n", "[hx711.c]", (long)hx711->Offset);
}

float Hx711_GetWeight(hx711_t *hx711, uint8_t times) {
    if (hx711->Scale == 0.0f) {
        return 0.0f; // Avoid division by zero
    }
    long raw_value = Hx711_GetValue(hx711, times);
    
    // Check if device returned error value (timeout/disconnected)
    if (raw_value == 0x7FFFFFFF) {
        return 0.0f; // Return 0 when device is not connected
    }
    
    return (float)(raw_value - hx711->Offset) / hx711->Scale;
}

void HX711_Calibration(void) {
    /*
     * NOTE: Manual calibration function.
     * To use:
     * 1. Uncomment the call to this function in your main application.
     * 2. Follow the on-screen instructions.
     * 3. Copy the calculated 'Scale' value and update Hx711_SetScale() in Hx711_Init().
    */

    printf("\r\n%-20s --- HX711 Calibration ---\r\n", "[hx711.c]");
    printf("%-20s Ensure the scale is empty.\r\n", "[hx711.c]");
    printf("%-20s Press USER button to start taring...\r\n", "[hx711.c]");
    // Assuming USER_KEY is defined elsewhere, otherwise replace with your button logic
    // while (HAL_GPIO_ReadPin(USER_KEY_GPIO_PORT, USER_KEY_PIN) == GPIO_PIN_SET);

    osDelay(1000); // Debounce

    Hx711_Tare(&hx711, 20);

    printf("%-20s Place a known weight (%fg) on the scale.\r\n", "[hx711.c]", KNOWN_WEIGHT_VALUE_G);
    printf("%-20s Press USER button when ready...\r\n", "[hx711.c]");
    while (HAL_GPIO_ReadPin(USER_KEY_GPIO_PORT, USER_KEY_PIN) == GPIO_PIN_RESET);

    osDelay(1000); // Debounce

    long raw_reading = Hx711_GetValue(&hx711, 20);
    float new_scale = (float)(raw_reading - hx711.Offset) / KNOWN_WEIGHT_VALUE_G;

    printf("%-20s Calibration complete!\r\n", "[hx711.c]");
    printf("%-20s Raw reading with weight: %ld\r\n", "[hx711.c]", raw_reading);
    printf("%-20s Calculated Scale: %f\r\n", "[hx711.c]", new_scale);
    printf("%-20s -> Please update Hx711_SetScale() with this value.\r\n", "[hx711.c]");

    Hx711_SetScale(&hx711, new_scale);
}
