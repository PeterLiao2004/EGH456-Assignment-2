/**
 * @file system_state.h
 * @brief High-level operating states for the control state machine.
 */

#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

/**
 * @brief High-level system state owned by the state manager.
 */
typedef enum
{
    /** Motor outputs are disabled and the system is ready for a start request. */
    SYSTEM_STATE_STOPPED = 0,

    /** Motor start has been requested and hall feedback is being validated. */
    SYSTEM_STATE_STARTING,

    /** Motor is running under normal control. */
    SYSTEM_STATE_RUNNING,

    /** Emergency stop has been requested and the system is waiting for rest. */
    SYSTEM_STATE_ESTOP_BRAKING,

    /** Fault condition is latched until the operator acknowledges it. */
    SYSTEM_STATE_FAULT_LATCHED
} SystemState_t;

#endif /* SYSTEM_STATE_H */
