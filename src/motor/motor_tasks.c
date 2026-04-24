/*
 * Motor subsystem task scaffold.
 *
 * This is the entry point for motor-owned RTOS tasks. Keep low-level motor
 * behavior in the motor module and let app_tasks.c only wire the module in.
 */
/* Standard includes. */
#include <stdint.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "utils/uartstdio.h"

/* Application includes. */
#include "motor_tasks.h"

/*-----------------------------------------------------------*/
/* Motor control API. Implementations will live in the motor module. */
void Motor_Init(void);
void Motor_Enable(void);
void Motor_Disable(void);

void Motor_Kickstart(void);
void Motor_SetDuty(float duty);

float Motor_GetSpeedRPM(void);
float Motor_GetDuty(void);
bool Motor_HasValidHallFeedback(void);
bool Motor_IsStopped(void);
/*-----------------------------------------------------------*/



/*-----------------------------------------------------------*/
/* Motor control task. */
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

/*-----------------------------------------------------------*/
/* Motor control task implementation. */
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
