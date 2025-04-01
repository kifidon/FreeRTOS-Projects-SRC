/*
 * gpio.h
 * ----------------------------------------
 * GPIO Interface for Button and LED Tasks
 *
 * Created by: Antonio Andara Lara
 * Modified by: Antonio Andara Lara | Mar 19, 2024; Mar 15, 2025
 *
 * Description:
 * Header file for GPIO-related tasks and resources. Declares FreeRTOS tasks for
 * handling pushbutton input and LED animations based on motor control state.
 *
 * Definitions:
 * - DELAY_50_MS: 50 millisecond delay in FreeRTOS ticks
 * - BUTTONS_CHANNEL: GPIO channel used for button input
 *
 * Globals:
 * - buttons, green_leds: GPIO instances
 * - Queues for button, emergency, and LED control
 *
 * Functions:
 * - pushbutton_task(): Monitors button input and sends signals to queues
 * - led_task(): Animates LEDs based on motor step mode
 */

#ifndef GPIO_H
#define GPIO_H

#include "xgpio.h"
#include "shared_resources.h"

#define DELAY_50_MS pdMS_TO_TICKS(50)
#define BUTTONS_CHANNEL 1

XGpio buttons, green_leds;

extern QueueHandle_t button_queue;
extern QueueHandle_t emergency_queue;
extern QueueHandle_t led_queue;

void pushbutton_task(void *p);
void led_task(void *p);

#endif
