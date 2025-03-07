/*
 * Lab 1, Part 3 â€“ Additional Peripheral Interfaces
 *
 * Created on:    9 January, 2024
 * Modified on:   20 January, 2025
 *
 * Authors:       Antonio Andara Lara
 *
 *
 * Summary:
 * - Integrate keypad, seven-segment display, push buttons, switches, RGB, and green LEDs.
 * - Use FreeRTOS tasks.
 * - Employ queues for inter-task communication.
 * - Implement a custom command in command task and corresponding logic in the peripheral tasks.
 *
 * Deliverables:
 * - Correctly process keypad inputs.
 * - Display last two keys on the 7-seg display.
 * - Execute commands upon button press to control RGB and green LEDs.
 */


//Include FreeRTOS Libraries
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

//Include xilinx Libraries
#include "xparameters.h"
#include "xgpio.h"
#include "xscugic.h"
#include "xil_exception.h"
#include "xil_printf.h"

//Other miscellaneous libraries
#include "pmodkypd.h"
#include "sleep.h"
#include "xil_cache.h"
#include "stdbool.h"
#include "stdio.h"
#include "stdlib.h"

// Device ID declarations
#define SSD_DEVICE_ID   XPAR_AXI_SSD_DEVICE_ID
#define KYPD_DEVICE_ID  XPAR_AXI_KEYPAD_DEVICE_ID
#define RGB_DEVICE_ID	XPAR_AXI_LEDS_DEVICE_ID
#define LEDS_DEVICE_ID	XPAR_AXI_LEDS_DEVICE_ID
#define BTN_DEVICE_ID	XPAR_AXI_INPUTS_DEVICE_ID
#define SW_DEVICE_ID	XPAR_AXI_INPUTS_DEVICE_ID

// Device channels
#define SSD_CHANNEL		1
#define KYPD_CHANNEL	1
#define LEDS_CHANNEL	1
#define RGB_CHANNEL		2
#define BTN_CHANNEL		1
#define SW_CHANNEL		2

// Button press values
#define BTN0 1
#define BTN1 2
#define BTN2 4
#define BTN3 8

// keypad key table
#define DEFAULT_KEYTABLE "0FED789C456B123A"

// miscellaneous
#define SSD_DELAY     10
#define COMMAND_DELAY 50
#define DELAY_500 	  500

/* Device declarations */
XGpio ssdGpio, rgbLedGpio, buttonGpio, switchGpio, greenLedGpio;
PmodKYPD keypadPmod;

/* task declarations */
static void KeypadInputTask         (void *pvParameters);
static void SevenSegmentDisplayTask (void *pvParameters);
static void CommandProcessorTask    (void *pvParameters);
static void RgbLedControllerTask    (void *pvParameters);
static void GreenLedControllerTask  (void *pvParameters);

/* queue declarations */
static QueueHandle_t xSevenSegmentQueue = NULL;
static QueueHandle_t xCommandQueue      = NULL;
static QueueHandle_t xRgbLedQueue 	    = NULL;
static QueueHandle_t xGreenLedQueue 	    = NULL;
/*************************** Enter your code here ****************************/
// TODO: Declare the Green LED queue.

/*****************************************************************************/


// The Message struct will be used by the command handlers
typedef struct
{
	char type;
    char action;
} Message;

/* Function prototypes */
void InitializeKeypad();
u32 SSD_decode(u8 key_value, u8 cathode);
static void HandleE7Command(Message* message);
static void HandleA5Command(Message* message);
static void Handle58Command(Message* message);
static void Handle11Command(Message* message);
/*************************** Enter your code here ****************************/
// TODO: Prototype for custom command handler.

/*****************************************************************************/
static void HandleUnknownCommand(const char* command);

int main(void)
{
	int status;

	/* Device Initialization*/
	// Keypad
	InitializeKeypad();

	// SSD
	status = XGpio_Initialize(&ssdGpio, SSD_DEVICE_ID);
	if(status != XST_SUCCESS){
		xil_printf("GPIO Initialization for SSD failed.\r\n");
		return XST_FAILURE;
	}

	// RGB LED
	status = XGpio_Initialize(&rgbLedGpio, RGB_DEVICE_ID);
	if(status != XST_SUCCESS){
		xil_printf("GPIO Initialization for RGB LED failed.\r\n");
		return XST_FAILURE;
	}

	// Buttons
	status = XGpio_Initialize(&buttonGpio, BTN_DEVICE_ID);
	if(status != XST_SUCCESS){
		xil_printf("GPIO Initialization for buttons failed.\r\n");
		return XST_FAILURE;
	}

	// Switches
	status = XGpio_Initialize(&switchGpio, SW_DEVICE_ID);
	if(status != XST_SUCCESS){
		xil_printf("GPIO Initialization for switches failed.\r\n");
		return XST_FAILURE;
	}

	// Green leds
	status = XGpio_Initialize(&greenLedGpio, LEDS_DEVICE_ID);
	if(status != XST_SUCCESS){
		xil_printf("GPIO Initialization for green LEDs failed.\r\n");
		return XST_FAILURE;
	}

	/* Device data direction: 0 for output 1 for input */
	XGpio_SetDataDirection(&ssdGpio, SSD_CHANNEL, 0x00);
	XGpio_SetDataDirection(&greenLedGpio, LEDS_CHANNEL, 0x00);
	XGpio_SetDataDirection(&rgbLedGpio, RGB_CHANNEL, 0x00);
	XGpio_SetDataDirection(&buttonGpio, BTN_CHANNEL, 0x0F);
	XGpio_SetDataDirection(&switchGpio, SW_CHANNEL, 0x0F);

	/* Task creation */
    xTaskCreate( KeypadInputTask,		  // The function that implements the task.
                "main task", 			  // Text name for the task, provided to assist debugging only.
                configMINIMAL_STACK_SIZE, // The stack allocated to the task.
                NULL, 					  // The task parameter is not used, so set to NULL.
                tskIDLE_PRIORITY,		  // The task runs at the idle priority.
                NULL );                   // Optional task's handle

    xTaskCreate( SevenSegmentDisplayTask,
				"ssd task",
				configMINIMAL_STACK_SIZE,
				NULL,
				tskIDLE_PRIORITY,
				NULL );

    xTaskCreate( CommandProcessorTask,
                "command task",
                configMINIMAL_STACK_SIZE,
                NULL,
                tskIDLE_PRIORITY+1,
                NULL );

    xTaskCreate( RgbLedControllerTask,
                "rgb led task",
                configMINIMAL_STACK_SIZE,
                NULL,
                tskIDLE_PRIORITY,
                NULL );

    xTaskCreate( GreenLedControllerTask,
                "green leds task",
                configMINIMAL_STACK_SIZE,
                NULL,
                tskIDLE_PRIORITY,
                NULL );

    /* Queue creation */
    xSevenSegmentQueue = xQueueCreate(1, sizeof(char));
    xCommandQueue      = xQueueCreate(1, sizeof(char[3]));
    xRgbLedQueue 	   = xQueueCreate(1, sizeof(Message));
/*************************** Enter your code here ****************************/
	// TODO: Create the Green LED queue.
    xGreenLedQueue	   = xQueueCreate(1, sizeof(Message));

/*****************************************************************************/

    /* Assert queue creation */
    configASSERT(xSevenSegmentQueue);
    configASSERT(xCommandQueue);
	configASSERT(xRgbLedQueue);
/*************************** Enter your code here ****************************/
	// TODO: Assert the Green LED queue creation.
	configASSERT(xGreenLedQueue);

/*****************************************************************************/


	xil_printf(
        "\n====== App Ready ======\n"
        "Input commands using the 16-key keypad.\n"
        "Press 'BTN0' to execute.\n"
        "========================\n\n"
        );

    /* Start Scheduler */
    vTaskStartScheduler();

    while(1);
    return 0;
}

// Keypad initialization function
void InitializeKeypad()
{
   KYPD_begin(&keypadPmod, XPAR_AXI_KEYPAD_BASEADDR);
   KYPD_loadKeyTable(&keypadPmod, (u8*) DEFAULT_KEYTABLE);
}

// This function translates key value codes to their binary representation
u32 SSD_decode(u8 key_value, u8 cathode)
{
    u32 result;

    // key_value is the ASCII code of the pressed key
	// The switch statement maps each ASCII code to the corresponding
	// 7-segment display encoding. The 7-segment display encoding is
	// represented as a binary number where each bit corresponds to a segment.
	// A bit value of 1 means the segment is on, and 0 means it's off.
    switch(key_value){
        case 48: result = 0b00111111; break; // 0
        case 49: result = 0b00110000; break; // 1
        case 50: result = 0b01011011; break; // 2
        case 51: result = 0b01111001; break; // 3
        case 52: result = 0b01110100; break; // 4
        case 53: result = 0b01101101; break; // 5
        case 54: result = 0b01101111; break; // 6
        case 55: result = 0b00111000; break; // 7
        case 56: result = 0b01111111; break; // 8
        case 57: result = 0b01111100; break; // 9
        case 65: result = 0b01111110; break; // A
        case 66: result = 0b01100111; break; // B
        case 67: result = 0b00001111; break; // C
        case 68: result = 0b01110011; break; // D
        case 69: result = 0b01001111; break; // E
        case 70: result = 0b01001110; break; // F
        default: result = 0b00000000; break; // Undefined, all segments are OFF
    }

    // cathode determines which of the two 7-segment displays is active.
	// - A cathode value of 1 activates the right display
	// - A cathode value of 0 activates the left display
	// The Most Significant Bit (MSB) is used as the control bit to select the display.
	// The MSB is set to 1 for the right display and left as 0 for the left display.
    if(cathode==0){
    	return result; // MSB is 0, left display active
    }
    else {
    	return result | 0b10000000; // MSB is set to 1, right display active
    }
}

/**
 * This task is responsible for continuously monitoring the state of a keypad
 * and sending the detected key presses to a queue for further processing.
 **/
static void KeypadInputTask( void *pvParameters )
{
   u16 keystate;
   XStatus status, last_status = KYPD_NO_KEY;
   u8 new_key='0';

   while (1){
	  // Reading the keypad state
	  keystate = KYPD_getKeyStates(&keypadPmod);
	  status = KYPD_getKeyPressed(&keypadPmod, keystate, &new_key);

	  // Sending key presses using the queue
	  if(status == KYPD_SINGLE_KEY && last_status == KYPD_NO_KEY){
		  xQueueOverwrite(xSevenSegmentQueue, &new_key);
	  } else if (status == KYPD_MULTI_KEY && status != last_status){
		  xil_printf("Error: Multiple keys pressed\r\n");
	  }

	  // updating last_status
      last_status = status;

      // Delay to throttle the loop
      vTaskDelay(pdMS_TO_TICKS(COMMAND_DELAY));
   }
}


/*
 * This task is responsible for displaying characters on a seven-segment display (SSD)
 * based on key presses received from a queue and sending them to another command task.
 */
static void SevenSegmentDisplayTask( void *pvParameters )
{
    u8 current_key = 'x';
    u32 ssd_value = 0; // Value to be displayed on the SSD
    char command[3] = "xx\0"; // Array to hold the command for the command task

    while(1){
        // Attempt to receive a key press from the queue
        if(xQueueReceive(xSevenSegmentQueue, &current_key, 0) == pdTRUE){
            if(current_key == 'r') {
                // If 'r' is received, reset the current and previous keys
                command[0] = 'x';
                command[1] = 'x';
            } else {
                // Update the command for the command task
                command[0] = command[1];
                command[1] = current_key;
            }

            // Send the command to the command task queue
            xQueueOverwrite(xCommandQueue, &command);
        }

        // Display the current key on the SSD
		ssd_value = SSD_decode(command[1], 1);
		XGpio_DiscreteWrite(&ssdGpio, SSD_CHANNEL, ssd_value);
		vTaskDelay(pdMS_TO_TICKS(SSD_DELAY)); // Delay for persistence of vision

		// Display the previous key (now the current key after shifting) on the SSD
        ssd_value = SSD_decode(command[0], 0);
        XGpio_DiscreteWrite(&ssdGpio, SSD_CHANNEL, ssd_value);
        vTaskDelay(pdMS_TO_TICKS(SSD_DELAY)); // Delay for persistence of vision
    }
}


static void CommandProcessorTask( void *pvParameters )
{
	char command[3] = {'x', 'x', '\0'};
	const char RESET_CHAR = 'r';
	unsigned int buttonVal=0, lastButtonVal=0;
	Message message = {.type = 'x', .action = 'x'};

	while(1){

        xQueueReceive(xCommandQueue, &command, 0);
        buttonVal = XGpio_DiscreteRead(&buttonGpio, 1);

        if(lastButtonVal == 0 && buttonVal == BTN0){
            if(strcmp(command, "E7") == 0){
            	HandleE7Command(&message);
            	 xQueueOverwrite(xSevenSegmentQueue, &RESET_CHAR);
			} else if(strcmp(command, "A5") == 0){
				HandleA5Command(&message);
				 xQueueOverwrite(xSevenSegmentQueue, &RESET_CHAR);
/*************************** Enter your code here ****************************/
			// TODO: Add the condition to handle your custom command
			//       and call its corresponding handler function here.
			}else if(strcmp(command, "58") == 0){
				Handle58Command(&message);
/*****************************************************************************/
            } else {
            	HandleUnknownCommand(command);
            }


            vTaskDelay(pdMS_TO_TICKS(DELAY_500)); // Delay after finish

        }else if(lastButtonVal == 0 && buttonVal == BTN1){
        	if(strcmp(command, "58") == 0){
				Handle11Command(&message);
				 xQueueOverwrite(xSevenSegmentQueue, &RESET_CHAR);
/*****************************************************************************/
			} else {
				HandleUnknownCommand(command);
			}
        }

        lastButtonVal = buttonVal;
        // Delay to throttle the loop
        vTaskDelay(pdMS_TO_TICKS(COMMAND_DELAY));

	}
}


static void GreenLedControllerTask( void *pvParameters )
{
	u8 greenLedsValue = 0;
	Message message = { .type = 'x', .action = 'x'};
	int step = 0;
	int stepR2 = 3;
	int stepR1 = 1;
	int round = 1;
	int shift = 1;

	while(1){
/*************************** Enter your code here ****************************/
		// TODO: Wait until a message is received from the Green LED queue.

        xQueueReceive(xGreenLedQueue, &message, 0);
/*****************************************************************************/
		switch(message.type){
            case 'a': // set the green LEDs to the values of the switches
				greenLedsValue = XGpio_DiscreteRead(&switchGpio, 2);
				break;

            case 's':
            	if(step < 0|| step > 3){
            		shift *=-1;
            		step += shift;
            	}
            	greenLedsValue = '\001' << step;
            	step += shift;
            	vTaskDelay(pdMS_TO_TICKS(100));
			    break;

            case 'r':

            	if(1 == round % 2){
            		greenLedsValue = stepR1;
            		stepR1 *= 2;
            		if (stepR1 > 8){
						stepR1 = 1;
					}
            		if(round >= 7){
            			round = 1;
            		}
            		else {
            			round++;
            		}

            	}
				else if(0 == round % 2){
					greenLedsValue = stepR2;
					stepR2 *=2;
					if (stepR2 > 12){
						stepR2 = 3;
					}
					if(round >= 7){
						round = 1;
					}
					else {
						round++;
					}
				}
				else {
					greenLedsValue = 0;
				}
				vTaskDelay(pdMS_TO_TICKS(250));
                break;

            case 'Q':
            	greenLedsValue = 0;
            	break;
        }
   		XGpio_DiscreteWrite(&greenLedGpio, 1, greenLedsValue);
	}
}


static void RgbLedControllerTask(void *pvParameters)
{
    typedef struct
    {
        u8 color:3;   // 3-bit color value representing RGB
        u8 frequency; // Blink frequency of the LED
        u8 dutyCycle; // Duty cycle percentage for brightness control
        bool state;   // State of the LED: ON or OFF
    } RgbLedState;

    // Initialize LED state
    RgbLedState RGBState = { .color = 1, .frequency = 0, .dutyCycle = 100, .state = false };
    Message message = { .type = 'x', .action = 'x' };

    while (1)
    {
        // Handle incoming messages to change LED state
        switch (message.type)
        {
            case 't': // Toggle LED state
                RGBState.state = !RGBState.state;
                break;

            case 'c':
                break;

            case 'p':
                break;

            case 'Q':
                break;

            default:
                break;
        }

        // Process LED control logic
        while (xQueueReceive(xRgbLedQueue, &message, 0) == pdFALSE)
        {
            if (RGBState.state){
				XGpio_DiscreteWrite(&rgbLedGpio, RGB_CHANNEL, RGBState.color);
            } else {
                // Turn OFF the LED if the state is false
                XGpio_DiscreteWrite(&rgbLedGpio, RGB_CHANNEL, 0);
            }
        }
    }
}



/****************************************
 *These are the command handler functions
 ****************************************/
static void HandleE7Command(Message* message)
{
    message->type = 't';
    xQueueSend(xRgbLedQueue, message, 0);
    xil_printf("\n----------E7----------\nRGB LED state changed\n");
    xil_printf("-------Finished-------\n");
}


static void HandleA5Command(Message* message)
{
    message->type = 'a';
    xQueueSend(xGreenLedQueue, message, 0);
/*************************** Enter your code here ****************************/
	// TODO: Send the command message to the Green LED controller
	//       by writing to the appropriate queue.

/*****************************************************************************/
    xil_printf("\n----------A5----------\ngreen LEDs values set\n");
    xil_printf("-------Finished-------\n");
}

/*************************** Enter your code here ****************************/
// TODO: Write a command handler function for your custom command.
static void Handle58Command(Message* message){
		message->type = 's';

	    xQueueSend(xGreenLedQueue, message, 0);
	    xil_printf("\n----------58----------\ngreen LED dancing\n");
	    xil_printf("-------Finished-------\n");

}

static void Handle11Command(Message* message){
    message->type = 'r';
    xQueueSend(xGreenLedQueue, message, 0);
    xil_printf("\n----------11----------\ngreen LED walking\n");
    xil_printf("-------Finished-------\n");
}
/*****************************************************************************/

static void HandleUnknownCommand(const char* command)
{
    xil_printf("\n***Command %s is not implemented***\n", command);
}
