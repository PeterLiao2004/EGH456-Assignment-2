/*
 * Control subsystem task scaffold.
 *
 * This is the entry point for control-owned RTOS tasks. Keep high-level
 * control logic in the control module and let app_tasks.c only wire it in.
 */

#include <stdint.h>
#include <stdbool.h>


#include "FreeRTOS.h"
#include "task.h"
#include "utils/uartstdio.h"
#include "semphr.h"
#include "event_groups.h"
#include "state_manager.h"
#include "system_state.h"
#include "system_events.h"
#include "motor/motor_tasks.h"

/** @brief State manager task stack size. */
#define STATE_MANAGER_STACK_SIZE    (configMINIMAL_STACK_SIZE * 2)

/** @brief State manager task priority (above motor task at +2). */
#define STATE_MANAGER_PRIORITY      (tskIDLE_PRIORITY + 3)

//Private state variables
static SemaphoreHandle_t s_stateMutex;
static EventGroupHandle_t s_events;
static SystemState_t s_state;
static uint32_t s_desiredSpeed;


static void prvStateManagerTask(void *pvParameters);

void vCreateStateManagerTasks(void)
{
    StateManager_Init();
    xTaskCreate(prvStateManagerTask,
                "State Manager",
                STATE_MANAGER_STACK_SIZE,
                NULL,
                STATE_MANAGER_PRIORITY,
                NULL);
}


static void prvStateManagerTask(void *pvParameters)
{

    (void)pvParameters;

    for (;;)
    {
        EventBits_t fired = xEventGroupWaitBits(
            s_events,                                      // 1
            EVENT_START | EVENT_E_STOP | EVENT_FAULT_ACK,  // 2
            pdTRUE,                                        // 3
            pdFALSE,                                       // 4
            pdMS_TO_TICKS(20)                              // 5
        );

        if ((fired & EVENT_START) != 0U)
        {
            s_state = SYSTEM_STATE_STARTING;
            UARTprintf("EVENT_START received\r\n");
            Motor_Start(400U);
        }

        switch (s_state)
        {
            case SYSTEM_STATE_STOPPED:
                UARTprintf("State Manager in STOPPED state\r\n");
                if (fired & EVENT_START)
                {
                    s_state = SYSTEM_STATE_STARTING;
                    UARTprintf("EVENT_START received\r\n");
                }
                // Wait for a start command from the UI or serial input
                // (not implemented yet)
                break;
            case SYSTEM_STATE_STARTING:
                UARTprintf("State Manager in STARTING state\r\n");
                // Ramp up the motor and wait for valid hall feedback
                break;
            case SYSTEM_STATE_RUNNING:
                UARTprintf("State Manager in RUNNING state\r\n");
                // Monitor for stop or e-stop commands
                // Normal operation. Monitor for stop or e-stop commands.
                break;
            case SYSTEM_STATE_ESTOP_BRAKING:
                UARTprintf("State Manager in E-STOP BRAKING state\r\n");
                // Wait for the motor to come to rest, then transition to FAULT_LATCHED
                break;
            case SYSTEM_STATE_FAULT_LATCHED:
                UARTprintf("State Manager in FAULT LATCHED state\r\n");
                // Wait for a reset command from the UI or serial input
                break;
            default:
                // Invalid state - should never happen
                break;
        }
        // vTaskDelay(pdMS_TO_TICKS(1000));
    }
}


void StateManager_Init(void)
{
    s_stateMutex = xSemaphoreCreateMutex();
    s_events     = xEventGroupCreate();
    s_state      = SYSTEM_STATE_STOPPED;
    s_desiredSpeed = 0U;
    UARTprintf("State Manager initialised\r\n");
}

void StateManager_TriggerStart(void)
{
    if (s_events != NULL)
    {
        xEventGroupSetBits(s_events, EVENT_START);
    }
}

void StateManager_TriggerEStop(void)
{
    if (s_events != NULL)
    {
        xEventGroupSetBits(s_events, EVENT_E_STOP);
    }
}
