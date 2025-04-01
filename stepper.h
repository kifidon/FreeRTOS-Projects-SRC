/*
 * stepper.h
 * ----------------------------------------
 * Stepper Motor Driver Interface
 *
 * Created on: Mar 24, 2021
 * Created by: Shyama Gandhi
 * Modified by: Antonio Andara | Mar 19, 2024; Mar 15, 2025
 *
 * Description:
 * Header file for stepper motor control, defining hardware macros,
 * step sequences for different modes, motor parameter structures,
 * and function prototypes. This driver supports WAVE, FULL, and HALF
 * step modes and provides an API for motion planning and real-time control.
 *
 */


#ifndef SRC_STEPPER_H_
#define SRC_STEPPER_H_

#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xparameters.h"
#include "xgpio.h"
#include "xscugic.h"
#include "xil_exception.h"
#include "xil_printf.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "math.h"

/********************** Stepper Motor Patterns **********************/
#define PMOD_MOTOR_DEVICE_ID  XPAR_STEPPER_MOTOR_DEVICE_ID
#define STEPS_PER_REVOLUTION_HALF_DRIVE  4096
#define STEPS_PER_REVOLUTION_FULL_DRIVE  2048

#define WINDINGS_OFF  0b0000

// Wave Drive Mode
#define WAVE_DRIVE_1  0b0001
#define WAVE_DRIVE_2  0b0010
#define WAVE_DRIVE_3  0b0100
#define WAVE_DRIVE_4  0b1000

// Full Step Mode
#define FULL_STEP_1   0b0011
#define FULL_STEP_2   0b0110
#define FULL_STEP_3   0b1100
#define FULL_STEP_4   0b1001

// Half Step Mode
#define HALF_STEP_1   0b0001
#define HALF_STEP_2   0b0011
#define HALF_STEP_3   0b0010
#define HALF_STEP_4   0b0110
#define HALF_STEP_5   0b0100
#define HALF_STEP_6   0b1100
#define HALF_STEP_7   0b1000
#define HALF_STEP_8   0b1001


/**
 * Enumeration for step modes (wave, full, half).
 */
typedef enum {
    WAVE_DRIVE,
    FULL_STEP,
    HALF_STEP
} step_mode_t;

/**
 * Optional struct if you want to store multiple motor parameters
 * or keep them in one place.
 */
typedef struct {
	long      current_position;
    long      final_position;
    long      dwell_time;
    float     rotational_speed;
    float     rotational_accel;
    float     rotational_decel;
    step_mode_t step_mode;
} motor_parameters_t;

// XGpio device
XGpio pmod_motor_inst;

int motor_signal[4];
int step_phase;
int step_dir;
step_mode_t current_step_mode;

float target_speed;      // Desired speed in steps/s
float accel;              // Acceleration in steps/s^2
float decel;              // Deceleration in steps/s^2
float curr_step_time;        // Current period between steps (ms)

long curr_pos;    // Current position in steps
long goal_pos;     // Target position in steps
long stop_margin;      // Steps needed for deceleration

float init_step_time;    // ms (approx. from ramp formula)
float step_interval;    // ms at cruising speed
float next_step_time;       // Next step period in ms
float accel_rate; // us**2
float decel_rate; // us**2

_Bool new_move;
unsigned long last_step_time;

_Bool stepper_update(void);
_Bool stepper_motion_complete(void);

extern QueueHandle_t emergQueue;

// Setup and initialization
void stepper_pmod_pins_to_output(void);
void stepper_initialize(void);
void stepper_set_step_mode(unsigned char new_mode);

// Position/speed configuration
void stepper_set_pos(long pos);
void stepper_setup_stop(void);
void stepper_set_speed(float speed_sps);
void stepper_set_accel(float accel_sps2);
void stepper_set_decel(float decel_sps2);
float stepper_get_speed(void);  // steps per second
long  stepper_get_pos(void);

// Movement
void stepper_move_rel(long steps);
void stepper_setup_relative_move_steps(long distance_steps);
void stepper_setup_move_steps(long absolute_steps);
void stepper_move_abs(long pos);
void stepper_set_next_step(int direction, step_mode_t mode);

// Motor control
void stepper_disable_motor(void);

#endif /* SRC_STEPPER_H_ */
