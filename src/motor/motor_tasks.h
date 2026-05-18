#ifndef MOTOR_TASKS_H
#define MOTOR_TASKS_H

#include <stdbool.h>
#include <stdint.h>

/* Motor control task header file. */
void vCreateMotorTasks(void);

typedef enum
{
    MOTOR_STATE_STOPPED = 0,
    MOTOR_STATE_RUNNING,
    MOTOR_STATE_STOPPING,
    MOTOR_STATE_ESTOP_BRAKING,
    MOTOR_STATE_FAULT_LATCHED
} MotorState_t;

/* Motor control API. Implementations will live in the motor module. */
void Motor_Init(void);
void Motor_Start(void);
void Motor_Stop(void);

uint32_t Motor_GetSpeed(void);
void Motor_SetSpeed(uint32_t rpm);

void Motor_EStop(void);
void Motor_ClearEStop(void);
bool Motor_IsFaultLatched(void);
MotorState_t Motor_GetState(void);

#endif /* MOTOR_TASKS_H */
