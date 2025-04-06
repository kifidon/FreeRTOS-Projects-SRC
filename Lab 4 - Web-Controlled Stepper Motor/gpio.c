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

#include <stdbool.h>
#include "stepper.h"
#include "gpio.h"

extern volatile bool emergency_active;


void pushbutton_task(void *p)
{
	/*u8 button_val, last_button_val= 0;
	int btnPress = 0;

    while(1){
        button_val = XGpio_DiscreteRead(&buttons, BUTTONS_CHANNEL);
        if (button_val != last_button_val){

        	if(button_val & 0x01){
        		btnPress++;
        		if(btnPress >=3){
        			u8 emergencySignal = 1;
        			xQueueSend(emergency_queue, &emergencySignal, 0);
        		}
        	} else{
        		btnPress = 0;
        	}
        	while (xQueueSend(button_queue, &button_val, 0) != pdPASS){
        		vTaskDelay(DELAY_50_MS);
			}
		 --------------------------------------------------
        	// TODO: Detect if Btn0 has been held down for 3 consecutive poll periods
        	// before sending the signal to the emergency_queue.
        	last_button_val = button_val;
        }
        vTaskDelay(DELAY_50_MS);
    }*/

	u8 button_val;
	int emergencyPressCount = 0;
	int resetPressCount = 0;

	while(1) {
		button_val = XGpio_DiscreteRead(&buttons, BUTTONS_CHANNEL);
		xil_printf("Button value: 0x%02X\r\n", button_val); // Debug: show raw value

		// Check if Btn0 (bit 0) is pressed for emergency
		if (button_val & 0x01) {
		    // Only increment if below threshold; trigger only once per press.
		    if (emergencyPressCount < 3) {
		        emergencyPressCount++;
		        xil_printf("Emergency Press Count: %d\r\n", emergencyPressCount);
		        if (emergencyPressCount == 3) {
		            u8 emergencySignal = 1;
		            xQueueSend(emergency_queue, &emergencySignal, 0);
		        }
		    }
		} else {
		    emergencyPressCount = 0;  // Reset counter when button is released
		}


		// Check if Btn1 (bit 1) is pressed for manual reset
		if (button_val & 0x02) {
			resetPressCount++;
			xil_printf("Reset Press Count: %d\r\n", resetPressCount); // Debug: count value
			if (resetPressCount >= 3) {
				// Clear the emergency flag to resume normal operation
				emergency_active = false;
				xil_printf("Manual reset activated. System resuming normal operation.\r\n");
				resetPressCount = 0;
			}
		} else {
			resetPressCount = 0;
		}

		// Send the current button value to the button_queue (if needed)
		while (xQueueSend(button_queue, &button_val, 0) != pdPASS) {
			vTaskDelay(pdMS_TO_TICKS(POLLING_PERIOD_MS));
		}
		vTaskDelay(pdMS_TO_TICKS(POLLING_PERIOD_MS));
	}
}


void led_task(void *p)
{
    u8 step_mode = 0;
    int index = 0; // To keep track of the current step in the animation sequence
    bool red_led_on = false;

    while(1) {

    	if (emergency_active) {
			// Flash the red LED on and off at 2 Hz
			if (red_led_on) {
				XGpio_DiscreteWrite(&RGB, RGB_CHANNEL, 0x00); // Turn off
				red_led_on = false;
			} else {
				XGpio_DiscreteWrite(&RGB, RGB_CHANNEL, 0x01); // Turn on
				red_led_on = true;
			}
			vTaskDelay(pdMS_TO_TICKS(250));
			continue;
		}

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
