/**
 * @file    motor_tasks.h
 * @brief   Motor subsystem API for the EGH456 electric vehicle project.
 *
 * This module owns the low-level motor mechanics:
 *   - Hall sensor reading and edge counting
 *   - Commutation via the MotorLib
 *   - Speed measurement (RPM) from hall edges
 *   - Speed ramp (acceleration / deceleration limiting)
 *   - PI closed-loop controller
 *   - PWM duty cycle output
 *
 * It is a passive follower of intent. An external controller (the state
 * manager) tells the motor what to do via a small set of intent-based
 * API calls; the motor handles its own ramp rate selection internally.
 *
 * Typical usage by the state manager:
 *   - Entering STARTING:       Motor_Start(SAFE_START_RPM);
 *   - User adjusts speed:      Motor_SetSpeed(user_rpm);
 *   - User requests stop:      Motor_Stop();                  (normal ramp)
 *   - E-stop triggered:        Motor_EStop();                 (immediate cut)
 *   - Entering FAULT_LATCHED:  Motor_Disable();
 *
 * State transitions are detected by polling status helpers:
 *   - STARTING -> RUNNING:           Motor_GetHallEdgeCount() >= threshold
 *   - ESTOP_BRAKING -> FAULT_LATCHED: Motor_HasReachedZero()
 *
 * Telemetry helpers (Motor_GetSpeed, Motor_GetDuty, Motor_GetReferenceRpm)
 * are provided for the SerialPlot output and the UI display.
 *
 * @note  All public Motor_* functions are thread-safe and may be called from
 *        any task. Do not call them from ISRs.
 */

#ifndef MOTOR_TASKS_H
#define MOTOR_TASKS_H

#include <stdbool.h>
#include <stdint.h>

/*-----------------------------------------------------------------------------
 * Task creation
 *---------------------------------------------------------------------------*/

/**
 * @brief Create the motor RTOS task.
 *
 * Must be called once during system initialisation, before
 * vTaskStartScheduler(). The task internally calls Motor_Init() before
 * entering its main loop.
 */
void vCreateMotorTasks(void);

/*-----------------------------------------------------------------------------
 * Lifecycle
 *---------------------------------------------------------------------------*/

/**
 * @brief Initialise motor hardware and internal state.
 *
 * Configures MotorLib with the system PWM period, disables outputs, reads
 * the initial hall sensor states, and resets all measurement and controller
 * state. Leaves the motor in a known safe stopped state.
 *
 * @note Called automatically by the motor task on startup.
 */
void Motor_Init(void);

/*-----------------------------------------------------------------------------
 * Intent-based control API
 *---------------------------------------------------------------------------*/

/**
 * @brief Enable outputs, apply an open-loop commutation kick, and begin
 *        ramping towards an initial target RPM at the normal acceleration rate.
 *
 * Used when transitioning into MOTOR_STATE_STARTING. After the kick, the
 * motor task drives the reference RPM upward at the normal ramp rate while
 * the PI controller tracks it. The state manager can poll
 * Motor_GetHallEdgeCount() to confirm valid hall feedback before
 * transitioning into MOTOR_STATE_RUNNING.
 *
 * @param initial_rpm  Initial target speed in RPM. Clamped to the configured
 *                     speed limits.
 */
void Motor_Start(uint32_t initial_rpm);

/**
 * @brief Change the target speed, ramping at the normal acceleration /
 *        deceleration rate.
 *
 * Used during MOTOR_STATE_RUNNING when the user adjusts the speed slider.
 * Values are clamped to the configured speed limits. Has no effect if the
 * motor outputs are currently disabled (see Motor_Disable()).
 *
 * @param rpm  Desired motor speed in RPM. Pass 0 to decelerate to rest at
 *             the normal rate (equivalent to Motor_Stop()).
 */
void Motor_SetSpeed(uint32_t rpm);

/**
 * @brief Decelerate the motor to zero at the normal deceleration rate.
 *
 * Used for user-requested stops. The reference RPM ramps from its current
 * value to zero at the normal rate. Outputs remain enabled until
 * Motor_Disable() is called, so the PI controller continues to track the
 * ramp during deceleration.
 *
 * @note This does NOT change the operating state - the state manager owns
 *       that. Typically called when the state manager transitions a request
 *       like "set speed to 0".
 */
void Motor_Stop(void);

/**
 * @brief Immediately cut motor output for an emergency stop.
 *
 * Used when transitioning into MOTOR_STATE_ESTOP_BRAKING. This forces duty to
 * zero, disables the motor driver through MotorLib, and clears the controller
 * reference/integrator so no further drive is commanded.
 */
void Motor_EStop(void);

/**
 * @brief Immediately disable motor outputs and reset all internal setpoints.
 *
 * Forces duty to zero, calls stopMotor() on the driver, clears the target
 * RPM, the reference RPM, and the PI integrator. Used when entering
 * MOTOR_STATE_STOPPED or MOTOR_STATE_FAULT_LATCHED.
 *
 * @note Subsequent calls to Motor_SetSpeed() / Motor_Stop() / Motor_EStop()
 *       have no effect until Motor_Start() is called again.
 */
void Motor_Disable(void);

/*-----------------------------------------------------------------------------
 * Status helpers (for the state manager)
 *---------------------------------------------------------------------------*/

/**
 * @brief Get the hall sensor edge count since the last Motor_Start() call.
 *
 * The state manager polls this to detect when valid hall feedback is
 * available during MOTOR_STATE_STARTING. The count is reset internally
 * each time Motor_Start() is called.
 *
 * @return Cumulative hall edge count since the last start.
 */
uint32_t Motor_GetHallEdgeCount(void);

/**
 * @brief Check whether the motor has come to rest after a stop or e-stop.
 *
 * True once both the reference RPM and the measured RPM are zero. The
 * state manager polls this during MOTOR_STATE_ESTOP_BRAKING to detect when
 * the transition to MOTOR_STATE_FAULT_LATCHED is appropriate.
 *
 * @return  true if the motor is fully stopped, false otherwise.
 */
bool Motor_HasReachedZero(void);

/*-----------------------------------------------------------------------------
 * Telemetry (for UI and SerialPlot)
 *---------------------------------------------------------------------------*/

/**
 * @brief Get the latest measured motor speed in RPM.
 *
 * Returns the filtered speed produced by the motor task at the speed
 * sample rate (currently 20 Hz).
 *
 * @return Measured speed in RPM. Returns 0 when the motor is not turning.
 */
uint32_t Motor_GetSpeed(void);

/**
 * @brief Get the current PI controller output (duty cycle).
 *
 * @return Duty cycle in percent, range [0, MOTOR_DUTY_MAX].
 */
uint16_t Motor_GetDuty(void);

/**
 * @brief Get the current ramped reference RPM.
 *
 * This is the controller's instantaneous target, which tracks the desired
 * speed at the active ramp rate. Useful for verifying ramp behaviour on
 * the SerialPlot output.
 *
 * @return Reference RPM.
 */
uint32_t Motor_GetReferenceRpm(void);

/**
 * @brief Get the current target RPM setpoint.
 *
 * The latest value passed via Motor_Start() / Motor_SetSpeed(), or 0 if
 * Motor_Stop() / Motor_EStop() / Motor_Disable() was the last command.
 *
 * @return Target RPM.
 */
uint32_t Motor_GetTargetRpm(void);

#endif /* MOTOR_TASKS_H */
