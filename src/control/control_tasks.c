#include "control_tasks.h"

#include "FreeRTOS.h"
#include "task.h"

static void vControlTask(void *pvParameters);

void vControlTaskCreate(void)
{
    xTaskCreate(vControlTask,
                "Control",
                configMINIMAL_STACK_SIZE,
                NULL,
                tskIDLE_PRIORITY + 2,
                NULL);
}

static void vControlTask(void *pvParameters)
{
    ( void ) pvParameters;

    for( ;; )
    {
        /* Control subsystem logic will be added here. */
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
