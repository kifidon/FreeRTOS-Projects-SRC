/*
 * Lab 1, Part 2 - RGB LED Control with FreeRTOS
 *
 * ECE-315 WINTER 2025 - COMPUTER INTERFACING
 * Created on: January 9, 2025
 * Author(s):  Antonio Andara Lara
 *
 * Summary:
 * 1) Update BSP to 1000 ticks/sec.
 * 2) Toggle RGB LED at varying frequencies to find persistence of vision threshold.
 * 3) Implement a PWM-like task to smoothly transition LED brightness.
 *
 * Deliverables:
 * - Show flicker elimination frequency and period.
 * - Demonstrate smooth brightness transitions.
 */


// Include FreeRTOS Libraries
#include "FreeRTOS.h"
#include <stdio.h>
#include "task.h"

// Include Xilinx Libraries
#include "xgpio.h"

// RGB LED Colors
#define RGB_OFF     0b000
#define RGB_RED     0b100
#define RGB_GREEN   0b010
#define RGB_BLUE    0b001
#define RGB_YELLOW  0b110 // Red + Green
#define RGB_CYAN    0b011 // Green + Blue
#define RGB_MAGENTA 0b101 // Red + Blue
#define RGB_WHITE   0b111

// RGB LED Device ID
#define RGB_LED_ID XPAR_AXI_LEDS_DEVICE_ID
#define RGB_CHANNEL 2

/*************************** Enter your code here ****************************/
    // TODO: Declare RGB LED peripheral
	XGpio RGB;


/*****************************************************************************/

/*************************** Enter your code here ****************************/
    // TODO: Task prototype

/*****************************************************************************/
	static void rgb_led_task(void *pvParameters);

int main(void)
{
    int status;
	XGpio_Initialize(&RGB, RGB_LED_ID);
	XGpio_SetDataDirection(&RGB, 2, 0x00);
/*************************** Enter your code here ****************************/
	// TODO:
	// 1) Configure the RGB LED pins as output.
	// 2) Create the FreeRTOS task for the RGB LED.
	// 3) Start the scheduler.
/*****************************************************************************/
	xTaskCreate(rgb_led_task,					/* The function that implements the task. */
					"main task", 				/* Text name for the task, provided to assist debugging only. */
					configMINIMAL_STACK_SIZE, 	/* The stack allocated to the task. */
					NULL, 						/* The task parameter is not used, so set to NULL. */
					tskIDLE_PRIORITY,			/* The task runs at the idle priority. */
					NULL);

	vTaskStartScheduler();

    while (1);
    return 0;
}


static void rgb_led_task(void *pvParameters)
{
/*************************** Enter your code here ****************************/
    // TODO: Declare a variable of type TickType_t named 'xDelay'.
	TickType_t xDelay = 1;
/*****************************************************************************/
	int freq = 1;
	TickType_t start;
    while (1){
/*************************** Enter your code here ****************************/
    // TODO: Implement a loop that increments xDelay by 1 tick in each iteration,
    //       Allow the loop to run for 3 seconds for each xDelay value.
	//       Use xil_printf to display xDelay and its associated period and frequency
	//       Select a color for the RGB LED.
    	start = xTaskGetTickCount();
    	while (xTaskGetTickCount() - start<=3000){
    		XGpio_DiscreteWrite(&RGB, 2, RGB_GREEN);
    		vTaskDelay(xDelay);
    		XGpio_DiscreteWrite(&RGB, 2, RGB_OFF);
    		vTaskDelay(xDelay);
    	}
    	xil_printf("xDelay: %d\r\n", (int)xDelay );
		freq = 1000 / (xDelay * portTICK_RATE_MS); // Convert to Hz
		xil_printf("Frequency: %d Hz\r\n", freq);
		xDelay++;
//		vTaskDelay(pdMS_TO_TICKS(3000));

/*****************************************************************************/
    }
}

/*************************** Enter your code here ****************************/
// TODO: Write the second task to control the duty cycle of the RGB LED signal.

/*****************************************************************************/
