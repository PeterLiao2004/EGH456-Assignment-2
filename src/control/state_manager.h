/**
 * @file state_manager.h
 * @brief Public API for the system control state machine.
 *
 * The state manager owns high-level system state transitions between stopped,
 * starting, running, emergency-stop braking, and fault-latched modes. Other
 * modules should request transitions through the trigger functions rather than
 * modifying state directly.
 */

#ifndef STATE_MANAGER_H
#define STATE_MANAGER_H

#include "system_state.h"

/**
 * @brief Create and initialise the state manager RTOS task.
 *
 * This calls StateManager_Init() before creating the task. It should be called
 * once during application task setup before switch or UI events are expected.
 */
void vCreateStateManagerTasks(void);

/**
 * @brief Initialise state manager synchronization objects and default state.
 *
 * Creates the state mutex, event group, and E-stop blink timer, then places the
 * system in SYSTEM_STATE_STOPPED with all board LEDs off.
 */
void StateManager_Init(void);

/**
 * @brief Request a transition from stopped to starting.
 *
 * The request is posted asynchronously to the state manager task. It has effect
 * only when the current state permits a start transition.
 */
void StateManager_TriggerStart(void);

/**
 * @brief Request an emergency stop.
 *
 * The request is posted asynchronously to the state manager task. If accepted,
 * the state manager enters SYSTEM_STATE_ESTOP_BRAKING and calls Motor_EStop().
 */
void StateManager_TriggerEStop(void);

/**
 * @brief Request acknowledgement of a latched fault.
 *
 * The request is posted asynchronously to the state manager task. It has effect
 * only while the system is in SYSTEM_STATE_FAULT_LATCHED.
 */
void StateManager_TriggerFaultAck(void);

/**
 * @brief Get the current high-level system state.
 *
 * @return Current SystemState_t value.
 */
SystemState_t StateManager_GetState(void);

/**
 * @brief Get a printable name for the current high-level system state.
 *
 * @return Pointer to a static string owned by the state manager module.
 */
const char *StateManager_GetStateString(void);

#endif /* STATE_MANAGER_H */
