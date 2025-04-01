/*
 * server.h
 * ----------------------------------------
 * HTTP Server Interface for Stepper Motor Control
 *
 *  Created by: Antonio Andara Lara
 *  Modified by: Antonio Andara Lara | Mar 19, 2024; Mar 15, 2025
 *
 * Description:
 * This header declares the interface for the HTTP server module used to
 * configure and monitor stepper motor parameters in an embedded system.
 *
 * Definitions:
 * - THREAD_STACKSIZE: Stack size for the server task thread (1kB)
 * - RECV_BUF_SIZE:    Buffer size for incoming HTTP requests (2kB)
 * - SERVER_PORT:      TCP port used for HTTP communication (default: 80)
 *
 * Global Variables:
 * - motor_pars: Stores the current stepper motor parameters
 * - button_queue, motor_queue: Shared FreeRTOS queues used by tasks
 *
 */

#ifndef SERVER_H
#define SERVER_H

#include "lwip/sockets.h"
#include "netif/xadapter.h"
#include "lwipopts.h"
#include "xil_printf.h"
#include "FreeRTOS.h"
#include "task.h"
#include "stepper.h"

#define THREAD_STACKSIZE 	1024
#define RECV_BUF_SIZE 		2048
#define SERVER_PORT 		80

// Globals
motor_parameters_t motor_pars;
extern QueueHandle_t button_queue;
extern QueueHandle_t motor_queue;

// Function prototypes
void server_application_thread();
void process_query_string(const char* query, motor_parameters_t* params);
int write_to_socket(int sd, const char* send_buf);
int parse_query_parameter(const char* name, const char* value, motor_parameters_t* motor_parameters);

#endif
