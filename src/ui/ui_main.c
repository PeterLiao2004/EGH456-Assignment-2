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
#define EVENT_BTN_TOGGLE_PLOT   (1 << 4) //Sw2 was pressed
#define EVENT_DISPLAY_HOLD      (1 << 5) // Sw1 was pressed
#define EVENT_DISPLAY_BITS      (EVENT_HIGH_THRESHOLD | EVENT_LOW_THRESHOLD | EVENT_SENSOR_MISSED | EVENT_QUEUE_FULL | EVENT_BTN_TOGGLE_PLOT | EVENT_DISPLAY_HOLD)

#define HIGH_LUX_THRESHOLD      80
#define LOW_LUX_THRESHOLD       10

// Sysclock from main.c
extern uint32_t g_ui32SysClock;

extern void i2cOptDriverInit(void);

volatile uint32_t g_ui32TimeStamp = 0;
volatile static uint32_t g_pui32ButtonPressed = NULL;

static volatile bool g_plotMode = false;
static bool g_displayHold = false;  

extern SemaphoreHandle_t xSW1Semaphore;
extern SemaphoreHandle_t xSW2Semaphore;
extern SemaphoreHandle_t xTimerSemaphore;
extern SemaphoreHandle_t xUARTMutex;
extern SemaphoreHandle_t xQueueDroppingMutex;

static QueueHandle_t xSensorQueue = NULL;
static EventGroupHandle_t xSensorEventGroup = NULL;

static void prvTaskSW1(void *pvParameters);
static void prvTaskSW2(void *pvParameters);
static void prvSensorTask(void *pvParameters);
static void prvDisplayTask(void *pvParameters);

void vCreateTasks(void);

static void prvConfigureButton(void);
static void prvConfigureSensor(void);

struct sensorData
{
    uint32_t sequenceNum;
    TickType_t timestamp;
    uint16_t rawData;
    uint16_t convertedLux;
    uint16_t filteredLux;
    uint16_t missedSampleCount;
    uint16_t queueOverflowCount;
};

/*-----------------------------------------------------------*/

void vCreateTasks(void)
{
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

    xSensorQueue = xQueueCreate(mainQUEUE_LENGTH, sizeof(struct sensorData));
    if (xSensorQueue == NULL)
    {
        if (xSemaphoreTake(xUARTMutex, portMAX_DELAY))
        {
            UARTprintf("Queue creation failed\r\n");
            xSemaphoreGive(xUARTMutex);
        }
        return;
    }

    xTaskCreate(prvTaskSW1,
                "SW1",
                configMINIMAL_STACK_SIZE,
                NULL,
                tskIDLE_PRIORITY + 2,
                NULL);

    xTaskCreate(prvTaskSW2,
                "SW2",
                configMINIMAL_STACK_SIZE,
                NULL,
                tskIDLE_PRIORITY + 2,
                NULL);

    xTaskCreate(prvSensorTask,
                "Sensor",
                configMINIMAL_STACK_SIZE,
                NULL,
                tskIDLE_PRIORITY + 1,
                NULL);

    xTaskCreate(prvDisplayTask,
                "Display",
                configMINIMAL_STACK_SIZE * 6,
                NULL,
                tskIDLE_PRIORITY + 1,
                NULL);
}

/*-----------------------------------------------------------*/

static void prvTaskSW1(void *pvParameters)
{
    for (;;)
    {
        if (xSemaphoreTake(xSW1Semaphore, portMAX_DELAY) == pdPASS)
        {
            xEventGroupSetBits(xSensorEventGroup, EVENT_DISPLAY_HOLD);
        }
    }
}

/*-----------------------------------------------------------*/

static void prvTaskSW2(void *pvParameters)
{
    for (;;)
    {
        if (xSemaphoreTake(xSW2Semaphore, portMAX_DELAY) == pdPASS)
        {
            xEventGroupSetBits(xSensorEventGroup, EVENT_BTN_TOGGLE_PLOT);
        }
    }
}

/*-----------------------------------------------------------*/

static void prvSensorTask(void *pvParameters)
{
    prvConfigureSensor();

    struct sensorData xSensorData;
    bool success;
    float convertedLux_float = 0;

    int past1 = 0;
    int past2 = 0;
    int past3 = 0;
    int past4 = 0;

    xSensorData.sequenceNum = 0U;
    xSensorData.timestamp = 0U;
    xSensorData.rawData = 0U;
    xSensorData.convertedLux = 0U;
    xSensorData.filteredLux = 0U;
    xSensorData.missedSampleCount = 0U;
    xSensorData.queueOverflowCount = 0U;

    for (;;)
    {
        if (xSemaphoreTake(xTimerSemaphore, portMAX_DELAY) == pdPASS)
        {
            success = sensorOpt3001Read(&xSensorData.rawData);

            if (success)
            {
                xSensorData.sequenceNum++;
                xSensorData.timestamp = xTaskGetTickCount();

                sensorOpt3001Convert(xSensorData.rawData, &convertedLux_float);
                xSensorData.convertedLux = (uint16_t)convertedLux_float;

                past4 = past3;
                past3 = past2;
                past2 = past1;
                past1 = xSensorData.convertedLux;

                xSensorData.filteredLux = (past1 + past2 + past3 + past4) / 4;

                if (xSensorData.filteredLux > HIGH_LUX_THRESHOLD)
                {
                    xEventGroupSetBits(xSensorEventGroup, EVENT_HIGH_THRESHOLD);
                }

                if (xSensorData.filteredLux < LOW_LUX_THRESHOLD)
                {
                    xEventGroupSetBits(xSensorEventGroup, EVENT_LOW_THRESHOLD);
                }

                xSemaphoreTake(xQueueDroppingMutex, portMAX_DELAY);

                if (xQueueSend(xSensorQueue, (void *)&xSensorData, 0) != pdPASS)
                {
                    xSensorData.queueOverflowCount++;
                    xEventGroupSetBits(xSensorEventGroup, EVENT_QUEUE_FULL);
                }

                xSemaphoreGive(xQueueDroppingMutex);
            }
            else
            {
                xSensorData.missedSampleCount++;
                xEventGroupSetBits(xSensorEventGroup, EVENT_SENSOR_MISSED);
            }
        }
    }
}

/*-----------------------------------------------------------*/

static void prvDisplayTask(void *pvParameters)
{
    struct sensorData receivedSensorData;
    struct sensorData latestSensorData;
    tContext sContext;

    tRectangle screenRect = {0, 0, 319, 239};
    tRectangle topArea = {0, 0, 319, 39};
    tRectangle graphArea = {0, 40, 319, 239};

    char line1[40];
    char line2[40];

    static int x = 20;
    static int lastFilteredY = 200;
    static int lastRawY = 200;

    int filteredY;
    int rawY;
    int luxMax = 100;

    EventBits_t uxBits;
    EventBits_t uxStatusBits = 0;
    bool haveSensorData = false;
    bool redrawHeader = true;

    Kentec320x240x16_SSD2119Init(configCPU_CLOCK_HZ);
    GrContextInit(&sContext, &g_sKentec320x240x16_SSD2119);
    GrContextFontSet(&sContext, &g_sFontFixed6x8);

    GrContextForegroundSet(&sContext, ClrBlack);
    GrRectFill(&sContext, &screenRect);

    GrContextForegroundSet(&sContext, ClrWhite);
    GrStringDraw(&sContext, "OPT3001 Lux Plot", -1, 10, 10, false);

    for (;;)
    {
        uxBits = xEventGroupWaitBits(xSensorEventGroup,
                                     EVENT_DISPLAY_BITS,
                                     pdTRUE,
                                     pdFALSE,
                                     0); // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! portMAX_DELAY

        if ((uxBits & EVENT_BTN_TOGGLE_PLOT) != 0)
        {
            g_plotMode = !g_plotMode;
            redrawHeader = true;
        }

        if ((uxBits & EVENT_DISPLAY_HOLD) != 0)
        {
            g_displayHold = !g_displayHold;
            redrawHeader = true;
        }

        uxStatusBits |= (uxBits & (EVENT_HIGH_THRESHOLD | EVENT_LOW_THRESHOLD | EVENT_SENSOR_MISSED | EVENT_QUEUE_FULL));
        if ((uxStatusBits != 0) || (uxBits != 0))
        {
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
            UARTprintf("Seq Num: %5d | Timestamp: %5d | Raw Data: %5d | Converted Lux: %5d | Filtered Lux: %5d | Missed Samples: %5d | Queue Overflows: %5d\n",
                       receivedSensorData.sequenceNum,
                       receivedSensorData.timestamp,
                       receivedSensorData.rawData,
                       receivedSensorData.convertedLux,
                       receivedSensorData.filteredLux,
                       receivedSensorData.missedSampleCount,
                       receivedSensorData.queueOverflowCount);
            xSemaphoreGive(xUARTMutex);
        }

        if (redrawHeader)
        {
            GrContextForegroundSet(&sContext, ClrBlack);
            GrRectFill(&sContext, &topArea);

            GrContextForegroundSet(&sContext, ClrWhite);

            if (g_plotMode)
            {
                GrStringDraw(&sContext, "Mode: Raw + Filtered", -1, 10, 5, false);
            }
            else
            {
                GrStringDraw(&sContext, "Mode: Filtered Only", -1, 10, 5, false);
            }

            if (haveSensorData)
            {
                usprintf(line1, "Raw:%u Filt:%u",
                         latestSensorData.convertedLux,
                         latestSensorData.filteredLux);
                GrStringDraw(&sContext, line1, -1, 10, 22, false);
            }

            if ((uxStatusBits & EVENT_HIGH_THRESHOLD) != 0)
            {
                GrContextForegroundSet(&sContext, ClrRed);
                GrStringDraw(&sContext, "HIGH", -1, 250, 5, false);
            }

            if ((uxStatusBits & EVENT_LOW_THRESHOLD) != 0)
            {
                GrContextForegroundSet(&sContext, ClrYellow);
                GrStringDraw(&sContext, "LOW", -1, 250, 5, false);
            }

            if ((uxStatusBits & EVENT_SENSOR_MISSED) != 0)
            {
                GrContextForegroundSet(&sContext, ClrMagenta);
                GrStringDraw(&sContext, "MISS", -1, 250, 15, false);
            }

            if ((uxStatusBits & EVENT_QUEUE_FULL) != 0)
            {
                GrContextForegroundSet(&sContext, ClrCyan);
                GrStringDraw(&sContext, "FULL", -1, 250, 25, false);
            }

            if (g_displayHold)
            {
                GrContextForegroundSet(&sContext, ClrWhite);
                GrStringDraw(&sContext, "HOLD", -1, 210, 22, false);
            }

            uxStatusBits = 0;
            redrawHeader = false;
        }

        if (haveSensorData && !g_displayHold)
        {
            receivedSensorData = latestSensorData;

            filteredY = 200 - ((receivedSensorData.filteredLux * 120) / luxMax);
            rawY = 200 - ((receivedSensorData.convertedLux * 120) / luxMax);

            if (filteredY < 60)
            {
                filteredY = 60;
            }
            if (filteredY > 200)
            {
                filteredY = 200;
            }

            if (rawY < 60)
            {
                rawY = 60;
            }
            if (rawY > 200)
            {
                rawY = 200;
            }

            if (g_plotMode)
            {
                GrContextForegroundSet(&sContext, ClrYellow);
                GrLineDraw(&sContext, x - 1, lastRawY, x, rawY);

                GrContextForegroundSet(&sContext, ClrCyan);
                GrLineDraw(&sContext, x - 1, lastFilteredY, x, filteredY);
            }
            else
            {
                GrContextForegroundSet(&sContext, ClrCyan);
                GrLineDraw(&sContext, x - 1, lastFilteredY, x, filteredY);
            }

            lastFilteredY = filteredY;
            lastRawY = rawY;

            x++;

            if (x > 300)
            {
                x = 20;
                lastFilteredY = 200;
                lastRawY = 200;

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

static void prvConfigureSensor(void)
{
    bool success;

    vTaskDelay(pdMS_TO_TICKS(2000));

    SysCtlPeripheralEnable(SYSCTL_PERIPH_I2C2);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOL);

    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_I2C2))
    {
    }
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOL))
    {
    }

    GPIOPinConfigure(GPIO_PL1_I2C2SCL);
    GPIOPinConfigure(GPIO_PL0_I2C2SDA);

    GPIOPinTypeI2CSCL(GPIO_PORTL_BASE, GPIO_PIN_1);
    GPIOPinTypeI2C(GPIO_PORTL_BASE, GPIO_PIN_0);

    I2CMasterInitExpClk(I2C2_BASE, SysCtlClockGet(), false);

    i2cOptDriverInit();

    sensorOpt3001Init();

    success = sensorOpt3001Test();

    while (!success)
    {
        SysCtlDelay(g_ui32SysClock);

        xSemaphoreTake(xUARTMutex, portMAX_DELAY);
        UARTprintf("Test Failed, Trying again\n");
        xSemaphoreGive(xUARTMutex);

        sensorOpt3001Init();
        success = sensorOpt3001Test();
    }
}

/*-----------------------------------------------------------*/

void Timer0IntHandler(void)
{
    TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    xSemaphoreGiveFromISR(xTimerSemaphore, &xHigherPriorityTaskWoken);

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
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
