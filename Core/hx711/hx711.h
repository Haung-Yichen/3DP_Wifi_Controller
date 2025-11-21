/*
 * HX711.h
 *
 *  Created on: 16 nov. 2021
 *      Author: PCov3r
 *  Modified on: 21 nov. 2025
 *      - Removed float types, using fixed-point integer math
 *      - Added RTOS-safe critical sections
 *      - Unified naming convention with Hx711_ prefix
 */

#ifndef INC_HX711_H_
#define INC_HX711_H_

#include "stm32f1xx_hal.h"

#define CHANNEL_A 1
#define CHANNEL_B 2

#define KNOWN_WEIGHT_VALUE_G (910.0f) // Known weight in grams for calibration

typedef struct {
	GPIO_TypeDef *SCK_GPIOx;
	uint16_t SCK_GPIO_Pin;
	GPIO_TypeDef *DT_GPIOx;
	uint16_t DT_GPIO_Pin;
	float Offset;
	float Scale;
	uint8_t currentGain;
} hx711_t;

extern hx711_t hx711;

/**
 * @brief Initializes the HX711 module.
 * @param hx711 Pointer to the hx711_t structure.
 */
void Hx711_Init(hx711_t *hx711);

/**
 * @brief Initializes the GPIO pins for the HX711 module.
 * @param hx711 Pointer to the hx711_t structure.
 * @param SCK_GPIOx Pointer to the GPIO port for the SCK pin.
 * @param SCK_GPIO_Pin The GPIO pin for the SCK pin.
 * @param DT_GPIOx Pointer to the GPIO port for the DT pin.
 * @param DT_GPIO_Pin The GPIO pin for the DT pin.
 */
void Hx711_MspInit(hx711_t *hx711, GPIO_TypeDef *SCK_GPIOx, uint16_t SCK_GPIO_Pin, GPIO_TypeDef *DT_GPIOx,
                   uint16_t DT_GPIO_Pin);

/**
 * @brief Sets the gain for the HX711.
 * @param hx711 Pointer to the hx711_t structure.
 * @param gain The gain to set (128, 64, or 32).
 */
void Hx711_SetGain(hx711_t *hx711, uint8_t gain);

/**
 * @brief Sets the offset value.
 * @param hx711 Pointer to the hx711_t structure.
 * @param offset The offset value.
 */
void Hx711_SetOffset(hx711_t *hx711, float offset);

/**
 * @brief Sets the scale factor.
 * @param hx711 Pointer to the hx711_t structure.
 * @param scale The scale factor.
 */
void Hx711_SetScale(hx711_t *hx711, float scale);

/**
 * @brief Tares the scale, setting the current reading as the zero point.
 * @param hx711 Pointer to the hx711_t structure.
 * @param times The number of readings to average for the tare operation.
 */
void Hx711_Tare(hx711_t *hx711, uint8_t times);

/**
 * @brief Gets the weight from the HX711.
 * @param hx711 Pointer to the hx711_t structure.
 * @param times The number of readings to average.
 * @return The calculated weight in grams.
 */
float Hx711_GetWeight(hx711_t *hx711, uint8_t times);

/**
 * @brief Reads the raw value from the HX711.
 * @param hx711 Pointer to the hx711_t structure.
 * @return The raw ADC value.
 */
long Hx711_Read(hx711_t *hx711);

/**
 * @brief Gets the raw value, averaged over a number of readings.
 * @param hx711 Pointer to the hx711_t structure.
 * @param times The number of readings to average.
 * @return The averaged raw value.
 */
long Hx711_GetValue(hx711_t *hx711, uint8_t times);

/**
 * @brief Manual calibration function (for debugging).
 */
void HX711_Calibration(void);

#endif //INC_HX711_H_