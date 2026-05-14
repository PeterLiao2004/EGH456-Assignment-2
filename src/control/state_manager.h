#ifndef CONTROL_STATE_H
#define CONTROL_STATE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    MOTOR_STATE_IDLE = 0,        // (renamed from STOPPED for clarity)
    MOTOR_STATE_STARTING,
    MOTOR_STATE_RUNNING,
    MOTOR_STATE_ESTOP_BRAKING,
    MOTOR_STATE_FAULT_LATCHED
} MotorState_t;

MotorState_t StateMachine_GetState(void);
void StateMachine_TriggerStart(void);          // From UI Start button
void StateMachine_TriggerEStop(void);          // From safety monitor
void StateMachine_TriggerEStopFromISR(void);   // From ISR
void StateMachine_TriggerStop(void);           // From UI Stop button
void StateMachine_TriggerFaultAck(void);       // From UI Acknowledge

#endif /* CONTROL_STATE_H */
