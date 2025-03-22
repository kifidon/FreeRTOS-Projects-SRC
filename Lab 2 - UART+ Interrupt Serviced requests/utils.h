/*
 * utils.h
 *
 *  Created on: Jan 17, 2025
 *      Author: Antonio Andara Lara
 */

#ifndef SRC_UTILS_H_
#define SRC_UTILS_H_

#include "timers.h"

#define MAX_USERS 3
#define MAX_LEN         32
#define HASH_LENGTH     32
#define HASH_STR_SIZE   ((2 * HASH_LENGTH)+1)
typedef uint8_t BYTE;

/*PROTOYPES*/
void receiveInput(char *buffer, int bufferSize);
void getParameter(char* name, char* value);
void vLogoutTimerCallback(TimerHandle_t xTimer);
void vUartCommandTask(void *pvParameters);
void vHashingTask(void *pvParameters);
void vLoginTask(void *pvParameters);

/* Structure for login data from the user */
typedef struct {
    char username[MAX_LEN];
    char password[MAX_LEN];
} LoginData;

typedef struct
{
	char type;
    char action;
} Message;

/* Structure for a registered user (only the hash is stored) */
typedef struct {
    char hashString[HASH_STR_SIZE];
} RegisteredUser;

/* User array */
RegisteredUser registeredUsers[MAX_USERS] = {
    { "AFE3AA268EB3DDFA31EA649273D418A7F956AE6B4ACFA6AA97E33669C3DC5DD8" },
    { "6398A8330C3D259C2660CE89D10E506E7045ED89F7FA33604440E4E7550397CB" }
};


int registeredUserCount = 2;

bool loggedIn = false;

TickType_t xPollPeriod = 100U;

#endif /* SRC_UTILS_H_ */
