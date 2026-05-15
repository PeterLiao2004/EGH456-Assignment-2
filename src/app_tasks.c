/*
 * Barebones application task template.
 *
 * This file provides a minimal FreeRTOS setup for a new project:
 * - a periodic timer interrupt,
 * - a button interrupt,
 * - UART logging tasks that respond to those events.
 */

/* Standard includes. */
#include <stdint.h>
#include <stdbool.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/* Hardware includes. */
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"
#include "driverlib/sysctl.h"
#include "driverlib/timer.h"
#include "drivers/rtos_hw_drivers.h"
#include "utils/uartstdio.h"

/* Application includes. */
#include "motor/motor_tasks.h"
#include "control/control_tasks.h"
#include "sensors/sensor_tasks.h"
#include "ui/ui_tasks.h"

/*-----------------------------------------------------------*/

extern volatile uint32_t g_ui32SysClock;

extern SemaphoreHandle_t xTimerSemaphore;
extern SemaphoreHandle_t xButtonSemaphore;

static volatile uint32_t g_ui32TimeStamp = 0;
static volatile uint32_t g_ui32ButtonPressCount = 0;

/*-----------------------------------------------------------*/

void vCreateAppTasks(void)
{

    /* Create the application tasks. */
    vCreateMotorTasks();
    vCreateControlTasks();
    vCreateSensorTasks();
    vCreateUiTasks();

}
