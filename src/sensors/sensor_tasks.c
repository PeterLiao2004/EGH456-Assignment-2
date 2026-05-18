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
#include <stdlib.h>
#include <math.h>

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
#include "drivers/sht31.h"
#include "drivers/bmi160.h"

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
#define sht31QUEUE_LENGTH (3U)   // 1Hz
#define bmi160QUEUE_LENGTH (30U) // 100Hz
// #define distQUEUE_LENGTH (10U) // 20Hz

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
extern SemaphoreHandle_t xSHT31ReadSemaphore;
extern SemaphoreHandle_t xBMI160ReadSemaphore;

extern SemaphoreHandle_t xOpt3001QueueDropMutex;
extern SemaphoreHandle_t xSHT31QueueDropMutex;
extern SemaphoreHandle_t xBMI160QueueDropMutex;

// Queues to hold data between tasks
static QueueHandle_t xOpt3001Queue = NULL;
static QueueHandle_t xSHT31Queue = NULL;
static QueueHandle_t xBMI160Queue = NULL;

// Called by app_tasks.c to create the sensor tasks
void vCreateSensorTasks(void);

// Tasks for sensors
static void prvSensorInitTask(void *pvParameters);
static void prvFastSchedulerTask(void *pvParameters);
static void prvSlowSchedulerTask(void *pvParameters);
static void prvOpt3001Task(void *pvParameters);
static void prvSHT31Task(void *pvParameters);
static void prvBMI160Task(void *pvParameters);
static void prvDisplayTask(void *pvParameters);

// Hardware configuration functions
static void prvConfigureOpt3001(void);
static void prvConfigureSHT31(void);
static void prvConfigureBMI160(void);

// Structs for passing data between tasks
struct opt3001Data
{
    uint32_t sequenceNum;
    TickType_t timestamp;
    float unfilteredLux;
    float filteredLux;
};

struct sht31Data
{
    uint32_t sequenceNum;
    TickType_t timestamp;
    float unfilteredTemp;
    float unfilteredHumidity;
    float filteredTemp;
    float filteredHumidity;
};

struct bmi160Data
{
    uint32_t sequenceNum;
    TickType_t timestamp;
    float unfilteredAccel;
    float filteredAccel;
};
/*----------------------------------------------------------- */
// Filter sizes
#define MAX_FILTER_SIZE (10U)    // max filter size for any sensor task (used for memory allocation of filter buffers)
#define OPT3001_FILTER_SIZE (4U) // 4-sample moving average filter for OPT3001
#define SHT31_FILTER_SIZE (3U)   // 3-sample moving average filter for SHT31
#define BMI160_FILTER_SIZE (5U)  // 5-sample moving average filter for BMI160

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
MovingAverageFilter_t sht31TempFilter;
MovingAverageFilter_t sht31HumidityFilter;
MovingAverageFilter_t bmi160Filter;

/*----------------------------------------------------------- */

void vCreateSensorTasks(void)
{
    xOpt3001Queue = xQueueCreate(opt3001QUEUE_LENGTH, sizeof(struct opt3001Data));
    if (xOpt3001Queue == NULL)
    {
        // Queue creation failed - handle error
        xSemaphoreTake(xUARTMutex, portMAX_DELAY);
        UARTprintf("Opt3001 Queue creation failed\n");
        xSemaphoreGive(xUARTMutex);
        return;
    }

    xSHT31Queue = xQueueCreate(sht31QUEUE_LENGTH, sizeof(struct sht31Data));
    if (xSHT31Queue == NULL)
    {
        // Queue creation failed - handle error
        xSemaphoreTake(xUARTMutex, portMAX_DELAY);
        UARTprintf("SHT31 Queue creation failed\n");
        xSemaphoreGive(xUARTMutex);
        return;
    }

    xBMI160Queue = xQueueCreate(bmi160QUEUE_LENGTH, sizeof(struct bmi160Data));
    if (xBMI160Queue == NULL)
    {
        // Queue creation failed - handle error
        xSemaphoreTake(xUARTMutex, portMAX_DELAY);
        UARTprintf("BMI160 Queue creation failed\n");
        xSemaphoreGive(xUARTMutex);
        return;
    }

    bool taskCreated;

    taskCreated = xTaskCreate(prvSensorInitTask,
                              "SensorInit",
                              configMINIMAL_STACK_SIZE,
                              NULL,
                              tskIDLE_PRIORITY + 3,
                              NULL);
    if (taskCreated != pdPASS)
    {
        UARTprintf("Sensor Init task creation failed\r\n");
    }

    taskCreated = xTaskCreate(prvFastSchedulerTask,
                              "FastScheduler",
                              configMINIMAL_STACK_SIZE,
                              NULL,
                              tskIDLE_PRIORITY + 2,
                              NULL);
    if (taskCreated != pdPASS)
    {
        UARTprintf("Fast Scheduler task creation failed\r\n");
    }

    taskCreated = xTaskCreate(prvSlowSchedulerTask,
                              "SlowScheduler",
                              configMINIMAL_STACK_SIZE,
                              NULL,
                              tskIDLE_PRIORITY + 2,
                              NULL);
    if (taskCreated != pdPASS)
    {
        UARTprintf("Slow Scheduler task creation failed\r\n");
    }

    taskCreated = xTaskCreate(prvOpt3001Task,
                              "Opt3001",
                              configMINIMAL_STACK_SIZE,
                              NULL,
                              tskIDLE_PRIORITY + 1,
                              NULL);
    if (taskCreated != pdPASS)
    {
        UARTprintf("Opt3001 task creation failed\r\n");
    }

    taskCreated = xTaskCreate(prvSHT31Task,
                              "SHT31",
                              configMINIMAL_STACK_SIZE,
                              NULL,
                              tskIDLE_PRIORITY + 1,
                              NULL);
    if (taskCreated != pdPASS)
    {
        UARTprintf("SHT31 task creation failed\r\n");
    }

    taskCreated = xTaskCreate(prvBMI160Task,
                              "BMI160",
                              configMINIMAL_STACK_SIZE,
                              NULL,
                              tskIDLE_PRIORITY + 1,
                              NULL);
    if (taskCreated != pdPASS)
    {
        UARTprintf("BMI160 task creation failed\r\n");
    }

    taskCreated = xTaskCreate(prvDisplayTask,
                              "Display",
                              configMINIMAL_STACK_SIZE,
                              NULL,
                              tskIDLE_PRIORITY + 1,
                              NULL);
    if (taskCreated != pdPASS)
    {
        UARTprintf("Display task creation failed\r\n");
    }
}

/*----------------------------------------------------------- */
static void prvSensorInitTask(void *pvParameters)
{
    // Short 2 second delay to allow the OPT3001 to power up before we attempt to configure it.
    // vTaskDelay(pdMS_TO_TICKS(2000));

    // The I2C2 peripheral must be enabled before use.
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

    // Configure the pin muxing for I2C0 functions on port N
    // This step is not necessary if your part does not support pin muxing.
    GPIOPinConfigure(GPIO_PN5_I2C2SCL);
    GPIOPinConfigure(GPIO_PN4_I2C2SDA);

    // Select the I2C function for these pins.  This function will also
    // configure the GPIO pins pins for I2C operation, setting them to
    // open-drain operation with weak pull-ups.  Consult the data sheet
    // to see which functions are allocated per pin.
    GPIOPinTypeI2CSCL(GPIO_PORTN_BASE, GPIO_PIN_5);
    GPIOPinTypeI2C(GPIO_PORTN_BASE, GPIO_PIN_4);

    I2CMasterInitExpClk(I2C2_BASE, SysCtlClockGet(), false);

    // Initialize I2C driver
    I2CDriverInit();

    // Configure Sensors
    prvConfigureOpt3001();
    prvConfigureSHT31();
    prvConfigureBMI160();

    vTaskDelete(NULL); // delete self task after initialization
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

            if (tick % 3 == 0)
            {
                // Trigger 100Hz tasks
                xSemaphoreGive(xBMI160ReadSemaphore);
            }

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
                xSemaphoreGive(xSHT31ReadSemaphore);
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
    bool success;
    struct opt3001Data xOpt3001Data;

    // Initialise filter
    MovingAverage_Init(&opt3001Filter, OPT3001_FILTER_SIZE);

    xOpt3001Data.sequenceNum = 0U;
    xOpt3001Data.timestamp = 0U;
    uint16_t rawData = 0U;
    xOpt3001Data.unfilteredLux = 0U;
    xOpt3001Data.filteredLux = 0U;

    for (;;)
    {
        if (xSemaphoreTake(xOpt3001ReadSemaphore, portMAX_DELAY) == pdPASS)
        {
            // Read raw data from sensor
            success = sensorOpt3001Read(&rawData);

            if (success)
            {
                xOpt3001Data.sequenceNum++;
                xOpt3001Data.timestamp = xTaskGetTickCount();

                sensorOpt3001Convert(rawData, &xOpt3001Data.unfilteredLux);

                // Update filter and get filtered value
                xOpt3001Data.filteredLux = MovingAverage_Update(&opt3001Filter, xOpt3001Data.unfilteredLux);

                // Send data to queue for display task
                xSemaphoreTake(xOpt3001QueueDropMutex, portMAX_DELAY);
                if (uxQueueSpacesAvailable(xOpt3001Queue) == 0)
                {
                    // Queue is full
                    // Handle by removing oldest message in queue and adding new message
                    struct opt3001Data dummyData;
                    xQueueReceive(xOpt3001Queue, (void *)&dummyData, 0); // remove oldest message in queue
                }
                xQueueSend(xOpt3001Queue, (void *)&xOpt3001Data, 0); // add new message to queue
                xSemaphoreGive(xOpt3001QueueDropMutex);
            }
        }
    }
}

/*----------------------------------------------------------- */

static void prvSHT31Task(void *pvParameters)
{
    bool success;
    struct sht31Data xSHT31Data;

    MovingAverage_Init(&sht31TempFilter, SHT31_FILTER_SIZE);
    MovingAverage_Init(&sht31HumidityFilter, SHT31_FILTER_SIZE);

    xSHT31Data.sequenceNum = 0U;
    xSHT31Data.timestamp = 0U;
    xSHT31Data.unfilteredTemp = 0.0f;
    xSHT31Data.unfilteredHumidity = 0.0f;
    xSHT31Data.filteredTemp = 0.0f;
    xSHT31Data.filteredHumidity = 0.0f;

    for (;;)
    {
        if (xSemaphoreTake(xSHT31ReadSemaphore, portMAX_DELAY) == pdPASS)
        {
            success = sensorSHT31Read(&xSHT31Data.unfilteredTemp, &xSHT31Data.unfilteredHumidity);
            if (success)
            {
                xSHT31Data.sequenceNum++;
                xSHT31Data.timestamp = xTaskGetTickCount();

                // Update filter and get filtered values
                xSHT31Data.filteredTemp = MovingAverage_Update(&sht31TempFilter, xSHT31Data.unfilteredTemp);
                xSHT31Data.filteredHumidity = MovingAverage_Update(&sht31HumidityFilter, xSHT31Data.unfilteredHumidity);

                // Send data to queue for display task
                xSemaphoreTake(xSHT31QueueDropMutex, portMAX_DELAY);
                if (uxQueueSpacesAvailable(xSHT31Queue) == 0)
                {
                    // Queue is full
                    // Handle by removing oldest message in queue and adding new message
                    struct sht31Data dummyData;
                    xQueueReceive(xSHT31Queue, (void *)&dummyData, 0); // remove oldest message in queue
                }
                xQueueSend(xSHT31Queue, (void *)&xSHT31Data, 0); // add new message to queue
                xSemaphoreGive(xSHT31QueueDropMutex);
            }
        }
    }
}

/*----------------------------------------------------------- */

static void prvBMI160Task(void *pvParameters)
{
    bool success;

    struct bmi160Data xBMI160Data;

    MovingAverage_Init(&bmi160Filter, BMI160_FILTER_SIZE);

    xBMI160Data.sequenceNum = 0U;
    xBMI160Data.timestamp = 0U;
    xBMI160Data.unfilteredAccel = 0.0f;
    xBMI160Data.filteredAccel = 0.0f;

    float rawAccelx = 0.0f;
    float rawAccely = 0.0f;
    float rawAccelz = 0.0f;

    for (;;)
    {
        if (xSemaphoreTake(xBMI160ReadSemaphore,
                           portMAX_DELAY) == pdPASS)
        {
            success = sensorBMI160Read(&rawAccelx, &rawAccely, &rawAccelz);

            if (success)
            {
                xBMI160Data.sequenceNum++;
                xBMI160Data.timestamp = xTaskGetTickCount();

                // Combine acceleration data into 1 value
                xBMI160Data.unfilteredAccel = fabsf(rawAccelx) + fabsf(rawAccely) + fabsf(rawAccelz);

                xBMI160Data.filteredAccel = MovingAverage_Update(&bmi160Filter, xBMI160Data.unfilteredAccel);

                // Send data to queue for display task
                xSemaphoreTake(xBMI160QueueDropMutex, portMAX_DELAY);
                if (uxQueueSpacesAvailable(xBMI160Queue) == 0)
                {
                    // Queue is full
                    // Handle by removing oldest message in queue and adding new message
                    struct bmi160Data dummyData;
                    xQueueReceive(xBMI160Queue, (void *)&dummyData, 0); // remove oldest message in queue
                }
                xQueueSend(xBMI160Queue, (void *)&xBMI160Data, 0); // add new message to queue
                xSemaphoreGive(xBMI160QueueDropMutex);
            }
        }
    }
}

/*----------------------------------------------------------- */

void prvDisplayTask(void *pvParameters)
{
    struct opt3001Data receivedOpt3001Data;
    struct sht31Data receivedSHT31Data;
    struct bmi160Data receivedBMI160Data;

    for (;;)
    {
        if (xQueueReceive(xOpt3001Queue, (void *)&receivedOpt3001Data, portMAX_DELAY) == pdPASS)
        {
            xSemaphoreTake(xUARTMutex, portMAX_DELAY);
            UARTprintf("OPT3001: UF Lux:%5d.%02d, F Lux:%5d.%02d\n",
                       (int32_t)receivedOpt3001Data.unfilteredLux,
                       (int32_t)(receivedOpt3001Data.unfilteredLux * 100) % 100,
                       (int32_t)receivedOpt3001Data.filteredLux,
                       (int32_t)(receivedOpt3001Data.filteredLux * 100) % 100);
            xSemaphoreGive(xUARTMutex);
        }

        if (xQueueReceive(xSHT31Queue, (void *)&receivedSHT31Data, 0) == pdPASS)
        {
            xSemaphoreTake(xUARTMutex, portMAX_DELAY);
            UARTprintf("SHT31: UF Temp:%3d.%02dC, UF Humidity:%3d.%02d%% | F Temp:%3d.%02dC, F Humidity:%3d.%02d%%\n",
                       (int32_t)receivedSHT31Data.unfilteredTemp,
                       (int32_t)(receivedSHT31Data.unfilteredTemp * 100) % 100,
                       (int32_t)receivedSHT31Data.unfilteredHumidity,
                       (int32_t)(receivedSHT31Data.unfilteredHumidity * 100) % 100,
                       (int32_t)receivedSHT31Data.filteredTemp,
                       (int32_t)(receivedSHT31Data.filteredTemp * 100) % 100,
                       (int32_t)receivedSHT31Data.filteredHumidity,
                       (int32_t)(receivedSHT31Data.filteredHumidity * 100) % 100);
            xSemaphoreGive(xUARTMutex);
        }

        if (xQueueReceive(xBMI160Queue, (void *)&receivedBMI160Data, 0) == pdPASS)
        {
            xSemaphoreTake(xUARTMutex, portMAX_DELAY);
            UARTprintf("BMI160: UF Accel:%2d.%02d | F Accel:%d.%02d\n",
                       (int32_t)receivedBMI160Data.unfilteredAccel,
                       (int32_t)(receivedBMI160Data.unfilteredAccel * 100) % 100,
                       (int32_t)receivedBMI160Data.filteredAccel,
                       (int32_t)(receivedBMI160Data.filteredAccel * 100) % 100);
            xSemaphoreGive(xUARTMutex);
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

    // Initialise sensor
    sensorOpt3001Init();

    success = sensorOpt3001Test();

    int fail_count = 0;

    // If the test fails, retry the full init + test sequence rather than
    // retesting a sensor that was never successfully enabled.
    while (!success)
    {
        fail_count++;
        xSemaphoreTake(xUARTMutex, portMAX_DELAY);
        UARTprintf("OPT3001 Test Failed, Trying again\n");
        xSemaphoreGive(xUARTMutex);

        vTaskDelay(pdMS_TO_TICKS(1000));

        sensorOpt3001Init();
        success = sensorOpt3001Test();
        if (fail_count >= 3)
        {
            xSemaphoreTake(xUARTMutex, portMAX_DELAY);
            UARTprintf("OPT3001 test failed 3 times, giving up\n");
            xSemaphoreGive(xUARTMutex);
            break;
        }
    }
}

/*----------------------------------------------------------- */

static void prvConfigureSHT31(void)
{
    bool success;

    success = sensorSHT31Test();

    int fail_count = 0;

    while (!success)
    {
        fail_count++;
        xSemaphoreTake(xUARTMutex, portMAX_DELAY);
        UARTprintf("SHT31 test failed\n");
        xSemaphoreGive(xUARTMutex);

        vTaskDelay(pdMS_TO_TICKS(1000));

        success = sensorSHT31Test();
        if (fail_count >= 3)
        {
            xSemaphoreTake(xUARTMutex, portMAX_DELAY);
            UARTprintf("SHT31 test failed 3 times, giving up\n");
            xSemaphoreGive(xUARTMutex);
            break;
        }
    }
}

/*----------------------------------------------------------- */

static void prvConfigureBMI160(void)
{
    bool success;

    sensorBMI160Init();

    success = sensorBMI160Test();

    int fail_count = 0;

    while (!success)
    {
        fail_count++;
        xSemaphoreTake(xUARTMutex, portMAX_DELAY);
        UARTprintf("BMI160 test failed\n");
        xSemaphoreGive(xUARTMutex);

        vTaskDelay(pdMS_TO_TICKS(1000));

        success = sensorBMI160Test();
        if (fail_count >= 3)
        {
            xSemaphoreTake(xUARTMutex, portMAX_DELAY);
            UARTprintf("BMI160 test failed 3 times, giving up\n");
            xSemaphoreGive(xUARTMutex);
            break;
        }
    }
}

/*----------------------------------------------------------- */