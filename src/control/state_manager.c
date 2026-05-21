/**
 * @file state_manager.c
 * @brief Control state machine implementation.
 *
 * Owns high-level transitions between stopped, starting, running, E-stop, and
 * fault-latched system states. External modules post event bits through the
 * public StateManager_Trigger* API; this task serialises those events and
 * invokes the motor subsystem as needed.
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

/** @brief Initial closed-loop target used when starting the motor. */
#define SAFE_START_RPM             400U

/** @brief Hall edge count required before considering startup feedback valid. */
#define HALL_VALID_EDGE_COUNT      12U

/** @brief Protects access to s_state for readers outside the state task. */
static SemaphoreHandle_t s_stateMutex;

/** @brief Event group used to receive asynchronous state transition requests. */
static EventGroupHandle_t s_events;

/** @brief Current high-level system state. */
static SystemState_t s_state;

/** @brief Reserved desired speed setpoint for future UI integration. */
static uint32_t s_desiredSpeed;

/** @brief FreeRTOS software timer used to blink LEDs during E-stop/fault. */
static TimerHandle_t s_estopBlinkTimer = NULL;


/**
 * @brief State manager task entry point.
 *
 * Waits for start, E-stop, and fault-acknowledge event bits and advances the
 * control state machine in response.
 *
 * @param pvParameters Unused FreeRTOS task parameter.
 */
static void prvStateManagerTask(void *pvParameters);

/**
 * @brief Change the current state and update state-dependent indicators.
 *
 * @param newState State to enter.
 */
static void prvSetState(SystemState_t newState);

/**
 * @brief Convert a system state enum to a printable string.
 *
 * @param state State value to convert.
 * @return Pointer to a static string literal.
 */
static const char *prvStateToString(SystemState_t state);

/**
 * @brief Toggle board LEDs while E-stop or fault indication is active.
 *
 * @param xTimer FreeRTOS timer handle, unused.
 */
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
 * @brief Toggles all board LEDs every 250ms when E-stop or fault is active.
 *
 * This runs from the FreeRTOS timer daemon, so no extra task stack is needed.
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
