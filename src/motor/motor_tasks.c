#include "motor_tasks.h"

#include "FreeRTOS.h"
#include "task.h"

static void vMotorTask(void *pvParameters);

void vMotorTaskCreate(void)
{
    xTaskCreate(vMotorTask,
                "Motor",
                configMINIMAL_STACK_SIZE,
                NULL,
                tskIDLE_PRIORITY + 2,
                NULL);
}

static void vMotorTask(void *pvParameters)
{
    ( void ) pvParameters;

    for( ;; )
    {
        /* Motor subsystem logic will be added here. */
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
