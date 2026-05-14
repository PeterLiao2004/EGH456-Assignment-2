/*
 * Control subsystem task scaffold.
 *
 * This is the entry point for control-owned RTOS tasks. Keep high-level
 * control logic in the control module and let app_tasks.c only wire it in.
 */

#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"
#include "utils/uartstdio.h"

#include "control_tasks.h"


void vCreateControlTasks(void)
{
    xTaskCreate(prvStateManagerTask,
                "State Manager",
                configMINIMAL_STACK_SIZE,
                NULL,
                tskIDLE_PRIORITY + 2,
                NULL);
    xTaskCreate(prvSafetyMonitorTask,
                "Safety Monitor",
                configMINIMAL_STACK_SIZE,
                NULL,
                tskIDLE_PRIORITY + 2,
                NULL);
}

static void prvSafetyMonitorTask(void *pvParameters)
{
    uint32_t ui32ControlMessageCount = 0;

    (void)pvParameters;

    UARTprintf("Safety monitor task initialised\r\n");

    for (;;)
    {
        ui32ControlMessageCount++;

        UARTprintf("Safety monitor task running | count=%u | tick=%u\r\n",
                   ui32ControlMessageCount,
                   (uint32_t)xTaskGetTickCount());

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void prvStateManagerTask(void *pvParameters)
{
    uint32_t ui32ControlMessageCount = 0;

    (void)pvParameters;

    UARTprintf("State manager task initialised\r\n");

    for (;;)
    {
        ui32ControlMessageCount++;

        UARTprintf("State manager task running | count=%u | tick=%u\r\n",
                   ui32ControlMessageCount,
                   (uint32_t)xTaskGetTickCount());

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

