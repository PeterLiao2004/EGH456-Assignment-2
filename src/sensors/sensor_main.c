/* Sensor main function */

/* Standard includes. */
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/* Hardware includes. */
#include "inc/hw_memmap.h"
#include "inc/hw_sysctl.h"
#include "driverlib/interrupt.h"
#include "driverlib/rom.h"
#include "driverlib/rom_map.h"
#include "driverlib/sysctl.h"
#include "drivers/rtos_hw_drivers.h"

#include "driverlib/gpio.h"
#include "driverlib/uart.h"
#include "driverlib/pin_map.h"
#include "utils/uartstdio.h"

#include "inc/hw_ints.h"
#include "driverlib/debug.h"
#include "driverlib/fpu.h"
#include "driverlib/timer.h"
#include "utils/ustdlib.h"
/*-----------------------------------------------------------*/

/* Global for binary semaphore shared between tasks. */
SemaphoreHandle_t xFastTimerSemaphore = NULL;
SemaphoreHandle_t xSlowTimerSemaphore = NULL;

SemaphoreHandle_t xOpt3001ReadSemaphore = NULL;

SemaphoreHandle_t xOpt3001QueueDropMutex = NULL;

/* Set up the clock and pin configurations to run this example. */
static void prvSetupHardware(void);

extern volatile uint32_t g_ui32SysClock;
/*-----------------------------------------------------------*/

void sensor_main(void)
{
    /* Prepare the hardware to run this example. */
    prvSetupHardware();

    // Create semaphores and mutexes for sensor tasks
    xFastTimerSemaphore = xSemaphoreCreateBinary();
    xSlowTimerSemaphore = xSemaphoreCreateBinary();

    xOpt3001ReadSemaphore = xSemaphoreCreateBinary();
    
    xOpt3001QueueDropMutex = xSemaphoreCreateMutex();
}

/*-----------------------------------------------------------*/

static void prvConfigureTimers(void) {
    /* Timer 0A configs - fast 300Hz timer*/
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
    TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);
    TimerLoadSet(TIMER0_BASE, TIMER_A, g_ui32SysClock / 300 -1); // 300Hz timer

    TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
    IntEnable(INT_TIMER0A);
    TimerEnable(TIMER0_BASE, TIMER_A);

    /* Timer 1A configs - slow 2Hz timer*/
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER1);
    TimerConfigure(TIMER1_BASE, TIMER_CFG_PERIODIC);
    TimerLoadSet(TIMER1_BASE, TIMER_A, g_ui32SysClock / 2 -1); // 2Hz timer

    TimerIntEnable(TIMER1_BASE, TIMER_TIMA_TIMEOUT);
    IntEnable(INT_TIMER1A);
    TimerEnable(TIMER1_BASE, TIMER_A);
}

/*-----------------------------------------------------------*/

static void prvSetupHardware(void)
{
    // Configure the timers for the sensors (1Hz, 20Hz, and 100Hz)
    prvConfigureTimers();
}
