#include "sensor_tasks.h"

#include "FreeRTOS.h"
#include "task.h"

static void vSensorTask(void *pvParameters);

void vSensorTaskCreate(void)
{
    xTaskCreate(vSensorTask,
                "Sensor",
                configMINIMAL_STACK_SIZE,
                NULL,
                tskIDLE_PRIORITY + 1,
                NULL);
}

static void vSensorTask(void *pvParameters)
{
    ( void ) pvParameters;

    for( ;; )
    {
        /* Sensor subsystem logic will be added here. */
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
