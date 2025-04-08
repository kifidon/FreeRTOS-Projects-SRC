/*
 * lab4_main.c
 * ----------------------------------------
 * Stepper Motor Control - Main Source File
 *
 * Created by: Shyama Gandhi        | Mar 24, 2021
 * Modified by: Antonio Andara Lara | Mar 19, 2024; Mar 15, 2025
 *
 * Description:
 * This file contains the main logic for controlling a stepper motor using
 * FreeRTOS tasks and Xilinx hardware peripherals. The system includes the
 * following tasks:
 *
 * - stepper_control_task:
 *   Receives motor parameters via a queue and configures the stepper motor
 *   using functions from stepper.c. Executes absolute motion and sends visual
 *   feedback to the LED task.
 *
 * - pushbutton_task:
 *   Monitors the state of pushbuttons and triggers corresponding events.
 *
 * - emergency_task:
 *   Executes an emergency stop when Btn0 is pressed for three consecutive
 *   polling cycles. The motor decelerates to a stop, and a red LED flashes at
 *   2 Hz to indicate the emergency state.
 *
 * - led_task:
 *   Controls visual feedback using on-board LEDs based on system state.
 *
 * - network task (inside main_thread):
 *   Allows the user to configure motor parameters via a web interface.
 *   Supports up to 25 (target position, dwell time) pairs and communicates
 *   them to stepper_control_task through a queue.
 *
 * Hardware Used:
 * - PMOD for motor signals (JC PMOD)
 * - AXI GPIOs for buttons, LEDs, and motor control
 * - UART for debug output
 *
 * Motor Parameters:
 * - Current Position (steps)
 * - Final Position (steps)
 * - Rotational Speed (steps/sec)
 * - Acceleration / Deceleration (steps/sec�)
 * - Dwell Time (ms)
 *
 */


#include <stdbool.h>
#include "network.h"
#include "stepper.h"
#include "gpio.h"

#define BUTTONS_DEVICE_ID 	XPAR_AXI_GPIO_INPUTS_DEVICE_ID
#define GREEN_LED_DEVICE_ID XPAR_GPIO_1_DEVICE_ID
#define GREEN_LED_CHANNEL 1
#define MOTOR_DEVICE_ID   	XPAR_GPIO_2_DEVICE_ID
#define RGB_LED_ID XPAR_AXI_GPIO_LEDS_DEVICE_ID
#define RGB_CHANNEL 2
#define RGB_RED 0b100 //red?

typedef struct
{
    u8 color:3;   // 3-bit color value representing RGB
    u8 frequency; // Blink frequency of the LED
    u8 dutyCycle; // Duty cycle percentage for brightness control
    bool state;   // State of the LED: ON or OFF
} RgbLedState;

// Initialize LED state
RgbLedState RGBState = { .color = 1, .frequency = 0, .dutyCycle = 100, .state = false };

//Declaring Device
XGpio RGB;

// TODO: add macro for polling period
#define POLLING_PERIOD pdMS_TO_TICKS(100)

static void stepper_control_task( void *pvParameters );
static void emergency_task( void *pvParameters );
static void toggleLED(void *pvParameters);
int Initialize_UART();

motor_parameters_t motor_parameters;
TaskHandle_t motorTaskHandle = NULL;
TaskHandle_t togleledHandle = NULL;

QueueHandle_t button_queue    = NULL;
QueueHandle_t motor_queue     = NULL;
QueueHandle_t emergency_queue = NULL;
QueueHandle_t led_queue       = NULL;
QueueHandle_t rgb_queue       = NULL;

// Helper function to toggle the RGB LED state.
void toggleRgbLed(bool *ledState)
{
    if (*ledState) {
        // Turn off the LED
        XGpio_DiscreteWrite(&RGB, RGB_CHANNEL, 0);
        *ledState = false;
    } else {
        // Turn on the LED with red color
        XGpio_DiscreteWrite(&RGB, RGB_CHANNEL, RGB_RED);
        *ledState = true;
    }
}

int main()
{
    int status;

    // Initialization of motor parameter values
	motor_parameters.current_position = 0;
	motor_parameters.final_position   = 0;
	motor_parameters.dwell_time       = 0;
	motor_parameters.rotational_speed = 0.0;
	motor_parameters.rotational_accel = 0.0;
	motor_parameters.rotational_decel = 0.0;

    button_queue    = xQueueCreate(1, sizeof(u32));
    led_queue       = xQueueCreate(1, sizeof(u8));
    rgb_queue       = xQueueCreate(1, sizeof(RgbLedState));
    motor_queue     = xQueueCreate( 25, sizeof(motor_parameters_t) );
    emergency_queue = xQueueCreate(1, sizeof(u8));

    configASSERT(led_queue);
    configASSERT(emergency_queue);
    configASSERT(button_queue);
	configASSERT(motor_queue);

	// Initialize the PMOD for motor signals (JC PMOD is being used)
	status = XGpio_Initialize(&pmod_motor_inst, MOTOR_DEVICE_ID);

	if(status != XST_SUCCESS){
		xil_printf("GPIO Initialization for Stepper Motor unsuccessful.\r\n");
		return XST_FAILURE;
	}

	//Initialize the UART
	status = Initialize_UART();

	if (status != XST_SUCCESS){
		xil_printf("UART Initialization failed\n");
		return XST_FAILURE;
	}

	// Initialize GPIO buttons
    status = XGpio_Initialize(&buttons, BUTTONS_DEVICE_ID);

    if (status != XST_SUCCESS) {
        xil_printf("GPIO Initialization Failed\r\n");
        return XST_FAILURE;
    }

    XGpio_SetDataDirection(&buttons, BUTTONS_CHANNEL, 0xFF);

    // TODO: Initialize green LEDS
    status = XGpio_Initialize(&green_leds, GREEN_LED_DEVICE_ID);

    if (status != XST_SUCCESS) {
        xil_printf("Green LED Initialization Failed\r\n");
        return XST_FAILURE;
    }

    XGpio_SetDataDirection(&green_leds, GREEN_LED_CHANNEL, 0x00);

    //Initialize RGB
    XGpio_Initialize(&RGB, RGB_LED_ID);
	XGpio_SetDataDirection(&RGB, 2, 0x00);

	xTaskCreate( stepper_control_task
			   , "Motor Task"
			   , configMINIMAL_STACK_SIZE*10
			   , NULL
			   , DEFAULT_THREAD_PRIO + 1
			   , &motorTaskHandle
			   );

    xTaskCreate( pushbutton_task
    		   , "PushButtonTask"
			   , THREAD_STACKSIZE
			   , NULL
			   , DEFAULT_THREAD_PRIO + 1
			   , NULL
			   );

    xTaskCreate( emergency_task
			   , "EmergencyTask"
			   , THREAD_STACKSIZE
			   , NULL
			   , DEFAULT_THREAD_PRIO
			   , NULL
			   );

    xTaskCreate( led_task
			   , "LEDTask"
			   , THREAD_STACKSIZE
			   , NULL
			   , DEFAULT_THREAD_PRIO
			   , NULL
			   );

    sys_thread_new( "main_thrd"
				  , (void(*)(void*))main_thread
				  , 0
				  , THREAD_STACKSIZE
				  , DEFAULT_THREAD_PRIO + 1
				  );

    vTaskStartScheduler();
    while(1);
    return 0;
}

static void stepper_control_task( void *pvParameters )
{
	u32 loops=0;
	const u8 stop_animation = 0;
	long motor_position = 0;

	stepper_pmod_pins_to_output();
	stepper_initialize();

	while(1){
		// get the motor parameters from the queue. The structure "motor_parameters" stores the received data.
		while(xQueueReceive(motor_queue, &motor_parameters, 0)!= pdPASS){
			vTaskDelay(POLLING_PERIOD); // polling period
		}
		xil_printf("\nreceived a package on motor queue. motor parameters:\n");
		stepper_set_speed(motor_parameters.rotational_speed);
		stepper_set_accel(motor_parameters.rotational_accel);
		stepper_set_decel(motor_parameters.rotational_decel);
		stepper_set_pos(motor_parameters.current_position);
		stepper_set_step_mode(motor_parameters.step_mode);
		xil_printf("\npars:\n");
		xQueueSend(led_queue, &motor_parameters.step_mode, 0);
		xil_printf("Sent step mode %d to LED task\n", motor_parameters.step_mode);
		motor_position = stepper_get_pos();
		stepper_move_abs(motor_parameters.final_position);
		xQueueSend(led_queue, &stop_animation, 0);
		motor_position = stepper_get_pos();
		xil_printf("finished on position: %lli", motor_position);
		vTaskDelay(motor_parameters.dwell_time);
		loops++;
		xil_printf("\n\nloops: %d\n", loops);
	}
}


static void emergency_task(void *pvParameters)
{
    u8 emergency = 0;

    for (;;) {
        if (xQueueReceive(emergency_queue, &emergency, portMAX_DELAY) == pdPASS) {
            // Trigger graceful stop
            if (motorTaskHandle != NULL) {
		stepper_setup_stop();
                stepper_disable_motor();
		emergencyActive = true;
		xTaskCreate( toggleLED
		   , "toggleLED"
		   , THREAD_STACKSIZE
		   , NULL
		   , DEFAULT_THREAD_PRIO
		   , &togleledHandle
		   );
		while(stepper_get_speed() > 0){
			vTaskDelay(POLLING_PERIOD);
		}
                vTaskDelete(motorTaskHandle);
                motorTaskHandle = NULL;
            } else {
            	vTaskDelete(togleledHandle);
            	xTaskCreate( stepper_control_task
		   , "Motor Task"
		   , configMINIMAL_STACK_SIZE*10
		   , NULL
		   , DEFAULT_THREAD_PRIO + 1
		   , &motorTaskHandle
		   );
            	emergencyActive = false;
            	bool rgbLedOn = true;
            	toggleRgbLed(&rgbLedOn);
            }



        }
        vTaskDelay(POLLING_PERIOD);
    }
}

static void toggleLED(void *pvParameters){
	TickType_t lastWakeTime = xTaskGetTickCount();
	const TickType_t delay250 = pdMS_TO_TICKS(250);
	bool rgbLedOn = true;
	while(1){
		lastWakeTime = xTaskGetTickCount();
		toggleRgbLed(&rgbLedOn);
	    vTaskDelayUntil(&lastWakeTime, delay250);
	}
}



