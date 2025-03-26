// Include FreeRTOS Libraries
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

// Include xilinx Libraries
#include "xparameters.h"
#include "xgpio.h"
#include "xscugic.h"
#include "xil_exception.h"
#include "xil_printf.h"
#include "xil_cache.h"
#include "semphr.h"

// Other miscellaneous libraries
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include "stdbool.h"
#include "pmodkypd.h"
#include "sleep.h"
#include "PmodOLED.h"
#include "OLEDControllerCustom.h"


#define BTN_DEVICE_ID  XPAR_INPUTS_DEVICE_ID
#define BTN_CHANNEL    1
#define KYPD_DEVICE_ID XPAR_KEYPAD_DEVICE_ID
#define KYPD_BASE_ADDR XPAR_KEYPAD_BASEADDR


#define FRAME_DELAY 50000

// keypad key table
#define DEFAULT_KEYTABLE 	"0FED789C456B123A"
#define XLENGTH 5
#define YLENGTH 5
#define MAX_GAME_COLUMNS 6
#define NUM_ATTACK   MAX_GAME_COLUMNS*MAX_GAME_COLUMNS
#define NUM_ENEMIES  MAX_GAME_COLUMNS*MAX_GAME_COLUMNS
#define SPEEDUP 5
#define INITIAL_GAME_SPEED 100
#define MAX_SPEED 25
#define POWERUP 5

#define RGB_CYAN    0b011 // Green + Blue
#define RGB_LED_ID XPAR_AXI_LEDS_DEVICE_ID
#define RGB_CHANNEL 2

typedef struct{
	u8 xCord;
	u8 yCord;
	u8 collision;
	int column;
} GameObject;

typedef struct
{
    u8 color:3;   // 3-bit color value representing RGB
    u8 frequency; // Blink frequency of the LED
    u8 dutyCycle; // Duty cycle percentage for brightness control
    bool state;   // State of the LED: ON or OFF
} RgbLedState;

// Initialize LED state
RgbLedState RGBState = { .color = 1, .frequency = 0, .dutyCycle = 100, .state = false };
GameObject player = {-1, -1, 0, 0};
GameObject enemies[NUM_ENEMIES];
GameObject attack[NUM_ATTACK];
int gameOver = 0;
int gameSpeed = INITIAL_GAME_SPEED;
int score = 0;
int attackPointer = 0;
int enemyPointer = 0;
int activeEnemies = 0;

xSemaphoreHandle enemyMutex; //Potentially sub out for priotiy inversion mutexes
xSemaphoreHandle attackMutex;

// Declaring the devices
XGpio       btnInst;
PmodOLED    oledDevice;
PmodKYPD 	KYPDInst;
XGpio RGB;

// Function prototypes
void initializeScreen();
static void playerTask( void *pvParameters );
static void moveEnemies( void *pvParameters );
static void moveAttack(void *pvParameters);
static void generateEnemies( void *pvParameters );
static void generateAttack( void *pvParameters );
static void updateScreen( void *pvParameters );
static void rgb_led_task(void *pvParameters);
static void usePowerUp(void *pvParameters);
// To change between PmodOLED and OnBoardOLED is to change Orientation
const u8 orientation = 0x0; // Set up for Normal PmodOLED(false) vs normal
                            // Onboard OLED(true)
const u8 invert = 0x0; // true = whitebackground/black letters
                       // false = black background /white letters
u8 keypad_val = 'x';

// Queues
static QueueHandle_t xRgbLedQueue 	    = NULL;
static QueueHandle_t xPowerUpQueue 	    = NULL;

// The Message struct will be used by the command handlers
typedef struct
{
	char type;
    char action;
} Message;

int main()
{
	int status = 0;
	// Initialize Devices
	initializeScreen();
	XGpio_Initialize(&RGB, RGB_LED_ID);
	XGpio_SetDataDirection(&RGB, 2, 0x00);
	// Buttons
	status = XGpio_Initialize(&btnInst, BTN_DEVICE_ID);
	if(status != XST_SUCCESS){
		xil_printf("GPIO Initialization for SSD failed.\r\n");
		return XST_FAILURE;
	}
	XGpio_SetDataDirection (&btnInst, BTN_CHANNEL, 0x0f);


	xil_printf("Initialization Complete, System Ready!\n");

	enemyMutex = xSemaphoreCreateMutex();
	attackMutex = xSemaphoreCreateMutex();

	xTaskCreate( playerTask					/* The function that implements the task. */
			, "Move Player"				/* Text name for the task, provided to assist debugging only. */
			, configMINIMAL_STACK_SIZE		/* The stack allocated to the task. */
			, NULL					/* The task parameter is not used, so set to NULL. */
			, tskIDLE_PRIORITY + 3			/* The task runs at the idle priority. */
			, NULL
			);
	xTaskCreate( moveEnemies 				/* The function that implements the task. */
			, "MoveEnemies"				/* Text name for the task, provided to assist debugging only. */
			, configMINIMAL_STACK_SIZE		/* The stack allocated to the task. */
			, NULL					/* The task parameter is not used, so set to NULL. */
			, tskIDLE_PRIORITY + 3			/* The task runs at the idle priority. */
			, NULL
			);
	xTaskCreate( moveAttack 				/* The function that implements the task. */
			, "moveAttack"				/* Text name for the task, provided to assist debugging only. */
			, configMINIMAL_STACK_SIZE		/* The stack allocated to the task. */
			, NULL					/* The task parameter is not used, so set to NULL. */
			, tskIDLE_PRIORITY  +3 			/* The task runs at the idle priority. */
			, NULL
			);
	xTaskCreate( generateEnemies 				/* The function that implements the task. */
			, "generateEnemies"			/* Text name for the task, provided to assist debugging only. */
			, configMINIMAL_STACK_SIZE		/* The stack allocated to the task. */
			, NULL					/* The task parameter is not used, so set to NULL. */
			, tskIDLE_PRIORITY + 2			/* The task runs at the idle priority. */
			, NULL
			);
	xTaskCreate( generateAttack 				/* The function that implements the task. */
			, "generateAttack"			/* Text name for the task, provided to assist debugging only. */
			, configMINIMAL_STACK_SIZE		/* The stack allocated to the task. */
			, NULL					/* The task parameter is not used, so set to NULL. */
			, tskIDLE_PRIORITY + 2			/* The task runs at the idle priority. */
			, NULL
			);
	xTaskCreate( updateScreen 				/* The function that implements the task. */
			, "updateScreen"			/* Text name for the task, provided to assist debugging only. */
			, configMINIMAL_STACK_SIZE		/* The stack allocated to the task. */
			, NULL					/* The task parameter is not used, so set to NULL. */
			, tskIDLE_PRIORITY+1 			/* The task runs at the idle priority. */
			, NULL
			);
	xTaskCreate( usePowerUp 				/* The function that implements the task. */
			, "usePowerUp"				/* Text name for the task, provided to assist debugging only. */
			, configMINIMAL_STACK_SIZE		/* The stack allocated to the task. */
			, NULL					/* The task parameter is not used, so set to NULL. */
			, tskIDLE_PRIORITY+1 			/* The task runs at the idle priority. */
			, NULL
			);
	xTaskCreate( rgb_led_task 				/* The function that implements the task. */
			, "rgb_led_task"			/* Text name for the task, provided to assist debugging only. */
			, configMINIMAL_STACK_SIZE		/* The stack allocated to the task. */
			, NULL					/* The task parameter is not used, so set to NULL. */
			, tskIDLE_PRIORITY+1 			/* The task runs at the idle priority. */
			, NULL
			);

	xRgbLedQueue 	   = xQueueCreate(1, sizeof(Message));
	configASSERT(xRgbLedQueue);
	xPowerUpQueue 	   = xQueueCreate(1, sizeof(Message));
	configASSERT(xPowerUpQueue);

	vTaskStartScheduler();


   while(1);

   return 0;
}

void initializeScreen()
{
   OLED_Begin(&oledDevice, XPAR_PMODOLED_0_AXI_LITE_GPIO_BASEADDR,
         XPAR_PMODOLED_0_AXI_LITE_SPI_BASEADDR, orientation, invert);
}

void InitializeKeypad()
{
   KYPD_begin(&KYPDInst, KYPD_BASE_ADDR);
   KYPD_loadKeyTable(&KYPDInst, (u8*) DEFAULT_KEYTABLE);
}



void moveGameObject(GameObject *gamer, int direction){


	if(direction == 0 && gamer->column > 0){
		gamer->column-= 1;
		gamer->yCord = gamer->column*YLENGTH;
	}else if(direction == 1 && gamer->column < MAX_GAME_COLUMNS -1){
		gamer->column += 1;
		gamer->yCord = gamer->column*YLENGTH;
	}else if(direction == 2){
		gamer->xCord -=1;
	}
	else if(direction == 3){
			gamer->xCord +=2;
	}


}


int restartGame(){
	for(int i = 0; i < NUM_ENEMIES; i++){
		enemies[i].collision = 0;
		enemies[i].xCord = 128;
		enemies[i].column  = -2;
	}
	for (int i = 0; i < NUM_ATTACK; i++){
		attack[i].collision = 0;
		attack[i].column  = -1;
		attack[i].xCord = 0;
	}
	player.column = MAX_GAME_COLUMNS/2;
	player.yCord = YLENGTH*player.column;
	enemyPointer = 0;
	attackPointer = 0;
	gameSpeed = INITIAL_GAME_SPEED;
	Message message =  {.type = 'x', .action = 'x'};
	xQueueOverwrite(xRgbLedQueue, &message);
	score = 0;
	gameOver = 0;
	return 0;
}

static void generateEnemies(void *pvparameters){
	int enemieIndex;
	int enemyColumn;
	int sleep=0;
	int locationSeed ;
//	int collumnOffset;
	for(int i = 0; i < NUM_ENEMIES; i++){
			attack[i].column = -2;
		}
	while(1){
		if(!gameOver){
			if (xSemaphoreTake(enemyMutex, portMAX_DELAY) == pdTRUE) {
				enemieIndex = enemyPointer;
				locationSeed = rand(); // for debugging value
				enemyColumn = locationSeed % MAX_GAME_COLUMNS;
				if(enemies[enemieIndex].collision == 0){
					enemies[enemieIndex].column = enemyColumn ;
					enemies[enemieIndex].xCord = ccolOledMax-XLENGTH;
					enemies[enemieIndex].yCord = (YLENGTH)*enemyColumn;
					enemies[enemieIndex].collision = 1;
					enemyPointer++;
					activeEnemies++;
					enemyPointer %= NUM_ENEMIES;
				}

				xSemaphoreGive(enemyMutex);
				sleep = (rand() % (XLENGTH)) + XLENGTH; // ensures a new y is generated after the previous enemy has moved enough positions
				vTaskDelay(pdMS_TO_TICKS(gameSpeed*sleep ));
			}
		}
		else{taskYIELD();}

	}
}

static void generateAttack(void *pvparameters)
{
	int attackIndex;
	//initialize attack array
	for(int i = 0; i < NUM_ATTACK; i++){
		attack[i].column = -1;
	}
	while (1)
	{
		if(!gameOver){
			if (xSemaphoreTake(attackMutex, portMAX_DELAY) == pdTRUE)
			{
				attackIndex = attackPointer ;
				if (attack[attackIndex].collision == 0 && player.xCord != -1)
				{ // initial position of attacks
					attack[attackIndex].column = player.column;
					attack[attackIndex].xCord = XLENGTH + 2;
					attack[attackIndex].yCord = attack[attackIndex].column*XLENGTH + MAX_GAME_COLUMNS/2;
					attack[attackIndex].collision = 1;
					attackPointer++;
					attackPointer %= NUM_ATTACK;


				}
				xSemaphoreGive(attackMutex);
			}
			vTaskDelay(pdMS_TO_TICKS(gameSpeed*XLENGTH));
		} else{taskYIELD();}
	}
}

static void moveAttack(void *pvparameters)
{
	OLED_SetDrawMode(&oledDevice, 0);
	Message message =  {.type = 't', .action = 'x'};
	while (1)
	{
		if(!gameOver){
			if (xSemaphoreTake(attackMutex, portMAX_DELAY) == pdTRUE)
			{
				for (int i = 0; i < NUM_ATTACK; i++)
				{
					if (attack[i].collision == 1)
					{
						moveGameObject(&attack[i], 3); // 3 moves up
						if(attack[i].xCord > (ccolOledMax-XLENGTH))
							attack[i].collision = 0; // attack is off screen
					}
				}
				xSemaphoreGive(attackMutex);
				taskYIELD();
				if (xSemaphoreTake(attackMutex, portMAX_DELAY) == pdTRUE && xSemaphoreTake(enemyMutex, portMAX_DELAY) == pdTRUE)
					for (int i = 0; i < NUM_ATTACK; i++)
					{
						for(int j = 0; j < NUM_ENEMIES; j++){
							if (attack[i].column == enemies[j].column &&
									enemies[j].collision ==1  && attack[i].collision == 1
									&& attack[i].xCord >= (enemies[j].xCord + XLENGTH/2) ){
								attack[i].collision = 0;
								enemies[j].collision = 0;
								score += 1;
								activeEnemies --;
								if (gameSpeed>MAX_SPEED){
									gameSpeed-=SPEEDUP;
								}
								if((score % POWERUP )== 0 ) // alert user for power up
									xQueueOverwrite(xRgbLedQueue, &message);

							}
						}
					}
				xSemaphoreGive(attackMutex);
				xSemaphoreGive(enemyMutex);

			}
			vTaskDelay(pdMS_TO_TICKS(gameSpeed));
		} else{taskYIELD();}
	}
}

	u8 gameOverPosition=8;
static void moveEnemies(void *pvparameters){

	while(1){
		if(!gameOver){
			if (xSemaphoreTake(enemyMutex, portMAX_DELAY) == pdTRUE) {
				for(int i = 0; i < NUM_ENEMIES; i++){
					if(enemies[i].collision == 1){
						moveGameObject(&enemies[i], 2); // 2 moves down
					}
				}
				xSemaphoreGive(enemyMutex);
				taskYIELD();
				if (xSemaphoreTake(enemyMutex, portMAX_DELAY) == pdTRUE) {
					for(int i = 0; i < NUM_ENEMIES; i++){
						if(enemies[i].xCord <= gameOverPosition && enemies[i].collision)
							gameOver = 1;
					}
					xSemaphoreGive(enemyMutex);
				}
			}
			vTaskDelay(pdMS_TO_TICKS(gameSpeed));
		}
		else{
			OLED_ClearBuffer(&oledDevice);
			OLED_SetCursor(&oledDevice, 0, 1);
			char output[50];  // Allocate enough space for the message
			sprintf(output, "Game over\tScore: %d", score);
			OLED_PutString(&oledDevice, output);
			OLED_Update(&oledDevice);
			vTaskDelay(pdMS_TO_TICKS(2000));
			gameOver = restartGame();
		}
	}
}

static void playerTask( void *pvParameters )
{
	player.collision = 1;
	player.xCord = 0;
	player.column = MAX_GAME_COLUMNS/2;
	player.yCord = YLENGTH*player.column;
	u8 buttonVal=0;
	u8 previousButtonVal=0;
	Message message =  {.type = 'x', .action = 'x'};
	while(1){
		if(!gameOver){
			previousButtonVal = buttonVal;
			buttonVal = XGpio_DiscreteRead(&btnInst, BTN_CHANNEL);

			if (buttonVal == 1 && previousButtonVal == 0){
				moveGameObject(&player, 0); //  moves Left
	//			player.yCord -= YLENGTH;
			} else  if (buttonVal == 2 && previousButtonVal == 0){
	//			player.yCord += YLENGTH;
				moveGameObject(&player, 1); // move right
			} else if (buttonVal ==4 && previousButtonVal ==0){
				xQueueOverwrite(xPowerUpQueue, &message);
			}
			else{
				vTaskDelay(pdMS_TO_TICKS(10));
			}
		}
		else{ taskYIELD();}
	}
}

static void usePowerUp(void *pvParameters){
	Message message =  {.type = 'x', .action = 'x'};
	int numCleared;
	int numToClear;
	while (1){
		if(xQueueReceive(xPowerUpQueue, &message, portMAX_DELAY) == pdTRUE){
			if(RGBState.state){
//				if (xSemaphoreTake(enemyMutex, portMAX_DELAY) == pdTRUE) {
					numToClear = activeEnemies/2;
					for(int i = enemyPointer; ; i++){
						if(enemies[i%NUM_ENEMIES].collision == 1){
//							score ++;
							numCleared ++;
						}
						enemies[i%NUM_ENEMIES].collision =0;
						activeEnemies--;
						if (numCleared >= numToClear){
							break;
						}
					}
					xQueueOverwrite(xRgbLedQueue, &message);
//				xSemaphoreGive(enemyMutex);
//				}
			}
		}
	}
}
static void rgb_led_task(void *pvParameters){
    Message message = { .type = 'x', .action = 'x' };
    while (1)
        {
            // Process LED control logic
			if(xQueueReceive(xRgbLedQueue, &message, portMAX_DELAY) == pdTRUE)
			{
				// Handle incoming messages to change LED state
				if (message.type == 't')
					RGBState.state = true;
				else{ RGBState.state = false;}
				if (RGBState.state){
					XGpio_DiscreteWrite(&RGB, RGB_CHANNEL, RGBState.color);
				} else {
					// Turn OFF the LED if the state is false
					XGpio_DiscreteWrite(&RGB, RGB_CHANNEL, 0);
				}
			}
        }
}

static void updateScreen(void *pvParameters){
		OLED_SetDrawMode(&oledDevice, 0);
		// Turn automatic updating off
		OLED_SetCharUpdate(&oledDevice, 0);
	while(1){
		OLED_ClearBuffer(&oledDevice);
		// draw player
		OLED_MoveTo(&oledDevice, player.xCord, player.yCord);
		OLED_RectangleTo(&oledDevice, player.xCord+XLENGTH, player.yCord+YLENGTH);
		// draw enemies
		for(int i = 0; i < NUM_ENEMIES; i++){
			if(enemies[i].collision == 1){
				OLED_MoveTo(&oledDevice, enemies[i].xCord, enemies[i].yCord);
				OLED_RectangleTo(&oledDevice, enemies[i].xCord+XLENGTH, enemies[i].yCord+YLENGTH);
			}
		}
		for(int i = 0; i < NUM_ATTACK; i++){
			if(attack[i].collision == 1){
				OLED_MoveTo(&oledDevice, attack[i].xCord, attack[i].yCord);
				OLED_DrawLineTo(&oledDevice, attack[i].xCord+(XLENGTH/2), attack[i].yCord);
			}
		}
		OLED_Update(&oledDevice);
		usleep(FRAME_DELAY);
		// vTaskDelay(pdMS_TO_TICKS(100));

	}
}
