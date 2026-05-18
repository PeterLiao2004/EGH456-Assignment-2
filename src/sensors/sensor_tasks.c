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
#include "drivers/i2c_driver.h"
#include "drivers/opt3001.h"

#include "driverlib/uart.h"
#include "driverlib/pin_map.h"
#include "utils/uartstdio.h"

#include "driverlib/debug.h"
#include "driverlib/rom.h"
#include "driverlib/i2c.h"
#include "utils/ustdlib.h"
#include "ui/ui_tasks.h"

#include "queue.h"

#include "sensor_tasks.h"

/*----------------------------------------------------------- */
// Sysclock from main.c
extern volatile uint32_t g_ui32SysClock;

// Drivers for sensors
extern void I2CDriverInit(void);

// Semaphores for triggering tasks
extern SemaphoreHandle_t xUARTMutex;
extern SemaphoreHandle_t xFastTimerSemaphore;
extern SemaphoreHandle_t xSlowTimerSemaphore;

extern SemaphoreHandle_t xOpt3001ReadSemaphore;

// Called by app_tasks.c to create the sensor tasks
void vCreateSensorTasks(void);

// Tasks for sensors
static void prvFastSchedulerTask(void *pvParameters);
static void prvSlowSchedulerTask(void *pvParameters);
static void prvOpt3001Task(void *pvParameters);

// Hardware configuration functions
static void prvConfigureOpt3001(void);

/*----------------------------------------------------------- */
// Filter sizes
#define MAX_FILTER_SIZE (10U)    // max filter size for any sensor task (used for memory allocation of filter buffers)
#define OPT3001_FILTER_SIZE (4U) // 4-sample moving average filter for OPT3001

// Create structure for moving average filter
typedef struct
{
    float buffer[MAX_FILTER_SIZE]; // max memory allocated for filter buffer
    uint16_t size;                 // filter window size
    uint16_t index;                // current write position
    uint16_t count;                // number of valid samples currently stored
    float sum;                     // running sum

} MovingAverageFilter_t;

bool MovingAverage_Init(MovingAverageFilter_t *filter, uint16_t filterSize);
float MovingAverage_Update(MovingAverageFilter_t *filter, float newValue);

// Declare filters
MovingAverageFilter_t opt3001Filter;

/*----------------------------------------------------------- */

void vCreateSensorTasks(void)
{
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

            if (tick >= 300)
            {
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

            if (tick >= 2)
            {
                // Reset tick count
                tick = 0;
            }
        }
    }
}

/*----------------------------------------------------------- */

static void prvOpt3001Task(void *pvParameters)
{
    prvConfigureOpt3001();

    bool success;
    Opt3001Data_t opt3001Data;

    // Initialise filter
    MovingAverage_Init(&opt3001Filter, OPT3001_FILTER_SIZE);

    opt3001Data.sequenceNum = 0U;
    opt3001Data.timestamp = 0U;
    uint16_t rawData = 0U;
    opt3001Data.luxRaw = 0.0f;
    opt3001Data.luxFiltered = 0.0f;
    opt3001Data.temperatureC = 0.0f;
    opt3001Data.humidityRH = 0.0f;
    opt3001Data.accelerationFiltered = 0.0f;
    opt3001Data.distanceCm = 0.0f;

    for (;;)
    {
        if (xSemaphoreTake(xOpt3001ReadSemaphore, portMAX_DELAY) == pdPASS)
        {
            // Read raw data from sensor
            success = sensorOpt3001Read(&rawData);

            if (success)
            {
                opt3001Data.sequenceNum++;
                opt3001Data.timestamp = xTaskGetTickCount();

                sensorOpt3001Convert(rawData, &opt3001Data.luxRaw);

                // Update filter and get filtered value
                opt3001Data.luxFiltered = MovingAverage_Update(&opt3001Filter, opt3001Data.luxRaw);

                if (xOpt3001Queue != NULL)
                {
                    xQueueSend(xOpt3001Queue, &opt3001Data, 0);
                }

                xSemaphoreTake(xUARTMutex, portMAX_DELAY);
                UARTprintf("Seq Num: %5d | Timestamp: %5d | Unfiltered Lux: %5d.%02d | Filtered Lux: %5d.%02d\n",
                           opt3001Data.sequenceNum,
                           opt3001Data.timestamp,
                           (int32_t)(opt3001Data.luxRaw * 100.0f) / 100,
                           (int32_t)(opt3001Data.luxRaw * 100.0f) % 100,
                           (uint32_t)(opt3001Data.luxFiltered * 100.0f) / 100,
                           (uint32_t)(opt3001Data.luxFiltered * 100.0f) % 100);
                xSemaphoreGive(xUARTMutex);
            }
        }
    }
}

/*----------------------------------------------------------- */

void Timer0IntHandler(void)
{
    // Clear the timer interrupt.
    TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);

    // Prepare context switch flag
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // Give semaphore from ISR
    xSemaphoreGiveFromISR(xFastTimerSemaphore, &xHigherPriorityTaskWoken);

    // Request context switch if needed
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void Timer2IntHandler(void)
{
    // Clear the timer interrupt.
    TimerIntClear(TIMER2_BASE, TIMER_TIMA_TIMEOUT);

    // Prepare context switch flag
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // Give semaphore from ISR
    xSemaphoreGiveFromISR(xSlowTimerSemaphore, &xHigherPriorityTaskWoken);

    // Request context switch if needed
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void Timer1IntHandler(void)
{
    // Timer1 is reserved for the touchscreen driver.
    TimerIntClear(TIMER1_BASE, TIMER_TIMA_TIMEOUT);
}

/*----------------------------------------------------------- */

// Function to initialise the moving average filter
bool MovingAverage_Init(MovingAverageFilter_t *filter, uint16_t filterSize)
{
    if (filterSize > MAX_FILTER_SIZE)
    {
        return false;
    }

    filter->size = filterSize;
    filter->index = 0;
    filter->count = 0;
    filter->sum = 0.0f;

    // Clear buffer
    for (uint16_t i = 0; i < MAX_FILTER_SIZE; i++)
    {
        filter->buffer[i] = 0.0f;
    }

    return true;
}

// Update function for moving average filter
float MovingAverage_Update(MovingAverageFilter_t *filter, float newValue)
{
    // Remove oldest sample from running sum
    filter->sum -= filter->buffer[filter->index];

    // Store new sample
    filter->buffer[filter->index] = newValue;

    // Add new sample to running sum
    filter->sum += newValue;

    // Advance circular index
    filter->index++;

    if (filter->index >= filter->size)
    {
        filter->index = 0;
    }

    // Increment sample count during startup
    if (filter->count < filter->size)
    {
        filter->count++;
    }

    // Return average
    return filter->sum / filter->count;
}

/*----------------------------------------------------------- */

static void prvConfigureOpt3001(void)
{
    bool success;

    // Short 2 second delay to allow the OPT3001 to power up before we attempt to configure it.
    // SysCtlDelay(2 * g_ui32SysClock);
    vTaskDelay(pdMS_TO_TICKS(2000));

    //
    // Clear the terminal and print the welcome message.
    //
    // xSemaphoreTake(xUARTMutex, portMAX_DELAY);
    // UARTprintf("OPT3001 Example\n");
    // xSemaphoreGive(xUARTMutex);

    //
    // The I2C2 peripheral must be enabled before use.
    //
    SysCtlPeripheralEnable(SYSCTL_PERIPH_I2C2);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPION);

    // Wait until both peripherals are fully clocked before touching their
    // registers. Skipping this causes intermittent failures on TM4C129x.
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_I2C2))
    {
    }
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPION))
    {
    }

    //
    // Configure the pin muxing for I2C0 functions on port N
    // This step is not necessary if your part does not support pin muxing.
    //
    GPIOPinConfigure(GPIO_PN5_I2C2SCL);
    GPIOPinConfigure(GPIO_PN4_I2C2SDA);

    //
    // Select the I2C function for these pins.  This function will also
    // configure the GPIO pins pins for I2C operation, setting them to
    // open-drain operation with weak pull-ups.  Consult the data sheet
    // to see which functions are allocated per pin.
    //
    GPIOPinTypeI2CSCL(GPIO_PORTN_BASE, GPIO_PIN_5);
    GPIOPinTypeI2C(GPIO_PORTN_BASE, GPIO_PIN_4);

    I2CMasterInitExpClk(I2C2_BASE, SysCtlClockGet(), false);

    I2CDriverInit(); // create semaphore + enable I2C interrupt

    //
    // Enable interrupts to the processor.
    //
    // IntMasterEnable();

    // Initialise sensor
    sensorOpt3001Init();

    // Test that sensor is set up correctly
    // xSemaphoreTake(xUARTMutex, portMAX_DELAY);
    // UARTprintf("Testing OPT3001 Sensor:\n");
    // xSemaphoreGive(xUARTMutex);
    success = sensorOpt3001Test();

    // If the test fails, retry the full init + test sequence rather than
    // retesting a sensor that was never successfully enabled.
    while (!success)
    {
        SysCtlDelay(g_ui32SysClock);
        xSemaphoreTake(xUARTMutex, portMAX_DELAY);
        UARTprintf("Test Failed, Trying again\n");
        xSemaphoreGive(xUARTMutex);
        sensorOpt3001Init();
        success = sensorOpt3001Test();
    }

    // xSemaphoreTake(xUARTMutex, portMAX_DELAY);
    // UARTprintf("All Tests Passed!\n\n");
    // xSemaphoreGive(xUARTMutex);
}

/*----------------------------------------------------------- */
