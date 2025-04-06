/*
 * server.c
 * ----------------------------------------
 * HTTP Server for Stepper Motor Control
 *
 * Created by: Antonio Andara Lara
 * Modified by: Antonio Andara Lara | Mar 19, 2024; Mar 15, 2025
 */

#include "server.h"
#include "string.h"

#define MIN_POSITION 0
#define MAX_POSITION 2048
#define MIN_DWELL_TIME 0
#define MAX_SPEED 1000
#define MAX_ACCELERATION 1000

void validate_input(motor_parameters_t* motor_pars);

/* Main server application thread */
void server_application_thread()
{
    int sock, new_sd;
    int size, n;
    struct sockaddr_in address, remote;
    char recv_buf[RECV_BUF_SIZE];
    char http_response[1024];
    char direction[20];
    memset(&address, 0, sizeof(address));

    // Create new socket
    if ((sock = lwip_socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        xil_printf("Error creating socket.\r\n");
        return;
    }

    // Address settings
    address.sin_family = AF_INET;
    address.sin_port = htons(SERVER_PORT);
    address.sin_addr.s_addr = INADDR_ANY;

    // Bind socket to address
    if (lwip_bind(sock, (struct sockaddr *)&address, sizeof(address)) < 0) {
        xil_printf("Error on lwip_bind.\r\n");
        return;
    }

    // Start listening
    lwip_listen(sock, 0);
    size = sizeof(remote);

    struct pollfd fds[1];
    fds[0].fd = sock;
    fds[0].events = POLLIN;

    while (1) {
        int ret = poll(fds, 1, 10);
        if (ret > 0) {
            new_sd = lwip_accept(sock, (struct sockaddr *)&remote, (socklen_t *)&size);
            if (new_sd < 0) {
                xil_printf("Error accepting connection.\r\n");
                continue;
            }

            memset(recv_buf, 0, sizeof(recv_buf));
            n = read(new_sd, recv_buf, RECV_BUF_SIZE - 1);
            if (n < 0) {
                xil_printf("Error reading from socket %d, closing.\r\n", new_sd);
                close(new_sd);
                continue;
            }

            recv_buf[n] = '\0';  // Null-terminate

            // Extract only the first line (request line) from the HTTP request.
            char *line_end = strstr(recv_buf, "\r\n");
            if (line_end != NULL) {
                *line_end = '\0';
            }
            xil_printf("Received request line: %s\n", recv_buf);

            // Determine which endpoint is requested.
            if (strncmp(recv_buf, "GET /getParams", 14) == 0) {
                // Process GET /getParams
                if (step_dir > 0) {
                    strcpy(direction, "Clockwise");
                } else if (step_dir < 0) {
                    strcpy(direction, "Counter-Clockwise");
                } else {
                    strcpy(direction, "Stopped");
                }
                snprintf(http_response, sizeof(http_response),
                         "HTTP/1.1 200 OK\r\n"
                         "Content-Type: application/json\r\n"
                         "Connection: close\r\n\r\n"
                         "{"
                           "\"rotational_accel\": %.2f,"
                           "\"rotational_decel\": %.2f,"
                           "\"final_position\": %ld,"
                           "\"rotational_speed\": %.2f,"
                           "\"direction\": \"%s\""
                         "}",
                         motor_pars.rotational_accel,
                         motor_pars.rotational_decel,
                         motor_pars.final_position,
                         motor_pars.rotational_speed,
                         direction);
            } else if (strncmp(recv_buf, "GET /setParams", 14) == 0) {
                // Extract the URL part from the request line.
                char *url_start = recv_buf + 4;  // Skip "GET "
                char *url_end = strchr(url_start, ' ');
                if (url_end) {
                    *url_end = '\0';  // Terminate the URL string
                }
                xil_printf("Clean URL: %s\n", url_start);

                // Process the query string from the clean URL.
                process_query_string(url_start, &motor_pars);
                validate_input(&motor_pars);
                xil_printf("After processing, parameters: cis=%ld, fis=%ld, dt=%ld, rs=%.2f, ra=%.2f, rd=%.2f, sm=%d\n",
                           motor_pars.current_position,
                           motor_pars.final_position,
                           motor_pars.dwell_time,
                           motor_pars.rotational_speed,
                           motor_pars.rotational_accel,
                           motor_pars.rotational_decel,
                           motor_pars.step_mode);

                // Send updated parameters to motor queue.
                xQueueSend(motor_queue, &motor_pars, 0);

                snprintf(http_response, sizeof(http_response),
                         "HTTP/1.1 200 OK\r\n"
                         "Content-Type: application/json\r\n"
                         "Connection: close\r\n\r\n"
                         "{"
                            "\"current_position\": %ld,"
                            "\"final_position\": %ld,"
                            "\"dwell_time\": %ld,"
                            "\"rotational_speed\": %.2f,"
                            "\"rotational_accel\": %.2f,"
                            "\"rotational_decel\": %.2f,"
                            "\"step_mode\": %d"
                         "}",
                         motor_pars.current_position,
                         motor_pars.final_position,
                         motor_pars.dwell_time,
                         motor_pars.rotational_speed,
                         motor_pars.rotational_accel,
                         motor_pars.rotational_decel,
                         motor_pars.step_mode);
            } else {
                // Return 404 for any other request.
                snprintf(http_response, sizeof(http_response),
                         "HTTP/1.1 404 Not Found\r\n"
                         "Content-Type: application/json\r\n"
                         "Connection: close\r\n\r\n"
                         "{\"error\": \"Unknown endpoint\"}");
            }
            // Send the response and close the socket.
            write_to_socket(new_sd, http_response);
            close(new_sd);
        }
    }
}

/* Helper function to write to socket */
int write_to_socket(int sd, const char* buffer)
{
    int nwrote = write(sd, buffer, strlen(buffer));
    if (nwrote < 0) {
        xil_printf("ERROR responding to client. tried = %d, written = %d\r\n",
                   (int)strlen(buffer), nwrote);
        xil_printf("Closing socket %d\r\n", sd);
    }
    return nwrote;
}

/* Process query string: parse name/value pairs into motor_parameters_t */
void process_query_string(const char* query, motor_parameters_t* params)
{
    char name[64];
    char value[64];
    const char* params_start = strchr(query, '?');

    if (!params_start) {
        xil_printf("No query parameters found.\n");
        return;
    }
    params_start++; // move past '?'

    while (1) {
        int bytesRead;
        if (sscanf(params_start, "%63[^=]=%63[^& ]%n", name, value, &bytesRead) == 2) {
            xil_printf("Parsed parameter: %s = %s\n", name, value);
            parse_query_parameter(name, value, params);
            params_start += bytesRead;
            // Skip '&'
            if (*params_start == '&')
                params_start++;
            if (*params_start == '\0')
                break; // End of string
        } else {
            break; // No more valid pairs
        }
    }
}

/* Parse individual name/value pairs into the motor parameters */
int parse_query_parameter(const char* name, const char* value, motor_parameters_t* params)
{
    int recognized = 1;

    if (strcmp(name, "rs") == 0) {
        params->rotational_speed = atof(value);
    } else if (strcmp(name, "ra") == 0) {
        params->rotational_accel = atof(value);
    } else if (strcmp(name, "rd") == 0) {
        params->rotational_decel = atof(value);
    } else if (strcmp(name, "cis") == 0) {
        params->current_position = atol(value);
    } else if (strcmp(name, "fis") == 0) {
        params->final_position = atol(value); // Use atol instead of atof
    } else if (strcmp(name, "sm") == 0) {
        params->step_mode = atoi(value);
    } else if (strcmp(name, "dt") == 0) {
        params->dwell_time = atol(value); // Use atol instead of atof
    } else {
        xil_printf("Unrecognized parameter: %s\r\n", name);
        recognized = 0;
    }

    return recognized;
}

void validate_input(motor_parameters_t* motor_pars) {
    // Current and final position
    if (motor_pars->current_position < MIN_POSITION) {
        motor_pars->current_position = MIN_POSITION;
    }
    if (motor_pars->final_position < MIN_POSITION) {
        motor_pars->final_position = MIN_POSITION;
    }
    if (motor_pars->current_position > MAX_POSITION) {
        motor_pars->current_position %= MAX_POSITION;
    }
    if (motor_pars->final_position > MAX_POSITION) {
        motor_pars->final_position %= MAX_POSITION;
    }

    // Dwell Time
    if (motor_pars->dwell_time < MIN_DWELL_TIME) {
        motor_pars->dwell_time = MIN_DWELL_TIME;
    }

    // Rotational Speed
    if (motor_pars->rotational_speed > MAX_SPEED || motor_pars->rotational_speed < -MAX_SPEED) {
        motor_pars->rotational_speed = (MAX_SPEED) / 2;
    }

    // Acceleration Speed
    if (motor_pars->rotational_accel > MAX_ACCELERATION || motor_pars->rotational_accel < -MAX_ACCELERATION) {
        motor_pars->rotational_accel = (MAX_ACCELERATION) / 2;
    }
    // Deceleration Speed
    if (motor_pars->rotational_decel > MAX_ACCELERATION || motor_pars->rotational_decel < -MAX_ACCELERATION) {
        motor_pars->rotational_decel = (MAX_ACCELERATION) / 2;
    }

    // Step Mode
    if (motor_pars->step_mode > 2 || motor_pars->step_mode < 0) {
        motor_pars->step_mode = 0;
    }
}
