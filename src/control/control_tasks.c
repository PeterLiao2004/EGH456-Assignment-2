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

static void prvControlTask(void *pvParameters);

void vCreateControlTasks(void)
{
    xTaskCreate(prvControlTask,
                "Control",
                configMINIMAL_STACK_SIZE,
                NULL,
                tskIDLE_PRIORITY + 2,
                NULL);
}

static void prvControlTask(void *pvParameters)
{
    uint32_t ui32ControlMessageCount = 0;

    (void)pvParameters;

    UARTprintf("Control task initialised\r\n");

    for (;;)
    {
        ui32ControlMessageCount++;

        UARTprintf("Control task running | count=%u | tick=%u\r\n",
                   ui32ControlMessageCount,
                   (uint32_t)xTaskGetTickCount());

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
