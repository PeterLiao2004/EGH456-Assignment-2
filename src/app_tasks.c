/*
 * Application task registration.
 *
 * Keep task creation centralized here so each subsystem can expose a simple
 * create/init function and ownership remains clear across the team.
 */

/* Standard includes. */
#include <stdint.h>
#include <stdbool.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"

#include "app_tasks.h"
#include "motor/motor_tasks.h"
#include "control/control_tasks.h"
#include "sensors/sensor_tasks.h"
#include "ui/ui_tasks.h"

void vCreateTasks(void)
{
    vMotorTaskCreate();
    vControlTaskCreate();
    vSensorTaskCreate();
    vUiTaskCreate();
}
