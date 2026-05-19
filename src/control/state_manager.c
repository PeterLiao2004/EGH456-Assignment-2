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
#include "timers.h"
#include "utils/uartstdio.h"
#include "semphr.h"
#include "event_groups.h"
#include "state_manager.h"
#include "system_state.h"
#include "system_events.h"
#include "motor/motor_tasks.h"
#include "debug/debug_log.h"
#include "drivers/rtos_hw_drivers.h"
/** @brief State manager task stack size. */
#define STATE_MANAGER_STACK_SIZE    (configMINIMAL_STACK_SIZE * 2)

/** @brief State manager task priority (above motor task at +2). */
#define STATE_MANAGER_PRIORITY      (tskIDLE_PRIORITY + 3)

#define SAFE_START_RPM             400U
#define HALL_VALID_EDGE_COUNT      12U

//Private state variables
static SemaphoreHandle_t s_stateMutex;
static EventGroupHandle_t s_events;
static SystemState_t s_state;
static uint32_t s_desiredSpeed;
static TimerHandle_t s_estopBlinkTimer = NULL;


static void prvStateManagerTask(void *pvParameters);
static void prvSetState(SystemState_t newState);
static const char *prvStateToString(SystemState_t state);
static void prvEStopBlinkCallback(TimerHandle_t xTimer);

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

/*-----------------------------------------------------------------------------
 * E-Stop LED blink timer callback
 *---------------------------------------------------------------------------*/

/**
 * @brief Toggles LED D1 every 250ms when E-stop or fault is active.
 *
 * This runs from the FreeRTOS timer daemon — no extra task stack needed.
 */
static void prvEStopBlinkCallback(TimerHandle_t xTimer)
{
    (void)xTimer;
    static bool on = false;
    on = !on;

    uint32_t allLEDs = LED_D1 | LED_D2 | LED_D3 | LED_D4;
    LEDWrite(allLEDs, on ? allLEDs : 0);
}

static void prvStateManagerTask(void *pvParameters)
{

    (void)pvParameters;

    for (;;)
    {
        EventBits_t fired = xEventGroupWaitBits(
            s_events,                                      
            EVENT_START | EVENT_E_STOP | EVENT_FAULT_ACK,  
            pdTRUE,                                        
            pdFALSE,                                       
            pdMS_TO_TICKS(20)                              
        );

        if (((fired & EVENT_E_STOP) != 0U) &&
            (s_state != SYSTEM_STATE_ESTOP_BRAKING) &&
            (s_state != SYSTEM_STATE_FAULT_LATCHED))
        {
            prvSetState(SYSTEM_STATE_ESTOP_BRAKING);
            DebugPrintf(DBG_STATE "EVENT_E_STOP received\r\n");
            Motor_EStop();
        }

        switch (s_state)
        {
            case SYSTEM_STATE_STOPPED:
                if ((fired & EVENT_START) != 0U)
                {
                    prvSetState(SYSTEM_STATE_STARTING);
                    DebugPrintf(DBG_STATE "EVENT_START received\r\n");
                    Motor_Start(SAFE_START_RPM);
                }
                break;

            case SYSTEM_STATE_STARTING:
                if (Motor_GetHallEdgeCount() >= HALL_VALID_EDGE_COUNT)
                {
                    prvSetState(SYSTEM_STATE_RUNNING);
                    DebugPrintf(DBG_STATE "Hall feedback valid, motor RUNNING\r\n");
                }
                break;

            case SYSTEM_STATE_RUNNING:
                break;

            case SYSTEM_STATE_ESTOP_BRAKING:
                if (Motor_HasReachedZero())
                {
                    Motor_Disable();
                    prvSetState(SYSTEM_STATE_FAULT_LATCHED);
                    DebugPrintf(DBG_STATE "Motor stopped, FAULT LATCHED\r\n");
                }
                break;

            case SYSTEM_STATE_FAULT_LATCHED:
                if ((fired & EVENT_FAULT_ACK) != 0U)
                {
                    prvSetState(SYSTEM_STATE_STOPPED);
                    DebugPrintf(DBG_STATE "FAULT_ACK received, STOPPED\r\n");
                }
                break;

            default:
                // Invalid state - should never happen
                break;
        }
        // vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void prvSetState(SystemState_t newState)
{
    if (s_state != newState)
    {
        s_state = newState;
        DebugPrintf(DBG_STATE "State Manager: %s\r\n", prvStateToString(newState));

        /* Update LEDs to reflect the new state.
         * All LEDs off by default.
         * E-stop / fault: all LEDs blink. */
        switch (newState)
        {
            case SYSTEM_STATE_STOPPED:
            case SYSTEM_STATE_STARTING:
            case SYSTEM_STATE_RUNNING:
                xTimerStop(s_estopBlinkTimer, 0);
                LEDWrite(LED_D1 | LED_D2 | LED_D3 | LED_D4, 0);  /* all off */
                break;

            case SYSTEM_STATE_ESTOP_BRAKING:
            case SYSTEM_STATE_FAULT_LATCHED:
                xTimerStart(s_estopBlinkTimer, 0);                /* all blink */
                break;

            default:
                xTimerStop(s_estopBlinkTimer, 0);
                LEDWrite(LED_D1 | LED_D2 | LED_D3 | LED_D4, 0);
                break;
        }
    }
}

static const char *prvStateToString(SystemState_t state)
{
    switch (state)
    {
        case SYSTEM_STATE_STOPPED:
            return "STOPPED";
        case SYSTEM_STATE_STARTING:
            return "STARTING";
        case SYSTEM_STATE_RUNNING:
            return "RUNNING";
        case SYSTEM_STATE_ESTOP_BRAKING:
            return "E-STOP BRAKING";
        case SYSTEM_STATE_FAULT_LATCHED:
            return "FAULT LATCHED";
        default:
            return "UNKNOWN";
    }
}

void StateManager_Init(void)
{
    s_stateMutex = xSemaphoreCreateMutex();
    s_events     = xEventGroupCreate();
    s_state      = SYSTEM_STATE_STOPPED;
    s_desiredSpeed = 0U;

    s_estopBlinkTimer = xTimerCreate(
        "EStopBlink",
        pdMS_TO_TICKS(250),        /* 250ms toggle = 2 Hz blink */
        pdTRUE,                    /* auto-reload (repeating)   */
        NULL,
        prvEStopBlinkCallback
    );

    /* All LEDs off at boot */
    LEDWrite(LED_D1 | LED_D2 | LED_D3 | LED_D4, 0);

    DebugPrintf(DBG_INIT "State Manager initialised\r\n");
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

void StateManager_TriggerFaultAck(void)
{
    if (s_events != NULL)
    {
        xEventGroupSetBits(s_events, EVENT_FAULT_ACK);
    }
}


SystemState_t StateManager_GetState(void)
{
    SystemState_t state;

    if (s_stateMutex != NULL)
    {
        xSemaphoreTake(s_stateMutex, portMAX_DELAY);
    }

    state = s_state;

    if (s_stateMutex != NULL)
    {
        xSemaphoreGive(s_stateMutex);
    }

    return state;
}

const char *StateManager_GetStateString(void)
{
    return prvStateToString(StateManager_GetState());
}