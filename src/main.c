/*
 * Barebones FreeRTOS application entry point.
 *
 * This file sets up the hardware, creates the shared synchronization objects,
 * and starts the minimal UART-based application tasks defined in app_tasks.c.
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
#include "inc/hw_ints.h"
#include "driverlib/interrupt.h"
#include "driverlib/rom.h"
#include "driverlib/rom_map.h"
#include "driverlib/sysctl.h"
#include "drivers/rtos_hw_drivers.h"
#include "driverlib/gpio.h"
#include "driverlib/uart.h"
#include "driverlib/pin_map.h"
#include "utils/uartstdio.h"
/*-----------------------------------------------------------*/

/* The system clock frequency. */
volatile uint32_t g_ui32SysClock;

/* Global semaphores shared between ISRs and tasks. */
SemaphoreHandle_t xTimerSemaphore = NULL;
SemaphoreHandle_t xUARTMutex = NULL;
SemaphoreHandle_t xQueueDroppingMutex = NULL;

/* Set up the clock and pin configurations to run this example. */
static void prvSetupHardware( void );

/* Function to create the application tasks. */
extern void vCreateAppTasks( void );

/* Set up sensor configurations. */
extern void sensor_main(void);
/*-----------------------------------------------------------*/

int main( void )
{
    IntMasterEnable();
    /* Prepare the hardware to run this example. */
    prvSetupHardware();

    sensor_main();

    /* Create shared synchronization objects. */
    xTimerSemaphore = xSemaphoreCreateBinary();
    xUARTMutex = xSemaphoreCreateMutex();
    xQueueDroppingMutex = xSemaphoreCreateMutex();

    if ( (xTimerSemaphore != NULL) &&
         (xUARTMutex != NULL) &&
         (xQueueDroppingMutex != NULL) )
    {
        /* Configure application specific hardware and initialize the tasks. */
        vCreateAppTasks();
        /* Start the tasks. */
        vTaskStartScheduler();
    }

    /* If all is well, the scheduler will now be running, and the following
    line will never be reached.  If the following line does execute, then
    there was insufficient FreeRTOS heap memory available for the idle and/or
    timer tasks to be created.  See the memory management section on the
    FreeRTOS web site for more details. */
    for( ;; );
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
//  Hall sensor inputs from BoosterPack 1:
//  Hall A -> PM3, Hall B -> PH2, Hall C -> PN2
#define HALL_A_PORT GPIO_PORTM_BASE
#define HALL_A_PIN  GPIO_PIN_3
#define HALL_B_PORT GPIO_PORTH_BASE
#define HALL_B_PIN  GPIO_PIN_2
#define HALL_C_PORT GPIO_PORTN_BASE
#define HALL_C_PIN  GPIO_PIN_2

static void prvConfigureHallSensors( void )
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOM);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOH);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPION);

    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOM))
    {
    }
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOH))
    {
    }
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPION))
    {
    }

    GPIOPinTypeGPIOInput(HALL_A_PORT, HALL_A_PIN);
    GPIOPinTypeGPIOInput(HALL_B_PORT, HALL_B_PIN);
    GPIOPinTypeGPIOInput(HALL_C_PORT, HALL_C_PIN);

    GPIOPadConfigSet(HALL_A_PORT, HALL_A_PIN, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
    GPIOPadConfigSet(HALL_B_PORT, HALL_B_PIN, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
    GPIOPadConfigSet(HALL_C_PORT, HALL_C_PIN, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);

    GPIOIntDisable(HALL_A_PORT, HALL_A_PIN);
    GPIOIntDisable(HALL_B_PORT, HALL_B_PIN);
    GPIOIntDisable(HALL_C_PORT, HALL_C_PIN);

    GPIOIntTypeSet(HALL_A_PORT, HALL_A_PIN, GPIO_BOTH_EDGES);
    GPIOIntTypeSet(HALL_B_PORT, HALL_B_PIN, GPIO_BOTH_EDGES);
    GPIOIntTypeSet(HALL_C_PORT, HALL_C_PIN, GPIO_BOTH_EDGES);

    GPIOIntClear(HALL_A_PORT, HALL_A_PIN);
    GPIOIntClear(HALL_B_PORT, HALL_B_PIN);
    GPIOIntClear(HALL_C_PORT, HALL_C_PIN);

    GPIOIntEnable(HALL_A_PORT, HALL_A_PIN);
    GPIOIntEnable(HALL_B_PORT, HALL_B_PIN);
    GPIOIntEnable(HALL_C_PORT, HALL_C_PIN);

    IntEnable(INT_GPIOM);
    IntEnable(INT_GPIOH);
    IntEnable(INT_GPION);
}
/*-----------------------------------------------------------*/

static void prvSetupHardware( void )
{
    /* Run from the PLL at configCPU_CLOCK_HZ MHz. */
    g_ui32SysClock = MAP_SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ |
            SYSCTL_OSC_MAIN | SYSCTL_USE_PLL |
            SYSCTL_CFG_VCO_240), configCPU_CLOCK_HZ);

    /* Configure device pins. */
    // This sets most pins to their default use
    // When using for a different project ensure to configure
    // project specific pins/devices after this function
    PinoutSet(false, false);
    prvConfigureUART();
    prvConfigureHallSensors();
}
/*-----------------------------------------------------------*/

void vApplicationMallocFailedHook( void )
{
    /* vApplicationMallocFailedHook() will only be called if
    configUSE_MALLOC_FAILED_HOOK is set to 1 in FreeRTOSConfig.h.  It is a hook
    function that will get called if a call to pvPortMalloc() fails.
    pvPortMalloc() is called internally by the kernel whenever a task, queue,
    timer or semaphore is created.  It is also called by various parts of the
    demo application.  If heap_1.c or heap_2.c are used, then the size of the
    heap available to pvPortMalloc() is defined by configTOTAL_HEAP_SIZE in
    FreeRTOSConfig.h, and the xPortGetFreeHeapSize() API function can be used
    to query the size of free heap space that remains (although it does not
    provide information on how the remaining heap might be fragmented). */
    IntMasterDisable();
    for( ;; );
}
/*-----------------------------------------------------------*/

void vApplicationIdleHook( void )
{
    /* vApplicationIdleHook() will only be called if configUSE_IDLE_HOOK is set
    to 1 in FreeRTOSConfig.h.  It will be called on each iteration of the idle
    task.  It is essential that code added to this hook function never attempts
    to block in any way (for example, call xQueueReceive() with a block time
    specified, or call vTaskDelay()).  If the application makes use of the
    vTaskDelete() API function (as this demo application does) then it is also
    important that vApplicationIdleHook() is permitted to return to its calling
    function, because it is the responsibility of the idle task to clean up
    memory allocated by the kernel to any task that has since been deleted. */
}
/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName )
{
    ( void ) pcTaskName;
    ( void ) pxTask;

    /* Run time stack overflow checking is performed if
    configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
    function is called if a stack overflow is detected. */
    IntMasterDisable();
    for( ;; );
}
/*-----------------------------------------------------------*/

void *malloc( size_t xSize )
{
    /* There should not be a heap defined, so trap any attempts to call
    malloc. */
    IntMasterDisable();
    for( ;; );
}
/*-----------------------------------------------------------*/

void vApplicationTickHook( void )
{
    /* This function will be called by each tick interrupt if
        configUSE_TICK_HOOK is set to 1 in FreeRTOSConfig.h.  User code can be
        added here, but the tick hook is called from an interrupt context, so
        code must not attempt to block, and only the interrupt safe FreeRTOS API
        functions can be used (those that end in FromISR()). */

    /* Only the full demo uses the tick hook so there is no code is
        executed here. */
}
/*-----------------------------------------------------------*/
