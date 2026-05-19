/**
 * @file switch_tasks.c
 * @brief Board switch handling implementation.
 *
 * Owns the GPIO interrupt setup and the small tasks that translate board
 * switch presses into control events.
 */

#include <stdint.h>
#include <stdbool.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"
#include "drivers/rtos_hw_drivers.h"

#include "control/state_manager.h"
#include "debug/debug_log.h"
#include "hardware/switch_tasks.h"

/** @brief Priority for switch event processing tasks. */
#define SWITCH_TASK_PRIORITY     (tskIDLE_PRIORITY + 2)

/** @brief Minimum accepted time between switch edges for software debounce. */
#define SWITCH_DEBOUNCE_TICKS    pdMS_TO_TICKS(100)

/** @brief Semaphore signalled by the switch ISR when SW1 is pressed. */
static SemaphoreHandle_t s_sw1Semaphore = NULL;

/** @brief Semaphore signalled by the switch ISR when SW2 is pressed. */
static SemaphoreHandle_t s_sw2Semaphore = NULL;

/** @brief Last accepted switch edge time, used for debounce. */
static volatile TickType_t s_lastButtonTick = 0;

/**
 * @brief Configure board switch GPIO pins and enable the Port J interrupt.
 */
static void prvConfigureButtons(void);

/**
 * @brief Task that converts SW1 presses into start requests.
 *
 * @param pvParameters Unused FreeRTOS task parameter.
 */
static void prvTaskSW1(void *pvParameters);

/**
 * @brief Task that converts SW2 presses into E-stop or fault-ack requests.
 *
 * @param pvParameters Unused FreeRTOS task parameter.
 */
static void prvTaskSW2(void *pvParameters);

void vCreateSwitchTasks(void)
{
    BaseType_t taskCreated;

    s_sw1Semaphore = xSemaphoreCreateBinary();
    s_sw2Semaphore = xSemaphoreCreateBinary();

    if ((s_sw1Semaphore == NULL) || (s_sw2Semaphore == NULL))
    {
        DebugPrintf(DBG_ERROR "Switch semaphore creation failed\r\n");
        return;
    }

    prvConfigureButtons();

    taskCreated = xTaskCreate(prvTaskSW1,
                              "SW1",
                              configMINIMAL_STACK_SIZE,
                              NULL,
                              SWITCH_TASK_PRIORITY,
                              NULL);
    if (taskCreated == pdPASS)
    {
        DebugPrintf(DBG_INIT "SW1 task created\r\n");
    }
    else
    {
        DebugPrintf(DBG_ERROR "SW1 task creation failed\r\n");
    }

    taskCreated = xTaskCreate(prvTaskSW2,
                              "SW2",
                              configMINIMAL_STACK_SIZE,
                              NULL,
                              SWITCH_TASK_PRIORITY,
                              NULL);
    if (taskCreated == pdPASS)
    {
        DebugPrintf(DBG_INIT "SW2 task created\r\n");
    }
    else
    {
        DebugPrintf(DBG_ERROR "SW2 task creation failed\r\n");
    }
}

static void prvConfigureButtons(void)
{
    ButtonsInit();

    GPIOIntTypeSet(BUTTONS_GPIO_BASE, ALL_BUTTONS, GPIO_FALLING_EDGE);
    GPIOIntEnable(BUTTONS_GPIO_BASE, ALL_BUTTONS);

    IntEnable(INT_GPIOJ);
}

static void prvTaskSW1(void *pvParameters)
{
    (void)pvParameters;

    for (;;)
    {
        if (xSemaphoreTake(s_sw1Semaphore, portMAX_DELAY) == pdPASS)
        {
            StateManager_TriggerStart();
            DebugPrintf(DBG_HARDWARE "SW1 pressed: START event fired\r\n");
        }
    }
}

static void prvTaskSW2(void *pvParameters)
{
    (void)pvParameters;

    for (;;)
    {
        if (xSemaphoreTake(s_sw2Semaphore, portMAX_DELAY) == pdPASS)
        {
            if (StateManager_GetState() == SYSTEM_STATE_FAULT_LATCHED)
            {
                StateManager_TriggerFaultAck();
                DebugPrintf(DBG_HARDWARE "SW2 pressed: FAULT ACK event fired\r\n");
            }
            else
            {
                StateManager_TriggerEStop();
                DebugPrintf(DBG_HARDWARE "SW2 pressed: E-STOP event fired\r\n");
            }
        }
    }
}

void xButtonsHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint32_t ui32Status;
    TickType_t tickNow;

    ui32Status = GPIOIntStatus(BUTTONS_GPIO_BASE, true);
    GPIOIntClear(BUTTONS_GPIO_BASE, ui32Status);

    tickNow = xTaskGetTickCountFromISR();

    if ((tickNow - s_lastButtonTick) > SWITCH_DEBOUNCE_TICKS)
    {
        if (((ui32Status & USR_SW1) == USR_SW1) && (s_sw1Semaphore != NULL))
        {
            xSemaphoreGiveFromISR(s_sw1Semaphore, &xHigherPriorityTaskWoken);
        }
        else if (((ui32Status & USR_SW2) == USR_SW2) && (s_sw2Semaphore != NULL))
        {
            xSemaphoreGiveFromISR(s_sw2Semaphore, &xHigherPriorityTaskWoken);
        }

        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }

    s_lastButtonTick = tickNow;
}
