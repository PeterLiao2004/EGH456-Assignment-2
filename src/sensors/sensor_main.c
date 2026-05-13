
// /******************************************************************************
//  *
//  * This project provides a simple demonstration on how to control GPIO as both
//  * inputs and outputs and how to use a binary semaphore to defer ISR processing
//  * to an individual task to keep the ISR processing very light.  The four LEDs
//  * on the EK-TM4C1294XL are configured with the standard PinoutSet() function.
//  * The buttons are used to control which LED is on.  The default is LED D1.
//  * Pressing SW1 moves the lit LED down by one and pressing SW2 moves it up by
//  * one.  Both switches will wrap around.
//  *
//  * main() creates one binary semaphore and one task.  It then starts the
//  * scheduler.
//  *
//  * The binary semaphore is used to keep the LED task in a blocked state until
//  * an interrupt fires from a switch input.
//  *
//  * The LED task configures the buttons, initializes the LEDs, and starts the
//  * task which will handle the processing for the input switches to toggle the
//  * LEDs.
//  *
//  * This example uses UARTprintf for output of UART messages.  UARTprintf is not
//  * a thread-safe API and is only being used for simplicity of the demonstration
//  * and in a controlled manner.
//  *
//  * Open a terminal with 115,200 8-N-1 to see the output for this demo.
//  *
//  */

// /* Standard includes. */
// #include <stdio.h>
// #include <stdbool.h>
// #include <stdint.h>

// /* Kernel includes. */
// #include "FreeRTOS.h"
// #include "task.h"
// #include "semphr.h"

// /* Hardware includes. */
// #include "inc/hw_memmap.h"
// #include "inc/hw_sysctl.h"
// #include "driverlib/interrupt.h"
// #include "driverlib/rom.h"
// #include "driverlib/rom_map.h"
// #include "driverlib/sysctl.h"
// #include "drivers/rtos_hw_drivers.h"

// #include "driverlib/gpio.h"
// #include "driverlib/uart.h"
// #include "driverlib/pin_map.h"
// #include "utils/uartstdio.h"

// #include "inc/hw_ints.h"
// #include "driverlib/debug.h"
// #include "driverlib/fpu.h"
// #include "driverlib/timer.h"
// #include "utils/ustdlib.h"
// /*-----------------------------------------------------------*/

// /* The system clock frequency. */
// uint32_t g_ui32SysClock;

// /* Global for binary semaphore shared between tasks. */
// SemaphoreHandle_t xSW1Semaphore = NULL;

// SemaphoreHandle_t xSW2Semaphore = NULL;

// SemaphoreHandle_t xTimerSemaphore = NULL;

// SemaphoreHandle_t xUARTMutex = NULL;

// SemaphoreHandle_t xQueueDroppingMutex = NULL;

// /* Set up the clock and pin configurations to run this example. */
// static void prvSetupHardware(void);

// /* create the LED task. */
// extern void vCreateTasks(void);
// /*-----------------------------------------------------------*/

// int main(void)
// {
//     /* Prepare the hardware to run this example. */
//     prvSetupHardware();

//     /* Create the binary semaphore used to synchronize the button ISR and the
//      * button processing task. */
//     xSW1Semaphore = xSemaphoreCreateBinary();

//     xSW2Semaphore = xSemaphoreCreateBinary();

//     xTimerSemaphore = xSemaphoreCreateBinary();

//     xUARTMutex = xSemaphoreCreateMutex();

//     xQueueDroppingMutex = xSemaphoreCreateMutex();

//     if ((xSW1Semaphore != NULL) && (xSW2Semaphore != NULL) && (xTimerSemaphore != NULL) && (xUARTMutex != NULL) && (xQueueDroppingMutex != NULL))
//     {
//         /* Configure application specific hardware and initialize the task thread. */
//         vCreateTasks();

//         /* Start the tasks. */
//         vTaskStartScheduler();
//     }

//     /* If all is well, the scheduler will now be running, and the following
//     line will never be reached.  If the following line does execute, then
//     there was insufficient FreeRTOS heap memory available for the idle and/or
//     timer tasks to be created.  See the memory management section on the
//     FreeRTOS web site for more details. */
//     for (;;)
//         ;
// }

// /*-----------------------------------------------------------*/
// static void prvConfigureUART(void)
// {
//     /* Enable GPIO port A which is used for UART0 pins.
//      * TODO: change this to whichever GPIO port you are using. */
//     SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);

//     /* Configure the pin muxing for UART0 functions on port A0 and A1.
//      * This step is not necessary if your part does not support pin muxing.
//      * TODO: change this to select the port/pin you are using. */
//     GPIOPinConfigure(GPIO_PA0_U0RX);
//     GPIOPinConfigure(GPIO_PA1_U0TX);

//     /* Enable UART0 so that we can configure the clock. */
//     SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);

//     /* Use the internal 16MHz oscillator as the UART clock source. */
//     UARTClockSourceSet(UART0_BASE, UART_CLOCK_PIOSC);

//     /* Select the alternate (UART) function for these pins.
//      * TODO: change this to select the port/pin you are using. */
//     GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);

//     /* Initialize the UART for console I/O. */
//     UARTStdioConfig(0, 115200, 16000000);
// }
// /*-----------------------------------------------------------*/

// static void prvConfigureTimer(void) {
//     /* Timer configs */
//     SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
//     TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);
//     TimerLoadSet(TIMER0_BASE, TIMER_A, g_ui32SysClock / 5 -1); // 5Hz timer

//     TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
//     IntEnable(INT_TIMER0A);
//     TimerEnable(TIMER0_BASE, TIMER_A);
// }

// /*-----------------------------------------------------------*/

// static void prvSetupHardware(void)
// {
//     /* Run from the PLL at configCPU_CLOCK_HZ MHz. */
//     g_ui32SysClock = MAP_SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ |
//                                              SYSCTL_OSC_MAIN | SYSCTL_USE_PLL |
//                                              SYSCTL_CFG_VCO_240),
//                                             configCPU_CLOCK_HZ);

//     /* Configure device pins. */
//     PinoutSet(false, false);

//     /* Configure UART0 to send messages to terminal. */
//     prvConfigureUART();
//     prvConfigureTimer();
// }

// /*-----------------------------------------------------------*/

// void vApplicationMallocFailedHook(void)
// {
//     /* vApplicationMallocFailedHook() will only be called if
//     configUSE_MALLOC_FAILED_HOOK is set to 1 in FreeRTOSConfig.h.  It is a hook
//     function that will get called if a call to pvPortMalloc() fails.
//     pvPortMalloc() is called internally by the kernel whenever a task, queue,
//     timer or semaphore is created.  It is also called by various parts of the
//     demo application.  If heap_1.c or heap_2.c are used, then the size of the
//     heap available to pvPortMalloc() is defined by configTOTAL_HEAP_SIZE in
//     FreeRTOSConfig.h, and the xPortGetFreeHeapSize() API function can be used
//     to query the size of free heap space that remains (although it does not
//     provide information on how the remaining heap might be fragmented). */
//     IntMasterDisable();
//     for (;;)
//         ;
// }
// /*-----------------------------------------------------------*/

// void vApplicationIdleHook(void)
// {
//     /* vApplicationIdleHook() will only be called if configUSE_IDLE_HOOK is set
//     to 1 in FreeRTOSConfig.h.  It will be called on each iteration of the idle
//     task.  It is essential that code added to this hook function never attempts
//     to block in any way (for example, call xQueueReceive() with a block time
//     specified, or call vTaskDelay()).  If the application makes use of the
//     vTaskDelete() API function (as this demo application does) then it is also
//     important that vApplicationIdleHook() is permitted to return to its calling
//     function, because it is the responsibility of the idle task to clean up
//     memory allocated by the kernel to any task that has since been deleted. */
// }
// /*-----------------------------------------------------------*/

// void vApplicationTickHook(void)
// {
//     /* This function will be called by each tick interrupt if
//         configUSE_TICK_HOOK is set to 1 in FreeRTOSConfig.h.  User code can be
//         added here, but the tick hook is called from an interrupt context, so
//         code must not attempt to block, and only the interrupt safe FreeRTOS API
//         functions can be used (those that end in FromISR()). */

//     /* Only the full demo uses the tick hook so there is no code is
//         executed here. */
// }
// /*-----------------------------------------------------------*/

// void vApplicationStackOverflowHook(TaskHandle_t pxTask, char *pcTaskName)
// {
//     (void)pcTaskName;
//     (void)pxTask;

//     /* Run time stack overflow checking is performed if
//     configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
//     function is called if a stack overflow is detected. */
//     IntMasterDisable();
//     for (;;)
//         ;
// }
// /*-----------------------------------------------------------*/

// void *malloc(size_t xSize)
// {
//     /* There should not be a heap defined, so trap any attempts to call
//     malloc. */
//     IntMasterDisable();
//     for (;;)
//         ;
// }
// /*-----------------------------------------------------------*/
