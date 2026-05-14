/*
 * Sensor subsystem task scaffold.
 *
 * This is the entry point for sensor-owned RTOS tasks. Keep sensor-specific
 * logic in the sensors module and let app_tasks.c only wire it in.
 */


/* Standard includes. */
#include <stdio.h>
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
#include "drivers/rtos_hw_drivers.h"
#include "driverlib/timer.h"

/* Sensor includes. */
#include "drivers/i2cOptDriver.h"
#include "drivers/opt3001.h"

#include "driverlib/uart.h"
#include "driverlib/pin_map.h"
#include "utils/uartstdio.h"

#include "driverlib/debug.h"
#include "driverlib/rom.h"
#include "driverlib/i2c.h"
#include "utils/ustdlib.h"

#include "queue.h"

#include "sensor_tasks.h"

/*----------------------------------------------------------- */
// Define queue lengths for each sensor task
#define opt3001QUEUE_LENGTH (5U) // 2Hz
// #define tempHumQUEUE_LENGTH (3U) // 1Hz
// #define accelQUEUE_LENGTH (30U) // 100Hz
// #define distQUEUE_LENGTH (10U) // 20Hz

/*----------------------------------------------------------- */
// Sysclock from main.c
extern volatile uint32_t g_ui32SysClock;

// Drivers for sensors
extern void i2cOptDriverInit(void);

// Semaphores for triggering tasks
extern SemaphoreHandle_t xFastTimerSemaphore;
extern SemaphoreHandle_t xSlowTimerSemaphore;

extern SemaphoreHandle_t xOpt3001ReadSemaphore;

extern SemaphoreHandle_t xOpt3001QueueDropSemaphore;

// Queues to hold data between tasks
static QueueHandle_t xOpt3001Queue = NULL;

// Called by app_tasks.c to create the sensor tasks
void vCreateSensorTasks(void);

// Tasks for sensors
static void prvFastSchedulerTask(void *pvParameters);
static void prvSlowSchedulerTask(void *pvParameters);
static void prvOpt3001Task(void *pvParameters);

// Structs for passing data between tasks
struct opt3001Data
{
    uint32_t sequenceNum;
    TickType_t timestamp;
    uint16_t unfilteredLux;
    uint16_t filteredLux;
};

/*----------------------------------------------------------- */

void vCreateSensorTasks(void)
{
    xOpt3001Queue = xQueueCreate(opt3001QUEUE_LENGTH, sizeof(struct opt3001Data));
    if (xOpt3001Queue == NULL) {
        // Queue creation failed - handle error
        // NEED TO PROTECT THE UART LATER!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        UARTprintf("Opt3001 Queue creation failed\n");
        return;
    }
    xTaskCreate(prvFastSchedulerTask,
                "FastScheduler",
                configMINIMAL_STACK_SIZE,
                NULL,
                tskIDLE_PRIORITY + 2,
                NULL);

    xTaskCreate(prvSlowSchedulerTask,
                "SlowScheduler",
                configMINIMAL_STACK_SIZE,
                NULL,
                tskIDLE_PRIORITY + 2,
                NULL);

    xTaskCreate(prvOpt3001Task,
                "Opt3001",
                configMINIMAL_STACK_SIZE,
                NULL,
                tskIDLE_PRIORITY + 1,
                NULL);
}

/*----------------------------------------------------------- */
static void prvFastSchedulerTask(void *pvParameters)
{
    uint16_t tick = 0;

    for (;;)
    {
        if (xSemaphoreTake(xFastTimerSemaphore, portMAX_DELAY) == pdPASS)
        {
            tick++;

            // if (tick % 2 == 0)
            // {
            //     // Trigger 150Hz tasks
            //     // Power task?
            // }

            // if (tick % 3 == 0) {
            //     // Trigger 100Hz tasks
            //     // Accelerometer task
            // }

            // if (tick % 15 == 0) {
            //     // Trigger 20Hz tasks
            //     // Distance sensor task
            // }

            if (tick >= 300) {
                // Reset tick count
                tick = 0;
            }
        }
    }
}

/*----------------------------------------------------------- */

static void prvSlowSchedulerTask(void *pvParameters)
{
    uint16_t tick = 0;

    for (;;)
    {
        if (xSemaphoreTake(xSlowTimerSemaphore, portMAX_DELAY) == pdPASS)
        {
            tick++;

            // Trigger 2Hz tasks
            xSemaphoreGive(xOpt3001ReadSemaphore);

            if (tick % 2 == 0)
            {
                // Trigger 1Hz tasks
                // Temperature and humidity task
            }

            if (tick >= 2) {
                // Reset tick count
                tick = 0;
            }
        }
    }
}

/*----------------------------------------------------------- */

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
