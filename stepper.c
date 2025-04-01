/*
 * stepper.c
 * ----------------------------------------
 * Stepper Motor Driver Implementation
 *
 * Created on: Mar 24, 2021
 * Created by: Shyama Gandhi
 * Modified by: Antonio Andara | Mar 19, 2024; Mar 15, 2025
 *
 * Description:
 * This file contains the implementation of a stepper motor driver adapted
 * from the TinyStepper_28BYJ_48 Arduino library for use in a FreeRTOS-based
 * embedded system. It handles step mode configuration, motion planning
 * (speed, acceleration, deceleration), and low-level stepping logic.
 *
 * Key Functions:
 * - stepper_initialize(): Initializes internal variables
 * - stepper_update(): Manages motion step-by-step
 * - stepper_disable_motor(): Disables motor coils to save power
 * - stepper_set_next_step(): Updates motor coil signals based on step mode
 */


#include "stepper.h"


/*
 * Sets the stepping mode (WAVE, FULL, or HALF).
 */
void stepper_set_step_mode(unsigned char new_mode)
{
    	current_step_mode = (step_mode_t)new_mode;
}

/*
 * Set PMOD pins to output and write initial "off" value.
 */
void stepper_pmod_pins_to_output(void)
{
    XGpio_SetDataDirection(&pmod_motor_inst, 1, 0x00);
    XGpio_DiscreteWrite(&pmod_motor_inst, 1, WINDINGS_OFF);
}

/*
 * Initializes internal driver variables.
 */
void stepper_initialize(void)
{
	motor_signal[0] = 0;
	motor_signal[1] = 0;
	motor_signal[2] = 0;
	motor_signal[3] = 0;

    curr_pos = 0;
    target_speed    = 2048.0f / 4.0f;    // initial speed
    accel           = 2048.0f / 10.0f;   // initial acceleration
    curr_step_time      = 0.0f;
    step_phase       = 0;
}

/*
 * Set current position (in steps) without causing rotation.
 * Call only when the motor is at rest.
 */
void stepper_set_pos(long pos)
{
    curr_pos = pos;
}

/*
 * Get the current position in steps.
 */
long stepper_get_pos(void)
{
    return curr_pos;
}

/*
 * Prepare for a controlled stop by setting a short "target" for deceleration.
 */
void stepper_setup_stop(void)
{
    if (step_dir > 0)
        goal_pos = curr_pos + stop_margin;
    else
        goal_pos = curr_pos - stop_margin;
}

/*
 * Set the target (maximum) speed in steps/s.
 */
void stepper_set_speed(float speed_sps)
{
    target_speed = speed_sps;
}

/*
 * Set the acceleration in steps/s^2.
 */
void stepper_set_accel(float accel_sps2)
{
    accel = accel_sps2;
}

/*
 * Set the deceleration in steps/s^2.
 */
void stepper_set_decel(float decel_sps2)
{
    decel = decel_sps2;
}

/*
 * Move by a relative number of steps (blocking).
 */
void stepper_move_rel(long steps)
{
    stepper_setup_relative_move_steps(steps);
    while (!stepper_update()) {
        // Optionally vTaskDelay(1) or do something else
    }
}

/*
 * Setup a relative move without blocking.
 */
void stepper_setup_relative_move_steps(long distance_steps)
{
    stepper_setup_move_steps(curr_pos + distance_steps);
}

/*
 * Move to an absolute position (in steps), blocking until complete.
 */
void stepper_move_abs(long pos)
{
    stepper_setup_move_steps(pos);
    while (!stepper_update()) {
        vTaskDelay(1);
    }
    stepper_disable_motor();
}


/*
 * Setup move parameters and ramp for an absolute target.
 */
void stepper_setup_move_steps(long absolute_steps)
{
    long step_dist = absolute_steps - curr_pos;
    if (step_dist < 0) {
        step_dist  = -step_dist;
        step_dir = -1;
    } else {
        step_dir = 1;
    }

    // Compute the speed the user *wants*
    float user_speed = target_speed;

    // 1) Compute the speed we *can* reach with the given distance
    //    by checking if we have enough distance to accelerate and decelerate fully:
    //    v_max^2 * (1/(2a) + 1/(2d)) = step_dist
    // => v_max = sqrt( 2*a*d*step_dist / (a + d) )

    float possible_speed = sqrtf( 2.0f * accel * decel * step_dist / (accel + decel) );

    // 2) If the user desired_speed is bigger than the possible max, clamp it down.
    if (user_speed > possible_speed) {
    	printf("\nspeed clamped from %.2f to %.2f\n", user_speed, possible_speed);
        user_speed = possible_speed;
    }

    // Now we have a speed that we can physically reach
    step_interval = 1000.0f / user_speed;  // in ms

    // 3) Set up the ramp initial period, stop_margin, etc. using that clamped speed
    init_step_time = 1000.0f / sqrtf(2.0f * accel);   // first step period
    stop_margin      = (long)roundf( (user_speed * user_speed) / (2.0f * decel) );

    // If step_dist is too small to even do 1 step, handle that corner case
    if (step_dist < 1) {
        step_dist = 1;
    }

    // If not enough distance to fully accelerate+decelerate, do partial logic
    // (some code to half the stop_margin, etc. if you want)
    if (step_dist <= stop_margin * 2L) {
        stop_margin = step_dist / 2L;
    }

    next_step_time    = init_step_time;
    accel_rate = accel / 1e6f;
    decel_rate = decel / 1e6f;
    new_move      = 1;

    // Finally set goal_pos
    goal_pos = absolute_steps;
}


/*
 * Processes movement step-by-step. Call this periodically (or in a loop).
 * Returns TRUE when motion is complete.
 */
_Bool stepper_update(void)
{
    unsigned long current_time;
    unsigned long time_since_last_step;
    long distance_to_target;

    // Already at target?
    if (curr_pos == goal_pos) {
        return 1; // TRUE
    }

    // First call to start this move
    if (new_move) {
        last_step_time = xTaskGetTickCount();
        new_move = 0;
    }

    current_time         = xTaskGetTickCount();
    time_since_last_step = current_time - last_step_time;

    // Not time for next step?
    if (time_since_last_step < (unsigned long)next_step_time) {
        return 0; // FALSE
    }

    // Determine distance to target
    distance_to_target = goal_pos - curr_pos;
    if (distance_to_target < 0) {
        distance_to_target = -distance_to_target;
    }

    // Start deceleration if close enough
    if (distance_to_target <= stop_margin) {
        accel_rate = -decel_rate;
    }

    // Perform the actual step
    stepper_set_next_step(step_dir, current_step_mode);

    // Update position
    curr_pos += step_dir;

    // Store the last actual step period
    curr_step_time = next_step_time;

    // Recompute next step period:
    //   next_step_time *= (1 - accel_rate * next_step_time^2)
    float period_sq = next_step_time * next_step_time;

    next_step_time = next_step_time * (1.0f - (accel_rate * period_sq));

    // Clip to desired speed
    if (next_step_time < step_interval) {
        next_step_time = step_interval;
    }

    // If the step period is still large, do a minimal delay
    if (roundf(next_step_time) > 2) {
        vTaskDelay((TickType_t)next_step_time - 1);
    }

    // Record time
    last_step_time = current_time;

    // Check completion
    if (curr_pos == goal_pos) {
        curr_step_time = 0.0f;
        return 1; // TRUE
    }
    return 0; // FALSE
}


/*
 * Write the coil pattern for the next step.
 */
void stepper_set_next_step(int direction, step_mode_t mode)
{
    static int phase_index = 0;

    // Update phase index
    phase_index += direction;

    // Wrap around behavior
    if (phase_index < 0) {
        phase_index = 3;
    } else if (phase_index > 3){
        phase_index = 0;
     }

    // TODO: Output pattern based on step mode
    if(mode == 0){
		switch (phase_index) {
			case 0: XGpio_DiscreteWrite(&pmod_motor_inst, 1, WAVE_DRIVE_1); break;
			case 1: XGpio_DiscreteWrite(&pmod_motor_inst, 1, WAVE_DRIVE_2); break;
			case 2: XGpio_DiscreteWrite(&pmod_motor_inst, 1, WAVE_DRIVE_3); break;
			case 3: XGpio_DiscreteWrite(&pmod_motor_inst, 1, WAVE_DRIVE_4); break;
		}
    } else if(mode ==1){
    	switch (phase_index) {
			case 0: XGpio_DiscreteWrite(&pmod_motor_inst, 1, FULL_STEP_1); break;
			case 1: XGpio_DiscreteWrite(&pmod_motor_inst, 1, FULL_STEP_2); break;
			case 2: XGpio_DiscreteWrite(&pmod_motor_inst, 1, FULL_STEP_3); break;
			case 3: XGpio_DiscreteWrite(&pmod_motor_inst, 1, FULL_STEP_4); break;
		}
    } else if(mode ==2){
    	switch (phase_index) {
			case 0: XGpio_DiscreteWrite(&pmod_motor_inst, 1, HALF_STEP_1); break;
			case 1: XGpio_DiscreteWrite(&pmod_motor_inst, 1, HALF_STEP_2); break;
			case 2: XGpio_DiscreteWrite(&pmod_motor_inst, 1, HALF_STEP_3); break;
			case 3: XGpio_DiscreteWrite(&pmod_motor_inst, 1, HALF_STEP_4); break;
			case 4: XGpio_DiscreteWrite(&pmod_motor_inst, 1, HALF_STEP_5); break;
			case 5: XGpio_DiscreteWrite(&pmod_motor_inst, 1, HALF_STEP_6); break;
			case 6: XGpio_DiscreteWrite(&pmod_motor_inst, 1, HALF_STEP_7); break;
			case 7: XGpio_DiscreteWrite(&pmod_motor_inst, 1, HALF_STEP_8); break;
    	}
    }
}


/*
 * Disable the motor to save power.
 */
void stepper_disable_motor(void)
{
    XGpio_DiscreteWrite(&pmod_motor_inst, 1, WINDINGS_OFF);
}

/*
 * Return current velocity in steps/s.
 */
float stepper_get_speed(void)
{
    if (curr_step_time == 0.0f) {
        return 0.0f;
    }
    // step_dir sets sign
    return (step_dir > 0)
        ? (1000.0f / curr_step_time)
        : -(1000.0f / curr_step_time);
}


/*
 * Return TRUE if the motor is at its target position.
 */
_Bool stepper_motion_complete(void)
{
    return (curr_pos == goal_pos);
}
