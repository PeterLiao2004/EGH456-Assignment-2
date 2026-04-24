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

static void prvHeartbeatTask(void *pvParameters);
static void prvButtonTask(void *pvParameters);
static void prvConfigureLED(void);
static void prvConfigureHWTimer(void);
static void prvConfigureButton(void);

/*-----------------------------------------------------------*/

void vCreateAppTasks(void)
{
    /* Configure the peripherals used by the application tasks. */
    prvConfigureLED();
    prvConfigureButton();
    prvConfigureHWTimer();

    /* Create the application tasks. */
    vCreateMotorTasks();
    //vCreateControlTasks();
    //vCreateSensorTasks();
    //vCreateUiTasks();

    // xTaskCreate(prvHeartbeatTask,
    //             "Heartbeat",
    //             configMINIMAL_STACK_SIZE,
    //             NULL,
    //             tskIDLE_PRIORITY + 1,
    //             NULL);

    // xTaskCreate(prvButtonTask,
    //             "Button",
    //             configMINIMAL_STACK_SIZE,
    //             NULL,
    //             tskIDLE_PRIORITY + 1,
    //             NULL);
}

/*-----------------------------------------------------------*/

static void prvConfigureLED(void)
{
    /* Leave the board in a known state so the template has a simple visual cue. */
    LEDWrite(LED_D1, LED_D1);
}

/*-----------------------------------------------------------*/

static void prvConfigureButton(void)
{
    ButtonsInit();

    GPIOIntTypeSet(BUTTONS_GPIO_BASE, ALL_BUTTONS, GPIO_FALLING_EDGE);
    GPIOIntEnable(BUTTONS_GPIO_BASE, ALL_BUTTONS);

    IntEnable(INT_GPIOJ);
    IntMasterEnable();
}

/*-----------------------------------------------------------*/

static void prvConfigureHWTimer(void)
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);

    TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);

    /* Generate a heartbeat twice per second. */
    TimerLoadSet(TIMER0_BASE, TIMER_A, g_ui32SysClock / 2);

    TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
    IntEnable(INT_TIMER0A);
    IntMasterEnable();

    TimerEnable(TIMER0_BASE, TIMER_A);
}

/*-----------------------------------------------------------*/

void xTimerHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
    xSemaphoreGiveFromISR(xTimerSemaphore, &xHigherPriorityTaskWoken);

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/*-----------------------------------------------------------*/

void xButtonsHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint32_t ui32Status;
    TickType_t xNow;

    ui32Status = GPIOIntStatus(BUTTONS_GPIO_BASE, true);
    GPIOIntClear(BUTTONS_GPIO_BASE, ui32Status);

    xNow = xTaskGetTickCountFromISR();

    /* Basic debounce to keep the template output readable. */
    if ((xNow - g_ui32TimeStamp) > pdMS_TO_TICKS(50))
    {
        if ((ui32Status & (USR_SW1 | USR_SW2)) != 0U)
        {
            g_ui32ButtonPressCount++;
            xSemaphoreGiveFromISR(xButtonSemaphore, &xHigherPriorityTaskWoken);
        }

        g_ui32TimeStamp = xNow;
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/*-----------------------------------------------------------*/

static void prvHeartbeatTask(void *pvParameters)
{
    uint32_t ui32HeartbeatCount = 0;

    (void)pvParameters;

    UARTprintf("\r\n=== Barebones app template starting ===\r\n");

    for (;;)
    {
        if (xSemaphoreTake(xTimerSemaphore, portMAX_DELAY) == pdPASS)
        {
            ui32HeartbeatCount++;

            UARTprintf("Heartbeat %u | tick=%u | button_presses=%u\r\n",
                       ui32HeartbeatCount,
                       (uint32_t)xTaskGetTickCount(),
                       g_ui32ButtonPressCount);
        }
    }
}

/*-----------------------------------------------------------*/

static void prvButtonTask(void *pvParameters)
{
    (void)pvParameters;

    for (;;)
    {
        if (xSemaphoreTake(xButtonSemaphore, portMAX_DELAY) == pdPASS)
        {
            UARTprintf("Button interrupt received | total_presses=%u\r\n",
                       g_ui32ButtonPressCount);
        }
    }
}

/*-----------------------------------------------------------*/
