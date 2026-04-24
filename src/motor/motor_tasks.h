#ifndef MOTOR_TASKS_H
#define MOTOR_TASKS_H

#include <stdbool.h>

/* Motor control task header file. */
void vCreateMotorTasks(void);

/* Motor control API. Implementations will live in the motor module. */
void Motor_Init(void);
void Motor_Enable(void);
void Motor_Disable(void);

void Motor_Kickstart(void);
void Motor_SetDuty(float duty);

float Motor_GetSpeedRPM(void);
float Motor_GetDuty(void);
bool Motor_HasValidHallFeedback(void);
bool Motor_IsStopped(void);

#endif /* MOTOR_TASKS_H */
