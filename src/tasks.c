/*
 * Application task registration.
 *
 * Keep task creation centralized here so each subsystem can expose a simple
 * create/init function and ownership remains clear across the team.
 */

/* Standard includes. */
#include <stdint.h>
#include <stdbool.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"

/*-----------------------------------------------------------*/
/* The system clock frequency. */
extern volatile uint32_t g_ui32SysClock;

static void prvSystemTask(void *pvParameters);

/* Called by main() to create all application tasks. */
void vCreateTasks(void);
/*-----------------------------------------------------------*/

void vCreateTasks(void)
{
    /* Replace this placeholder with module create calls, for example:
     * vMotorTaskCreate();
     * vControlTaskCreate();
     * vSensorTaskCreate();
     * vUiTaskCreate();
     */
    xTaskCreate(prvSystemTask,
                "System",
                configMINIMAL_STACK_SIZE,
                NULL,
                tskIDLE_PRIORITY,
                NULL);
}


static void prvSystemTask(void *pvParameters)
{
    ( void ) pvParameters;

    for( ;; )
    {
        /* Placeholder task to keep the scheduler structure valid until
         * subsystem-specific tasks are added. */
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
