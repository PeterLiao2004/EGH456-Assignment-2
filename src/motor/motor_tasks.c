/*
 * Motor subsystem task scaffold.
 *
 * This is the entry point for motor-owned RTOS tasks. Keep low-level motor
 * behavior in the motor module and let app_tasks.c only wire the module in.
 */
/* Standard includes. */
#include "driverlib/pin_map.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"

/* Hardware includes. */
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_gpio.h"
#include "inc/hw_types.h"
#include "driverlib/interrupt.h"
#include "driverlib/sysctl.h"
#include "drivers/rtos_hw_drivers.h"
#include "utils/uartstdio.h"
#include "driverlib/gpio.h"
#include "driverlib/pwm.h"

/* Motor library include. */
#include "motorlib.h"

/* Application includes. */
#include "motor_tasks.h"


/*-----------------------------------------------------------*/
/* Motor control API. Implementations will live in the motor module. */
void Motor_Init(void);
void Motor_Start(void);
void Motor_Stop(void);

uint32_t Motor_GetSpeed(void);
void Motor_SetSpeed(uint32_t rpm);

//void Motor_EStop(void);
//Motor_GetState(void);

/*-----------------------------------------------------------*/
// Private functions
static void prvMotorTask( void *pvParameters );
static void prvKickStartMotor( void );
static void prvReadHallSensors( bool *hall_a, bool *hall_b, bool *hall_c );
static void prvLogHallState( const char *tag, bool hall_a, bool hall_b, bool hall_c );
static void prvUpdateMeasuredMotorSpeed( uint32_t sample_ms, uint32_t *last_edge_count );

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

//--------------------Private Motor variables and macros--------------------//

//-------Hall sensors--------
// Hall sensor inputs from BoosterPack 1:
// Hall A -> PM3, Hall B -> PH2, Hall C -> PN2
#define HALL_A_PORT GPIO_PORTM_BASE
#define HALL_A_PIN  GPIO_PIN_3
#define HALL_B_PORT GPIO_PORTH_BASE
#define HALL_B_PIN  GPIO_PIN_2
#define HALL_C_PORT GPIO_PORTN_BASE
#define HALL_C_PIN  GPIO_PIN_2

// Hall sensor state variables
static volatile bool g_hallAState;
static volatile bool g_hallBState;
static volatile bool g_hallCState;
static volatile uint32_t g_hallEdgeCount;
static volatile bool g_hallStateChanged;

//---------Motor control varialbes--------
// Motor PWS period (in microseconds)
#define MOTOR_PWM_PERIOD 50U
// Speed sensing variables
#define SPEED_SAMPLE_MS 100U
#define HALL_EDGES_PER_MECH_REV 24U

// Motor state variables
static volatile bool g_motorInitialised = false;
static volatile bool g_motorRunning = false;
static volatile uint16_t g_motorDuty = 0;
static volatile uint32_t g_motorRpm = 0;

// Ramp control limits
#define CONTROL_PERIOD_MS       100U
#define ACCEL_LIMIT_RPM_PER_S   500U
#define DECEL_LIMIT_RPM_PER_S   500U

static uint32_t g_desiredRpm = 0U;    // user-requested RPM
static uint32_t g_referenceRpm = 0U;  // ramped RPM used by controller / duty map

//--------------------Private Motor functions--------------------//
// Read the current state of the hall effect sensors and return as bools
static void prvReadHallSensors( bool *hall_a, bool *hall_b, bool *hall_c )
{
    *hall_a = (GPIOPinRead(HALL_A_PORT, HALL_A_PIN) != 0U);
    *hall_b = (GPIOPinRead(HALL_B_PORT, HALL_B_PIN) != 0U);
    *hall_c = (GPIOPinRead(HALL_C_PORT, HALL_C_PIN) != 0U);
}
// Helper: Log current state of Hall sensor via UART
static void prvLogHallState( const char *tag, bool hall_a, bool hall_b, bool hall_c )
{
    UARTprintf("%s Hall state A=%d B=%d C=%d edges=%u\r\n",
               tag,
               hall_a ? 1 : 0,
               hall_b ? 1 : 0,
               hall_c ? 1 : 0,
               (unsigned int)g_hallEdgeCount);
}

/*  Convert Hall edge counts over a fixed sample period into mechanical RPM. 
    sample_ms: Sample period in milliseconds 
    last_edge_count: Pointer to the last edge count
    */ 
static void prvUpdateMeasuredMotorSpeed( uint32_t sample_ms, uint32_t *last_edge_count )
{
    uint32_t edge_count;
    uint32_t delta_edges;

    taskENTER_CRITICAL();
    edge_count = g_hallEdgeCount;
    taskEXIT_CRITICAL();

    // Get number of edges since last sample, update last edge count
    delta_edges = edge_count - *last_edge_count;
    *last_edge_count = edge_count;

    if ((sample_ms == 0U) || (HALL_EDGES_PER_MECH_REV == 0U))
    {
        g_motorRpm = 0U;
        return;
    }
    // Calculate RPM: (edges per minute) / (edges per mech rev)
    g_motorRpm = (delta_edges * 60000U) /
                 (HALL_EDGES_PER_MECH_REV * sample_ms);
}

// Kick start the motor by reading the hall sensor state and update motor
static void prvKickStartMotor( void )
{
    bool hall_a = false;
    bool hall_b = false;
    bool hall_c = false;

    // Do an initial read of the hall effect sensor GPIO lines
    prvReadHallSensors(&hall_a, &hall_b, &hall_c);
    g_hallAState = hall_a;
    g_hallBState = hall_b;
    g_hallCState = hall_c;
    // give the read hall effect sensor lines to updateMotor() to move the motor one single phase
    updateMotor(hall_a, hall_b, hall_c);
}

// Temporary: Rough mapping of RPM to duty cycle for testing. Replace with a proper control algorithm in the future.
static uint32_t prvRpmToDuty(uint32_t rpm)
{
    if (rpm == 0U)
    {
        return 0U;
    }
    else if (rpm <= 600U)
    {
        return 7U;     // ~590 rpm
    }
    else if (rpm <= 1000U)
    {
        return 9U;     // ~950 rpm
    }
    else if (rpm <= 1500U)
    {
        return 13U;    // ~1450 rpm
    }
    else if (rpm <= 2000U)
    {
        return 17U;    // ~2010 rpm
    }
    else if (rpm <= 2500U)
    {
        return 20U;    // ~2470 rpm
    }
    else if (rpm <= 3000U)
    {
        return 23U;    // ~2970 rpm
    }
    else if (rpm <= 3500U)
    {
        return 26U;    // ~3470 rpm
    }
    else if (rpm <= 4000U)
    {
        return 29U;    // ~3970 rpm
    }
    else if (rpm <= 4500U)
    {
        return 33U;    // ~4460 rpm
    }
    else if (rpm <= 5000U)
    {
        return 39U;    // ~5010 rpm
    }
    else if (rpm <= 5500U)
    {
        return 43U;    // ~5500 rpm
    }
    else if (rpm <= 6000U)
    {
        return 46U;    // ~5920 rpm
    }
    else
    {
        return 49U;    // ~6390 rpm max tested
    }
}

// Ramp Control: update reference RPM to desired RPM at fixed acceleration/deceleration limits.
static void prvUpdateSpeedRamp(void)
{
    uint32_t step;

    if (g_referenceRpm < g_desiredRpm)
    {
        // Accelerating
        step = (ACCEL_LIMIT_RPM_PER_S * CONTROL_PERIOD_MS) / 1000U;

        g_referenceRpm += step;

        if (g_referenceRpm > g_desiredRpm)
        {
            g_referenceRpm = g_desiredRpm;
        }
    }
    else if (g_referenceRpm > g_desiredRpm)
    {
        // Decelerating
        step = (DECEL_LIMIT_RPM_PER_S * CONTROL_PERIOD_MS) / 1000U;

        if (g_referenceRpm > step)
        {
            g_referenceRpm -= step;
        }
        else
        {
            g_referenceRpm = 0U;
        }

        if (g_referenceRpm < g_desiredRpm)
        {
            g_referenceRpm = g_desiredRpm;
        }
    }
}

//---------------------------Motor Task---------------------------//
/* Motor control task implementation. */
static void prvMotorTask( void *pvParameters )
{
    // DEBUG: local hall state variables for polling readings
    bool hall_a = false;
    bool hall_b = false;
    bool hall_c = false;

    // Variables for speed sensing.
    uint32_t last_edge_count = 0;
    TickType_t last_wake_time;

    ( void ) pvParameters;

    /* Initialise the motors and set the duty cycle (speed) in microseconds */
    Motor_Init();

    Motor_Start();

    // Temporary test target speed
    Motor_SetSpeed(1000U);

    last_wake_time = xTaskGetTickCount();

    /* Motor test - ramp up the duty cycle from 10% to 100%, than stop the motor */
    for (;;)
    {

        prvUpdateMeasuredMotorSpeed(SPEED_SAMPLE_MS, &last_edge_count);

        prvUpdateSpeedRamp();

        uint32_t duty_us = prvRpmToDuty(g_referenceRpm);

        setDuty(duty_us);

        UARTprintf("Desired=%u, Ref=%u, Actual=%u, Duty=%u\r\n",
                   (unsigned int)g_desiredRpm,
                   (unsigned int)g_referenceRpm,
                   (unsigned int)g_motorRpm,
                   (unsigned int)duty_us);

        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS( SPEED_SAMPLE_MS  ));

    }
}

/*-----------------------------------------------------------*/

/* Interrupt handlers */
void HallSensorHandler(void)
{
    bool hall_a;
    bool hall_b;
    bool hall_c;

    /* Read the current Hall state before updating commutation. */
    prvReadHallSensors(&hall_a, &hall_b, &hall_c);
    g_hallAState = hall_a;
    g_hallBState = hall_b;
    g_hallCState = hall_c;
    g_hallEdgeCount++;
    g_hallStateChanged = true;

    /* Clear any pending edge interrupts across the three Hall input ports. */
    GPIOIntClear(HALL_A_PORT, HALL_A_PIN);
    GPIOIntClear(HALL_B_PORT, HALL_B_PIN);
    GPIOIntClear(HALL_C_PORT, HALL_C_PIN);

    updateMotor(hall_a, hall_b, hall_c);

    // Could also add speed sensing code here too.
    
}
//--------------------Motor API functions--------------------//
// Initialise motor 
void Motor_Init(void)
{
    initMotorLib(MOTOR_PWM_PERIOD);

    // Start in a safe known state
    setDuty(0);
    stopMotor(1);

    // Reset motor state variables
    g_motorDuty = 0;
    g_motorRunning = false;

    prvReadHallSensors((bool *)&g_hallAState,
                       (bool *)&g_hallBState,
                       (bool *)&g_hallCState);

    g_hallEdgeCount = 0;
    g_hallStateChanged = false;
    g_motorRpm = 0U;

    g_motorInitialised = true;
}

// Start the motor
void Motor_Start(void)
{
    enableMotor();

    /* Use enough duty to overcome static friction. */
    setDuty(10);
    g_motorDuty = 10;

    prvKickStartMotor();

    /* Drop back to normal starting duty. */
    setDuty(5);
    g_motorDuty = 5;

    g_motorRunning = true;
}

// Stop the motor
void Motor_Stop(void)
{
    if (!g_motorInitialised)
    {
        return;
    }

    setDuty(0);
    stopMotor(1);

    g_motorDuty = 0;
    g_motorRunning = false;
    g_motorRpm = 0U;
}

// Return the most recent measured motor speed in RPM.
uint32_t Motor_GetSpeed(void)
{
    uint32_t rpm;

    taskENTER_CRITICAL();
    rpm = g_motorRpm;
    taskEXIT_CRITICAL();

    return rpm;
}

void Motor_SetSpeed(uint32_t rpm)
{
    g_desiredRpm = rpm;
}


//-----------------------Code Graveyard, ignore-----------------------//
// // log hall sensor state
// if(g_hallStateChanged){
//     g_hallStateChanged = false;
//     prvLogHallState("Edge", g_hallAState, g_hallBState, g_hallCState);
// }

//prvReadHallSensors(&hall_a, &hall_b, &hall_c);
// prvLogHallState("Poll", hall_a, hall_b, hall_c);


// if(duty_value >= MOTOR_PWM_PERIOD){
//     Motor_Stop();
//     UARTprintf("Motor Stopped\r\n");
//     vTaskDelay(pdMS_TO_TICKS( SPEED_SAMPLE_MS ));
//     continue;
// }

// prvReadHallSensors(&hall_a, &hall_b, &hall_c);
// prvLogHallState("Initial", hall_a, hall_b, hall_c);