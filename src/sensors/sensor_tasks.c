/*
 * Sensor subsystem task scaffold.
 *
 * This is the entry point for sensor-owned RTOS tasks. Keep sensor-specific
 * logic in the sensors module and let app_tasks.c only wire it in.
 */

#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"
#include "utils/uartstdio.h"

#include "sensor_tasks.h"

static void prvOpt3001Task(void *pvParameters);

void vCreateSensorTasks(void)
{
    xTaskCreate(prvOpt3001Task,
                "Opt3001",
                configMINIMAL_STACK_SIZE,
                NULL,
                tskIDLE_PRIORITY + 2,
                NULL);
}

static void prvOpt3001Task(void *pvParameters)
{
    uint32_t ui32SensorMessageCount = 0;

    (void)pvParameters;

    UARTprintf("Opt3001 task initialised\r\n");

    for (;;)
    {
        ui32SensorMessageCount++;

        UARTprintf("Opt3001 task running | count=%u | tick=%u\r\n",
                   ui32SensorMessageCount,
                   (uint32_t)xTaskGetTickCount());

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
