/*
 * UI subsystem task scaffold.
 *
 * This is the entry point for UI-owned RTOS tasks. Keep UI logic in the UI
 * module and let app_tasks.c only wire it in.
 */

#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"
#include "utils/uartstdio.h"

#include "ui_tasks.h"

static void prvUiTask(void *pvParameters);

void vCreateUiTasks(void)
{
    xTaskCreate(prvUiTask,
                "UI",
                configMINIMAL_STACK_SIZE,
                NULL,
                tskIDLE_PRIORITY + 2,
                NULL);
}

static void prvUiTask(void *pvParameters)
{
    uint32_t ui32UiMessageCount = 0;

    (void)pvParameters;

    UARTprintf("UI task initialised\r\n");

    for (;;)
    {
        ui32UiMessageCount++;

        UARTprintf("UI task running | count=%u | tick=%u\r\n",
                   ui32UiMessageCount,
                   (uint32_t)xTaskGetTickCount());

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
