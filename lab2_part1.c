/*
 *	Lab 2: Part 2: UART in Polled Mode
 *
 *	ECE 315		: Computer Interfacing
 *  Created on	: July 27, 2021
 *  Author	: Shyama M. Gandhi, Mazen Elbaz
 *  Modified by : Antonio Andara
 *  Modified on	: January 24, 2023
 *  Modified by : Antonio Andara
 *  Modified on	: January 17, 2025
 *
 *     ------------------------------------------------------------------------------------------------------------------------------
 *
 * This FreeRTOS task creates hashes of username and password for secure storage. It expects:
 *   1) A username string.
 *   2) A password string.
 * The code obtains the username and password separately and returns the hash (SHA256) of the concatenation of the username and password
 * using two colons (::) as separator.
  *     ------------------------------------------------------------------------------------------------------------------------------
 */

// FreeRTOS
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

// Xilinx
#include "xuartps.h"
#include "xparameters.h"
#include "xgpio.h"
#include "xscugic.h"
#include "xil_exception.h"
#include "xil_printf.h"
#include "xil_cache.h"

// Misc
#include "sleep.h"
#include "stdbool.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "sha256.h"

// UART macros
#define UART_DEVICE_ID  XPAR_XUARTPS_0_DEVICE_ID
#define UART_BASEADDR 	XPAR_XUARTPS_0_BASEADDR
#define UART_MODE       XUARTPS_OPER_MODE_NORMAL
#define UART_FIFO 		XUARTPS_FIFO_OFFSET

#define MAX_LEN         32
#define HASH_LENGTH 	32
#define QUEUE_LENGTH 	512
#define QUEUE_ITEM_SIZE sizeof(char)


/*************************** Enter your code here ****************************/
// TODO: Declare a global instance of the UART device (XUartPs) and a pointer
//       to its configuration (XUartPs_Config) to be used during initialization.
XUartPs UART;
XUartPs_Config * UART_CONFIG;
/*****************************************************************************/

/*************************** Enter your code here ****************************/
// TODO: Declare xUartInputQueue

/*****************************************************************************/
QueueHandle_t xUserDataQueue;
QueueHandle_t xHashResultQueue;
QueueHandle_t xUartInputQueue;

/*************************** Enter your code here ****************************/
// TODO: Define the UART poll period (in ms)

TickType_t xPollPeriod = 10/portTICK_RATE_MS;
/*****************************************************************************/


// User Data Struct
typedef struct UserData
{
    char username[MAX_LEN];
	char password[MAX_LEN];
	char hashString[512];
	BYTE hash[32];
} UserData;


// Function prototypes
int Intialize_UART(u16 DeviceId, XUartPs uart, XUartPs_Config *Config);
void receiveInput(char *buffer, int bufferSize);
void createUser(char *username, char *password);
void getParameter(char* name, char* value);
void sha256String(const char* input, BYTE output[32]);
void hashToString(BYTE *hash, char *hashString);
void concatenateStrings(const char *str1, const char *str2, char *result, int resultSize);

// FreeRTOS tasks
void vUserCreateTask(void *pvParameters);
void vHashingTask(void *pvParameters);
void vUartInputTask(void *pvParameters);
/*************************** Enter your code here ****************************/
// TODO add the UART task function prototype

/*****************************************************************************/

int main(void)
{
/*************************** Enter your code here ****************************/
// TODO: UART Initialization: use Initialize_UART()

	Intialize_UART(UART_DEVICE_ID, UART, UART_CONFIG);

/*****************************************************************************/

    xTaskCreate(
        vUserCreateTask,
        (const char *)"User Creation",
        configMINIMAL_STACK_SIZE + 1000,
        NULL,
        tskIDLE_PRIORITY + 1,
		NULL
    );
/*************************** Enter your code here ****************************/
// TODO: create the UART task by calling xTaskCreate
    xTaskCreate(
           vUartInputTask,
           (const char *)"User Creation",
           configMINIMAL_STACK_SIZE + 1000,
           NULL,
           tskIDLE_PRIORITY + 1,
   		NULL
       );

/*****************************************************************************/

    xTaskCreate(
		vHashingTask,
		(const char *)"Hashing Task",
		configMINIMAL_STACK_SIZE + 1000,
		NULL,
		tskIDLE_PRIORITY + 3,
		NULL
	);

/*************************** Enter your code here ****************************/
// TODO create and assert xUartInputQueue
    xUartInputQueue  = xQueueCreate(MAX_LEN , sizeof(char));
	configASSERT(xUartInputQueue);
/*****************************************************************************/

    xUserDataQueue  = xQueueCreate( 1, sizeof(UserData));
	xHashResultQueue  = xQueueCreate( 1, sizeof(UserData));

	configASSERT(xUserDataQueue);
	configASSERT(xHashResultQueue);

	xil_printf("Starting ECE 315 Lab 2 application\n");

    vTaskStartScheduler();

    while(1);
    return 0;
}


void vUserCreateTask(void *pvParameters)
{
    UserData userData;
    while(1){
    	xil_printf("\nenter a username and a password to create a hash value for part 2\n");
		getParameter("username", userData.username);
		getParameter("password", userData.password);
		xQueueSend(xUserDataQueue, &userData, 0);

		/*************************** Enter your code here ****************************/
		// TODO: poll xHashResultQueue until a hashed result is available.
		while(xQueueReceive(xHashResultQueue, &userData, 0) != pdPASS){
			vTaskDelay(xPollPeriod);
		}
		/*****************************************************************************/
		xil_printf("\n\nSHA256 Hash of \"%s::%s\" is: %s\n", userData.username, userData.password, userData.hashString);
	}
}


void vHashingTask(void *pvParameters)
{
	UserData userData;
    static char userString[QUEUE_LENGTH];

    while (1){
		/*************************** Enter your code here ****************************/
		// TODO: Fix this so vHashingTask doesn't consume all CPU.
		while (xQueueReceive(xUserDataQueue, &userData, 0) != pdPASS){
			vTaskDelay(xPollPeriod);
		}
		concatenateStrings(userData.username, userData.password, userString, sizeof(userString));
		sha256String(userString, userData.hash);
		hashToString(userData.hash, userData.hashString);
		xQueueOverwrite(xHashResultQueue, &userData);
		/*****************************************************************************/
	}
}


void vUartInputTask(void *pvParameters)
{
	char cReceivedByte;
	while (1) {
		/*************************** Enter your code here ****************************/
		// TODO: write the body of this task to read a character from the UART FIFO
		//       into cReceivedByte and send it to the xUartInputQueue
		while(XUartPs_IsReceiveData(UART_BASEADDR)==0){
			vTaskDelay(xPollPeriod);
		}
		cReceivedByte = XUartPs_ReadReg(UART_BASEADDR,UART_FIFO);
		xQueueSend( xUartInputQueue, &cReceivedByte, 0);


		/*****************************************************************************/
	}
}


void getParameter(char* name, char* value)
{
	xil_printf("%s: ", name);
	receiveInput(value, MAX_LEN);
	xQueueReset(xUartInputQueue);
	xil_printf("%s\n", value);
}


void receiveInput(char *buffer, int bufferSize)
{
    int characters_read = 0;

    while (characters_read < bufferSize - 1){
        if (xQueueReceive(xUartInputQueue, &buffer[characters_read], 0) == pdPASS){
            if (buffer[characters_read] == '\0' || buffer[characters_read] == '\r'){
                break;
            }
            characters_read++;
        }else {
            vTaskDelay(100);
        }
    }
    buffer[characters_read] = '\0';
}


void concatenateStrings(const char *str1, const char *str2, char *result, int resultSize)
{
    if(resultSize > 0) {
        int written = snprintf(result, resultSize, "%s::%s", str1, str2);

        if(written >= resultSize) {
            xil_printf("\nuser string too long\n");
        }
    }
}


int Intialize_UART(u16 DeviceId, XUartPs uart, XUartPs_Config *Config)
{
	int status;
	/*
	 * Initialize the UART driver so that it's ready to use.
	 * Look up the configuration in the config table, then initialize it.
	 */
	Config = XUartPs_LookupConfig(DeviceId);
	if (Config == NULL) {
		return XST_FAILURE;
	}

	status = XUartPs_CfgInitialize(&uart, Config, Config->BaseAddress);
	if (status != XST_SUCCESS){
		return XST_FAILURE;
	}

	// NORMAL UART mode.
	XUartPs_SetOperMode(&uart, UART_MODE);

	return XST_SUCCESS;
}

