/*
 * Motor subsystem task scaffold.
 *
 * This is the entry point for motor-owned RTOS tasks. Keep low-level motor
 * behavior in the motor module and let app_tasks.c only wire the module in.
 */

#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"
#include "utils/uartstdio.h"

#include "motor_tasks.h"

static void prvMotorTask(void *pvParameters);

void vCreateMotorTasks(void)
{
    xTaskCreate(prvMotorTask,
                "Motor",
                configMINIMAL_STACK_SIZE,
                NULL,
                tskIDLE_PRIORITY + 2,
                NULL);
}

static void prvMotorTask(void *pvParameters)
{
    uint32_t ui32MotorMessageCount = 0;

    (void)pvParameters;

    UARTprintf("Motor task initialised\r\n");

    for (;;)
    {
        ui32MotorMessageCount++;

        UARTprintf("Motor task running | count=%u | tick=%u\r\n",
                   ui32MotorMessageCount,
                   (uint32_t)xTaskGetTickCount());

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
