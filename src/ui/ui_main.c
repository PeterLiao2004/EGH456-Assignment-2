
/******************************************************************************
 *
 * Legacy UI-only entry point. The application entry point is src/main.c;
 * switch handling is owned by the hardware module.
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

#include "grlib.h"
#include "drivers/Kentec320x240x16_ssd2119_spi.h"
#include "drivers/touch.h"
#include "widget.h"
#include "canvas.h"
#include "pushbutton.h"
/*-----------------------------------------------------------*/

/* The system clock frequency. */
extern volatile uint32_t g_ui32SysClock;

extern SemaphoreHandle_t xUARTMutex;

extern SemaphoreHandle_t xQueueDroppingMutex;

/* Set up the clock and pin configurations to run this example. */
static void prvSetupHardware(void);

/* create the LED task. */
extern void vCreateUiTasks(void);
/*-----------------------------------------------------------*/

int ui_main(void)
{
    /* Prepare the hardware to run this example. */
    prvSetupHardware();

    xUARTMutex = xSemaphoreCreateMutex();

    xQueueDroppingMutex = xSemaphoreCreateMutex();

    if ((xUARTMutex != NULL) && (xQueueDroppingMutex != NULL))
    {
        /* Configure application specific hardware and initialize the task thread. */
        vCreateUiTasks();

        /* Start the tasks. */
        vTaskStartScheduler();
    }

    /* If all is well, the scheduler will now be running, and the following
    line will never be reached.  If the following line does execute, then
    there was insufficient FreeRTOS heap memory available for the idle and/or
    timer tasks to be created.  See the memory management section on the
    FreeRTOS web site for more details. */
    for (;;)
        ;
}

/*-----------------------------------------------------------*/
static void prvConfigureUART(void)
{
    /* Enable GPIO port A which is used for UART0 pins.
     * TODO: change this to whichever GPIO port you are using. */
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);

    /* Configure the pin muxing for UART0 functions on port A0 and A1.
     * This step is not necessary if your part does not support pin muxing.
     * TODO: change this to select the port/pin you are using. */
    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);

    /* Enable UART0 so that we can configure the clock. */
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);

    /* Use the internal 16MHz oscillator as the UART clock source. */
    UARTClockSourceSet(UART0_BASE, UART_CLOCK_PIOSC);

    /* Select the alternate (UART) function for these pins.
     * TODO: change this to select the port/pin you are using. */
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);

    /* Initialize the UART for console I/O. */
    UARTStdioConfig(0, 115200, 16000000);
}

/*-----------------------------------------------------------*/

static void prvSetupHardware(void)
{
    prvConfigureUART();
}

/*-----------------------------------------------------------*/
