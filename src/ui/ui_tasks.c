#include "ui_tasks.h"

#include "FreeRTOS.h"
#include "task.h"

static void vUiTask(void *pvParameters);

void vUiTaskCreate(void)
{
    xTaskCreate(vUiTask,
                "UI",
                configMINIMAL_STACK_SIZE,
                NULL,
                tskIDLE_PRIORITY + 1,
                NULL);
}

static void vUiTask(void *pvParameters)
{
    ( void ) pvParameters;

    for( ;; )
    {
        /* UI subsystem logic will be added here. */
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
