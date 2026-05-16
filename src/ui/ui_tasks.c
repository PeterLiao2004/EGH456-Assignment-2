/*
 * UI subsystem task scaffold.
 *
 * This is the entry point for UI-owned RTOS tasks. Keep UI logic in the UI
 * module and let app_tasks.c only wire it in.
 */

// yellow - raw
// cyan - filtered
/* Standard includes. */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "event_groups.h"

/* Hardware includes. */
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"
#include "driverlib/sysctl.h"
#include "drivers/rtos_hw_drivers.h"

/* Sensor includes. */
#include "drivers/opt3001.h"

#include "driverlib/uart.h"
#include "driverlib/pin_map.h"
#include "utils/uartstdio.h"

#include "driverlib/debug.h"
#include "driverlib/rom.h"
#include "driverlib/i2c.h"
#include "utils/ustdlib.h"

#include "queue.h"

#include "grlib.h"
#include "drivers/Kentec320x240x16_ssd2119_spi.h"
// #include "drivers/touch.h"

/*-----------------------------------------------------------*/
#define mainQUEUE_LENGTH (4U)
#define mainQUEUE_SEND_TICKS_TO_WAIT ((TickType_t)0U)
#define mainQUEUE_RECEIVE_TICKS_TO_WAIT ((TickType_t)10U)

#define EVENT_HIGH_THRESHOLD    (1 << 0) // filtered flux is too high
#define EVENT_LOW_THRESHOLD     (1 << 1) // filtered flux is too low
#define EVENT_SENSOR_MISSED     (1 << 2) // sensor read failed / no conversation
#define EVENT_QUEUE_FULL        (1 << 3) // queue send failed - full
#define EVENT_DISPLAY_BITS      (EVENT_HIGH_THRESHOLD | EVENT_LOW_THRESHOLD | EVENT_SENSOR_MISSED | EVENT_QUEUE_FULL | EVENT_BTN_TOGGLE_PLOT | EVENT_DISPLAY_HOLD)

#define HIGH_LUX_THRESHOLD      80
#define LOW_LUX_THRESHOLD       10

// Sysclock from main.c
extern volatile uint32_t g_ui32SysClock;

extern void i2cOptDriverInit(void);

volatile uint32_t g_ui32TimeStamp = 0;
volatile static uint32_t g_pui32ButtonPressed = 0;

extern SemaphoreHandle_t xSW1Semaphore;
extern SemaphoreHandle_t xSW2Semaphore;
extern SemaphoreHandle_t xUARTMutex;
extern SemaphoreHandle_t xQueueDroppingMutex;

QueueHandle_t xSensorQueue = NULL; // sensor data -> UI
QueueHandle_t xSystemStatusQueue = NULL; // motor/control data -> UI
QueueHandle_t xUiCommandQueue = NULL; // UI data -> motor/control
static EventGroupHandle_t xSensorEventGroup = NULL;

static void prvTaskSW1(void *pvParameters);
static void prvTaskSW2(void *pvParameters);
static void prvDisplayTask(void *pvParameters);
static void vFormatTimeFromTicks(TickType_t startTick, char *timeString);

void vCreateUiTasks(void);

static void prvConfigureButton(void);

typedef struct
{
    uint32_t sequenceNum;
    TickType_t timestamp;

    float luxRaw;
    float luxFiltered;

    float temperatureC;
    float humidityRH;

    float accelerationFiltered;

    float distanceCm;
} SensorData_t;

// Motor states
typedef enum
{
    MOTOR_IDLE = 0,
    MOTOR_STARTING,
    MOTOR_RUNNING,
    MOTOR_STOPPING,
    MOTOR_STOPPED,
    MOTOR_FAULT_LATCHED,
    MOTOR_ESTOP
} MotorState_t;
static const char *pcMotorStateToString(MotorState_t state);

// Status of the system
typedef struct
{
    MotorState_t motorState;

    bool eStopActive;
    bool faultLatched;
    bool coolingOn;
    bool nightMode;

    float currentRPM;
    float desiredRPM;
    float motorPower;
} SystemStatus_t;

//UI commands
typedef enum
{
    UI_CMD_START_MOTOR = 0,
    UI_CMD_STOP_MOTOR,
    UI_CMD_SET_RPM,
    UI_CMD_SET_THRESHOLDS,
    UI_CMD_ACK_ESTOP,
    UI_CMD_TOGGLE_SENSOR_VIEW
} UiCommandType_t;

// thresholds for sensors
typedef struct
{
    float maxMotorPower;
    float maxAcceleration;
    float minDistanceCm;

    float maxTemperatureC;
    float maxHumidityRH;

    float nightLuxThreshold;
} UiThresholds_t;

// what UI sends to motor/control
typedef struct
{
    UiCommandType_t commandType;

    float desiredRPM;

    UiThresholds_t thresholds;
} UiCommand_t;

/*-----------------------------------------------------------*/

void vCreateUiTasks(void)
{
    BaseType_t taskCreated;

    prvConfigureButton();

    xSensorEventGroup = xEventGroupCreate();
    if (xSensorEventGroup == NULL)
    {
        if (xSemaphoreTake(xUARTMutex, portMAX_DELAY))
        {
            UARTprintf("Event group creation failed\r\n");
            xSemaphoreGive(xUARTMutex);
        }
        return;
    }

    xSensorQueue = xQueueCreate(mainQUEUE_LENGTH, sizeof(SensorData_t));
    if (xSensorQueue == NULL)
    {
        if (xSemaphoreTake(xUARTMutex, portMAX_DELAY))
        {
            UARTprintf("Queue creation failed\r\n");
            xSemaphoreGive(xUARTMutex);
        }
        return;
    }

    xSystemStatusQueue = xQueueCreate(mainQUEUE_LENGTH, sizeof(SystemStatus_t));
    if (xSystemStatusQueue == NULL)
    {
        if (xSemaphoreTake(xUARTMutex, portMAX_DELAY))
        {
            UARTprintf("System status queue creation failed\r\n");
            xSemaphoreGive(xUARTMutex);
        }
        return;
    }

    xUiCommandQueue = xQueueCreate(mainQUEUE_LENGTH, sizeof(UiCommand_t));
    if (xUiCommandQueue == NULL)
    {
        if (xSemaphoreTake(xUARTMutex, portMAX_DELAY))
        {
            UARTprintf("UI command queue creation failed\r\n");
            xSemaphoreGive(xUARTMutex);
        }
        return;
    }

    taskCreated = xTaskCreate(prvTaskSW1,
                              "SW1",
                              512,
                              NULL,
                              tskIDLE_PRIORITY + 2,
                              NULL);
    if (taskCreated != pdPASS)
    {
        UARTprintf("SW1 task creation failed\r\n");
    }

    taskCreated = xTaskCreate(prvTaskSW2,
                              "SW2",
                              512,
                              NULL,
                              tskIDLE_PRIORITY + 2,
                              NULL);
    if (taskCreated != pdPASS)
    {
        UARTprintf("SW2 task creation failed\r\n");
    }

    taskCreated = xTaskCreate(prvDisplayTask,
                              "Display",
                              2048,
                              NULL,
                              tskIDLE_PRIORITY + 1,
                              NULL);
    if (taskCreated != pdPASS)
    {
        UARTprintf("Display task creation failed\r\n");
    }
}

/*-----------------------------------------------------------*/

static void prvTaskSW1(void *pvParameters)
{
    UiCommand_t command;
    static bool motorRequestedOn = false;

    for (;;)
    {
        if (xSemaphoreTake(xSW1Semaphore, portMAX_DELAY) == pdPASS)
        {
            motorRequestedOn = !motorRequestedOn;

            if (motorRequestedOn)
            {
                command.commandType = UI_CMD_START_MOTOR;
            }
            else
            {
                command.commandType = UI_CMD_STOP_MOTOR;
            }

            command.desiredRPM = 0.0f;

            xQueueSend(xUiCommandQueue, &command, 0);
        }
    }
}

/*-----------------------------------------------------------*/

static void prvTaskSW2(void *pvParameters)
{
    UiCommand_t command;

    for (;;)
    {
        if (xSemaphoreTake(xSW2Semaphore, portMAX_DELAY) == pdPASS)
        {
            command.commandType = UI_CMD_ACK_ESTOP;
            command.desiredRPM = 0.0f;

            xQueueSend(xUiCommandQueue, &command, 0);
        }
    }
}

/*-----------------------------------------------------------*/
static const char *pcMotorStateToString(MotorState_t state)
{
    switch (state)
    {
        case MOTOR_IDLE:
            return "Idle";

        case MOTOR_STARTING:
            return "Starting";

        case MOTOR_RUNNING:
            return "Running";

        case MOTOR_STOPPING:
            return "Stopping";

        case MOTOR_STOPPED:
            return "Stopped";

        case MOTOR_FAULT_LATCHED:
            return "Fault Latched";

        case MOTOR_ESTOP:
            return "E-STOP";

        default:
            return "Unknown";
    }
}

/*-----------------------------------------------------------*/
// time
static void vFormatTimeFromTicks(TickType_t startTick, char *timeString)
{
    TickType_t nowTick = xTaskGetTickCount();
    uint32_t elapsedSeconds = (nowTick - startTick) / configTICK_RATE_HZ;

    uint32_t hours = 12;
    uint32_t minutes = 0;
    uint32_t seconds = 0;
    bool isPM = false;

    seconds = elapsedSeconds % 60;
    minutes = (elapsedSeconds / 60) % 60;
    hours = 12 + ((elapsedSeconds / 3600) % 12);

    if (hours > 12)
    {
        hours -= 12;
    }

    if (((elapsedSeconds / 3600) % 24) >= 12)
    {
        isPM = true;
    }

    usprintf(timeString,
             "%02u:%02u:%02u %s",
             hours,
             minutes,
             seconds,
             isPM ? "PM" : "AM");
}
/*-----------------------------------------------------------*/

static void prvDisplayTask(void *pvParameters)
{
    SensorData_t receivedSensorData;
    SensorData_t latestSensorData;

    SystemStatus_t receivedSystemStatus;
    SystemStatus_t latestSystemStatus;

    tContext sContext;

    latestSystemStatus.motorState = MOTOR_IDLE;
    latestSystemStatus.eStopActive = false;
    latestSystemStatus.faultLatched = false;
    latestSystemStatus.coolingOn = false;
    latestSystemStatus.nightMode = false;
    latestSystemStatus.currentRPM = 0.0f;
    latestSystemStatus.desiredRPM = 0.0f;
    latestSystemStatus.motorPower = 0.0f;

    tRectangle screenRect = {0, 0, 319, 239};

    // Top 40 pixels reserved for motor state/header
    tRectangle topArea = {0, 0, 319, 39};

    // Graph starts below the top area
    tRectangle graphArea = {0, 40, 319, 239};

    char line1[40];

    // clock
    char timeString[20];
    TickType_t clockStartTick;
    TickType_t lastClockUpdateTick;

    static int x = 20;
    static int lastFilteredY = 200;
    static int lastRawY = 200;

    int filteredY;
    int rawY;

    int graphTop = 50;      // leave small gap under header
    int graphBottom = 220;  // bottom of graph
    int graphHeight = graphBottom - graphTop;

    float luxMax = 100.0f;  // change this if your lux range is bigger

    bool haveSensorData = false;
    bool redrawHeader = true;

    Kentec320x240x16_SSD2119Init(configCPU_CLOCK_HZ);
    GrContextInit(&sContext, &g_sKentec320x240x16_SSD2119);
    GrContextFontSet(&sContext, &g_sFontFixed6x8);

    // Clear whole screen
    GrContextForegroundSet(&sContext, ClrBlack);
    GrRectFill(&sContext, &screenRect);

    clockStartTick = xTaskGetTickCount();
    lastClockUpdateTick = clockStartTick;

    for (;;)
    {
        if ((xTaskGetTickCount() - lastClockUpdateTick) >= pdMS_TO_TICKS(1000))
        {
            lastClockUpdateTick = xTaskGetTickCount();
            redrawHeader = true;
        }

        if (xQueueReceive(xSensorQueue,
                          (void *)&receivedSensorData,
                          mainQUEUE_RECEIVE_TICKS_TO_WAIT) == pdPASS)
        {
            latestSensorData = receivedSensorData;
            haveSensorData = true;
            redrawHeader = true;

            xSemaphoreTake(xUARTMutex, portMAX_DELAY);
            UARTprintf("Seq:%u Time:%u Lux Raw:%d Lux Filt:%d Temp:%d Hum:%d Acc:%d Dist:%d\r\n",
                    receivedSensorData.sequenceNum,
                    receivedSensorData.timestamp,
                    (int)receivedSensorData.luxRaw,
                    (int)receivedSensorData.luxFiltered,
                    (int)receivedSensorData.temperatureC,
                    (int)receivedSensorData.humidityRH,
                    (int)receivedSensorData.accelerationFiltered,
                    (int)receivedSensorData.distanceCm);
            xSemaphoreGive(xUARTMutex);
        }

        if (xQueueReceive(xSystemStatusQueue,
                        (void *)&receivedSystemStatus,
                        0) == pdPASS)
        {
            latestSystemStatus = receivedSystemStatus;
            redrawHeader = true;
        }

        if (redrawHeader)
        {
            // Clear only the top header area
            GrContextForegroundSet(&sContext, ClrBlack);
            GrRectFill(&sContext, &topArea);

            // formatting clock
            vFormatTimeFromTicks(clockStartTick, timeString);

            GrContextForegroundSet(&sContext, ClrWhite);
            GrStringDraw(&sContext,
                        timeString,
                        -1,
                        5,
                        25,
                        false);

            // Centre motor state text
            GrContextForegroundSet(&sContext, ClrWhite);
            GrStringDrawCentered(&sContext,
                                pcMotorStateToString(latestSystemStatus.motorState),
                                 -1,
                                 160,
                                 10,
                                 false);
            if (latestSystemStatus.eStopActive || latestSystemStatus.faultLatched)
            {
                GrContextForegroundSet(&sContext, ClrRed);
                GrStringDraw(&sContext, "FAULT/E-STOP", -1, 220, 8, false);
            }
            else if (latestSystemStatus.motorState == MOTOR_RUNNING)
            {
                GrContextForegroundSet(&sContext, ClrGreen);
                GrStringDraw(&sContext, "NORMAL", -1, 235, 8, false);
            }
            else
            {
                GrContextForegroundSet(&sContext, ClrOrange);
                GrStringDraw(&sContext, "STOPPED", -1, 235, 8, false);
            }

            GrContextForegroundSet(&sContext, ClrWhite);

            if (latestSystemStatus.nightMode)
            {
                GrStringDraw(&sContext, "Night", -1, 5, 8, false);
            }
            else
            {
                GrStringDraw(&sContext, "Day", -1, 5, 8, false);
            }

            if (haveSensorData)
            {
                usprintf(line1, "L:%d T:%d H:%d D:%d",
                        (int)latestSensorData.luxFiltered,
                        (int)latestSensorData.temperatureC,
                        (int)latestSensorData.humidityRH,
                        (int)latestSensorData.distanceCm);

                GrStringDrawCentered(&sContext,
                                     line1,
                                     -1,
                                     160,
                                     25,
                                     false);
            }

            redrawHeader = false;
        }

        if (haveSensorData)
        {
            // Convert lux values into screen y-coordinates
            filteredY = graphBottom -
                        (int)((latestSensorData.luxFiltered / luxMax) * graphHeight);

            rawY = graphBottom -
                (int)((latestSensorData.luxRaw / luxMax) * graphHeight);

            // Clamp filtered value inside graph area
            if (filteredY < graphTop)
            {
                filteredY = graphTop;
            }
            if (filteredY > graphBottom)
            {
                filteredY = graphBottom;
            }

            // Clamp raw value inside graph area
            if (rawY < graphTop)
            {
                rawY = graphTop;
            }
            if (rawY > graphBottom)
            {
                rawY = graphBottom;
            }

            // Draw raw lux in yellow
            GrContextForegroundSet(&sContext, ClrYellow);
            GrLineDraw(&sContext, x - 1, lastRawY, x, rawY);

            // Draw filtered lux in cyan
            GrContextForegroundSet(&sContext, ClrCyan);
            GrLineDraw(&sContext, x - 1, lastFilteredY, x, filteredY);

            lastRawY = rawY;
            lastFilteredY = filteredY;

            x++;

            // Reset graph when it reaches the right side
            if (x > 300)
            {
                x = 20;
                lastRawY = graphBottom;
                lastFilteredY = graphBottom;

                GrContextForegroundSet(&sContext, ClrBlack);
                GrRectFill(&sContext, &graphArea);
            }

            haveSensorData = false;
        }
    }
}

/*-----------------------------------------------------------*/

static void prvConfigureButton(void)
{
    ButtonsInit();

    GPIOIntTypeSet(BUTTONS_GPIO_BASE, ALL_BUTTONS, GPIO_FALLING_EDGE);

    GPIOIntEnable(BUTTONS_GPIO_BASE, ALL_BUTTONS);

    IntEnable(INT_GPIOJ);
}

/*-----------------------------------------------------------*/

void xButtonsHandler(void)
{
    BaseType_t xLEDTaskWoken;
    uint32_t ui32Status;

    xLEDTaskWoken = pdFALSE;

    ui32Status = GPIOIntStatus(BUTTONS_GPIO_BASE, true);

    GPIOIntClear(BUTTONS_GPIO_BASE, ui32Status);

    if ((xTaskGetTickCount() - g_ui32TimeStamp) > 100)
    {
        if ((ui32Status & USR_SW1) == USR_SW1)
        {
            g_pui32ButtonPressed = USR_SW1;
            xSemaphoreGiveFromISR(xSW1Semaphore, &xLEDTaskWoken);
        }
        else if ((ui32Status & USR_SW2) == USR_SW2)
        {
            g_pui32ButtonPressed = USR_SW2;
            xSemaphoreGiveFromISR(xSW2Semaphore, &xLEDTaskWoken);
        }

        portYIELD_FROM_ISR(xLEDTaskWoken);
    }

    g_ui32TimeStamp = xTaskGetTickCount();
}

/*-----------------------------------------------------------*/
