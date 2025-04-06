/*
 * gpio.c
 * ----------------------------------------
 * GPIO Task Implementations for Button and LED Control
 *
 * Created by: Antonio Andara Lara
 * Modified by: Antonio Andara Lara | Mar 19, 2024; Mar 15, 2025
 *
 * Description:
 * This file defines FreeRTOS tasks for:
 * - pushbutton_task: Reads button states and sends them to the appropriate queues.
 * - led_task: Displays an LED animation based on the motor step mode.
 */

#include "stepper.h"
#include "gpio.h"
#include <stdbool.h>

#define BTN0_MASK 0x01
#define POLLING_PERIOD pdMS_TO_TICKS(100)
extern volatile bool emergencyActive = false;



void pushbutton_task(void *p)
{
    u8 button_val;
    u8 last_button_val = 0;
    int btn0_press_duration = 0;
    const u8 emergencySignal = 1;  // signal value sent to emergency_queue

    while(1) {
        // Read current button states
        button_val = XGpio_DiscreteRead(&buttons, BUTTONS_CHANNEL);

        // Check BTN0 (assume mask is 0x01)
        if (button_val & 0x01) {
            btn0_press_duration++;
        } else {
            btn0_press_duration = 0; // reset counter if BTN0 not pressed
        }

        // Trigger emergency if BTN0 pressed for 3 consecutive polling cycles
        if(btn0_press_duration == 3){
            xil_printf("Emergency!!! Sending signal...\n");

            // Send emergency signal
            if (xQueueSend(emergency_queue, &emergencySignal, 0) != pdPASS) {
                xil_printf("Failed to send emergency signal!\n");
            }
        }

        // Save current state as last state for next loop
        last_button_val = button_val;

        // Delay task to avoid flooding the output
        vTaskDelay(POLLING_PERIOD);
    }
}





void led_task(void *p)
{
    u8 step_mode = 0;
    int index = 0; // To keep track of the current step in the animation sequence

    while(1) {
	/* --------------------------------------------------*/
    	// TODO: receive from led_queue into step_mode
    	if (xQueueReceive(led_queue, &step_mode, 0) == pdPASS) {
    		xil_printf("LED task received step mode: %d\n", step_mode);
    	    index = 0; // Reset
    	}
	/* --------------------------------------------------*/
        if ( step_mode > 2 ) {
        	index = 0;
            vTaskDelay(pdMS_TO_TICKS(100));
        } else {
            switch (step_mode) {
                case WAVE_DRIVE:
                    // Cycle through Wave Drive patterns
                    switch (index++ % 4) { // Modulo 4 for 4 steps
                        case 0: XGpio_DiscreteWrite(&green_leds, 1, WAVE_DRIVE_1); break;
                        case 1: XGpio_DiscreteWrite(&green_leds, 1, WAVE_DRIVE_2); break;
                        case 2: XGpio_DiscreteWrite(&green_leds, 1, WAVE_DRIVE_3); break;
                        case 3: XGpio_DiscreteWrite(&green_leds, 1, WAVE_DRIVE_4); break;
                    } break;
                case FULL_STEP:
                    // Cycle through Full Step patterns
                    switch (index++ % 4) { // Modulo 4 for 4 steps
                        case 0: XGpio_DiscreteWrite(&green_leds, 1, FULL_STEP_1); break;
                        case 1: XGpio_DiscreteWrite(&green_leds, 1, FULL_STEP_2); break;
                        case 2: XGpio_DiscreteWrite(&green_leds, 1, FULL_STEP_3); break;
                        case 3: XGpio_DiscreteWrite(&green_leds, 1, FULL_STEP_4); break;
                    } break;
                case HALF_STEP:
                    // Cycle through Half Step patterns
                    switch (index++ % 8) { // Modulo 8 for 8 steps
                        case 0: XGpio_DiscreteWrite(&green_leds, 1, HALF_STEP_1); break;
                        case 1: XGpio_DiscreteWrite(&green_leds, 1, HALF_STEP_2); break;
                        case 2: XGpio_DiscreteWrite(&green_leds, 1, HALF_STEP_3); break;
                        case 3: XGpio_DiscreteWrite(&green_leds, 1, HALF_STEP_4); break;
                        case 4: XGpio_DiscreteWrite(&green_leds, 1, HALF_STEP_5); break;
                        case 5: XGpio_DiscreteWrite(&green_leds, 1, HALF_STEP_6); break;
                        case 6: XGpio_DiscreteWrite(&green_leds, 1, HALF_STEP_7); break;
                        case 7: XGpio_DiscreteWrite(&green_leds, 1, HALF_STEP_8); break;
                    } break;
            }
            vTaskDelay(pdMS_TO_TICKS(250));
        }
    }
}
