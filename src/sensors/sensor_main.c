
/******************************************************************************
 *
 * This project provides a simple demonstration on how to control GPIO as both
 * inputs and outputs and how to use a binary semaphore to defer ISR processing
 * to an individual task to keep the ISR processing very light.  The four LEDs
 * on the EK-TM4C1294XL are configured with the standard PinoutSet() function.
 * The buttons are used to control which LED is on.  The default is LED D1.
 * Pressing SW1 moves the lit LED down by one and pressing SW2 moves it up by
 * one.  Both switches will wrap around.
 *
 * main() creates one binary semaphore and one task.  It then starts the
 * scheduler.
 *
 * The binary semaphore is used to keep the LED task in a blocked state until
 * an interrupt fires from a switch input.
 *
 * The LED task configures the buttons, initializes the LEDs, and starts the
 * task which will handle the processing for the input switches to toggle the
 * LEDs.
 *
 * This example uses UARTprintf for output of UART messages.  UARTprintf is not
 * a thread-safe API and is only being used for simplicity of the demonstration
 * and in a controlled manner.
 *
 * Open a terminal with 115,200 8-N-1 to see the output for this demo.
 *
 */

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
SemaphoreHandle_t xSW1Semaphore = NULL;

SemaphoreHandle_t xSW2Semaphore = NULL;

SemaphoreHandle_t xTimerSemaphore = NULL;

SemaphoreHandle_t xUARTMutex = NULL;

SemaphoreHandle_t xQueueDroppingMutex = NULL;

/* Set up the clock and pin configurations to run this example. */
static void prvSetupHardware(void);

/* create the LED task. */
extern void vCreateTasks(void);

extern volatile uint32_t g_ui32SysClock;
/*-----------------------------------------------------------*/

int sensor_main(void)
{
    /* Prepare the hardware to run this example. */
    prvSetupHardware();

    /* Create the binary semaphore used to synchronize the button ISR and the
     * button processing task. */
    // xSW1Semaphore = xSemaphoreCreateBinary();

    // xSW2Semaphore = xSemaphoreCreateBinary();

    // xTimerSemaphore = xSemaphoreCreateBinary();

    // xUARTMutex = xSemaphoreCreateMutex();

    // xQueueDroppingMutex = xSemaphoreCreateMutex();

    // if ((xSW1Semaphore != NULL) && (xSW2Semaphore != NULL) && (xTimerSemaphore != NULL) && (xUARTMutex != NULL) && (xQueueDroppingMutex != NULL))
    // {
    //     /* Configure application specific hardware and initialize the task thread. */
    //     vCreateTasks();

    //     /* Start the tasks. */
    //     vTaskStartScheduler();
    // }

    /* If all is well, the scheduler will now be running, and the following
    line will never be reached.  If the following line does execute, then
    there was insufficient FreeRTOS heap memory available for the idle and/or
    timer tasks to be created.  See the memory management section on the
    FreeRTOS web site for more details. */
    for (;;)
        ;
}

/*-----------------------------------------------------------*/

static void prvConfigureTimers(void) {
    /* Timer 0A configs - fast 200Hz timer*/
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
    TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);
    TimerLoadSet(TIMER0_BASE, TIMER_A, g_ui32SysClock / 200 -1); // 200Hz timer

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
