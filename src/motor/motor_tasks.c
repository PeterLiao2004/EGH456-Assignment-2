/**
 * @file    motor_tasks.c
 * @brief   Motor subsystem implementation - intent-based speed follower.
 *
 * Implements the motor mechanics: hall sensing, commutation, speed
 * measurement, ramp control, PI closed-loop controller, and PWM output.
 *
 * The motor module exposes an intent-based API to the state manager:
 *
 *     Motor_Start(initial_rpm)  ->  enable + kick + ramp at NORMAL rate
 *     Motor_SetSpeed(rpm)       ->  change target, NORMAL ramp rate
 *     Motor_Stop()              ->  ramp to 0 at NORMAL rate
 *     Motor_EStop()             ->  ramp to 0 at EMERGENCY rate
 *     Motor_Disable()           ->  immediate output disable, clear state
 *
 * Internally the motor task owns two ramp constants (NORMAL_RAMP_RPM_PER_S
 * and ESTOP_RAMP_RPM_PER_S) and selects between them based on the most
 * recent intent. The state manager never sees these values.
 *
 * RTOS structure:
 *   - prvMotorTask runs periodically at MOTOR_CONTROL_PERIOD_MS (5 ms).
 *   - Each iteration:
 *       1. Update measured RPM at the configured sample rate.
 *       2. If outputs are disabled, force duty to zero and skip control.
 *       3. Otherwise ramp the reference toward the target at the active
 *          ramp rate, run one PI iteration, and write duty to MotorLib.
 *
 * Shared variable protection:
 *   All shared variables tagged "PROTECT" are accessed from the motor task,
 *   the hall ISR, or external API callers. Critical sections guarantee
 *   atomicity for read-modify-write sequences and multi-byte values.
 */

/* Standard includes. */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"

/* Hardware includes. */
#include "driverlib/pin_map.h"
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_gpio.h"
#include "inc/hw_types.h"
#include "driverlib/interrupt.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/pwm.h"
#include "drivers/rtos_hw_drivers.h"
#include "utils/uartstdio.h"

/* Motor library include. */
#include "motorlib.h"

/* Application includes. */
#include "motor_tasks.h"

/*-----------------------------------------------------------------------------
 * Private prototypes
 *---------------------------------------------------------------------------*/
static void     prvMotorTask(void *pvParameters);
static void     prvKickStartMotor(void);
static void     prvReadHallSensors(bool *hall_a, bool *hall_b, bool *hall_c);
static void     prvUpdateMeasuredMotorSpeed(uint32_t sample_ms,
                                            uint32_t *last_edge_count);
static void     prvUpdateSpeedRamp(void);
static uint32_t prvUpdatePIController(uint32_t reference_rpm,
                                      uint32_t measured_rpm);
static int32_t  prvClampInt32(int32_t value,
                              int32_t min_value,
                              int32_t max_value);

/*-----------------------------------------------------------------------------
 * Configuration macros
 *---------------------------------------------------------------------------*/

/* --- Hall sensor pins (BoosterPack 1) --- */
#define HALL_A_PORT             GPIO_PORTM_BASE
#define HALL_A_PIN              GPIO_PIN_3
#define HALL_B_PORT             GPIO_PORTH_BASE
#define HALL_B_PIN              GPIO_PIN_2
#define HALL_C_PORT             GPIO_PORTN_BASE
#define HALL_C_PIN              GPIO_PIN_2

/* Hall edges per mechanical revolution. */
#define HALL_EDGES_PER_MECH_REV 24U

/* --- Motor PWM --- */
#define MOTOR_PWM_PERIOD        50U     /* PWM period in microseconds.      */

/* --- Timing --- */
#define MOTOR_CONTROL_PERIOD_MS 5U      /* Control loop period (ms).        */
#define SPEED_SAMPLE_MS         50U     /* Speed sampling period (ms).      */

/* --- Speed and duty limits --- */
#define MOTOR_SPEED_MIN_RPM     200U
#define MOTOR_SPEED_MAX_RPM     6200U
#define MOTOR_DUTY_MIN          1U
#define MOTOR_DUTY_MAX          49U

/* --- Ramp rates (RPM per second) ---
 * NORMAL is used for Motor_Start / Motor_SetSpeed / Motor_Stop.
 * ESTOP is used for Motor_EStop only.
 */
#define NORMAL_RAMP_RPM_PER_S   500U
#define ESTOP_RAMP_RPM_PER_S    1000U

/* --- PI controller --- */
#define PI_SCALE                1000L
#define PI_KP                   50L     /* Kp = PI_KP / PI_SCALE.           */
#define PI_KI                   1L      /* Ki = PI_KI / PI_SCALE.           */
#define PI_INTEGRAL_MIN         (-50000L)
#define PI_INTEGRAL_MAX         (50000L)

/*-----------------------------------------------------------------------------
 * Private state
 *---------------------------------------------------------------------------*/

/* PROTECT: written by HallSensorHandler() ISR, read by task and API. */
static volatile bool     g_hallAState;
static volatile bool     g_hallBState;
static volatile bool     g_hallCState;
static volatile uint32_t g_hallEdgeCount;

/* PROTECT: shared between motor task and Motor_* API callers. */
static volatile bool     g_motorInitialised = false;
static volatile bool     g_outputsEnabled   = false;
static volatile uint16_t g_motorDuty        = 0U;
static volatile uint32_t g_motorRpm         = 0U;

/* PROTECT: setpoint inputs written by external callers, read by motor task. */
static uint32_t g_targetRpm        = 0U;
static uint32_t g_activeRampRate   = NORMAL_RAMP_RPM_PER_S;

/* PROTECT: ramped reference RPM, written by motor task, readable via API. */
static uint32_t g_referenceRpm     = 0U;

/* PROTECT: PI controller integrator. */
static int32_t  g_piIntegral       = 0;

/*-----------------------------------------------------------------------------
 * Task creation
 *---------------------------------------------------------------------------*/

void vCreateMotorTasks(void)
{
    xTaskCreate(prvMotorTask,
                "Motor",
                configMINIMAL_STACK_SIZE,
                NULL,
                tskIDLE_PRIORITY + 2,
                NULL);
}

/*-----------------------------------------------------------------------------
 * Private helpers
 *---------------------------------------------------------------------------*/

/**
 * @brief Read the three hall sensor GPIO lines into booleans.
 */
static void prvReadHallSensors(bool *hall_a, bool *hall_b, bool *hall_c)
{
    *hall_a = (GPIOPinRead(HALL_A_PORT, HALL_A_PIN) != 0U);
    *hall_b = (GPIOPinRead(HALL_B_PORT, HALL_B_PIN) != 0U);
    *hall_c = (GPIOPinRead(HALL_C_PORT, HALL_C_PIN) != 0U);
}

/**
 * @brief Open-loop commutation kick to start the motor from rest.
 *
 * Reads current hall states and advances the MotorLib commutation by one
 * phase. Also resets the hall edge counter so the state manager sees only
 * edges from this start-up onward.
 */
static void prvKickStartMotor(void)
{
    bool hall_a = false;
    bool hall_b = false;
    bool hall_c = false;

    prvReadHallSensors(&hall_a, &hall_b, &hall_c);

    taskENTER_CRITICAL();
    g_hallAState    = hall_a;
    g_hallBState    = hall_b;
    g_hallCState    = hall_c;
    g_hallEdgeCount = 0U;
    taskEXIT_CRITICAL();

    updateMotor(hall_a, hall_b, hall_c);
}

/**
 * @brief Convert hall edge counts over a fixed sample period into mechanical RPM.
 *
 * Applies a 1st-order IIR filter (75/25 split) to the raw RPM for noise
 * suppression.
 *
 * @param sample_ms        Sample period over which edges were counted.
 * @param last_edge_count  In/out: previous edge count snapshot.
 */
static void prvUpdateMeasuredMotorSpeed(uint32_t sample_ms,
                                        uint32_t *last_edge_count)
{
    uint32_t edge_count;
    uint32_t delta_edges;
    uint32_t raw_rpm;

    taskENTER_CRITICAL();
    edge_count = g_hallEdgeCount;
    taskEXIT_CRITICAL();

    delta_edges      = edge_count - *last_edge_count;
    *last_edge_count = edge_count;

    if ((sample_ms == 0U) || (HALL_EDGES_PER_MECH_REV == 0U))
    {
        g_motorRpm = 0U;
        return;
    }

    raw_rpm = (delta_edges * (60000U / HALL_EDGES_PER_MECH_REV)) / sample_ms;

    /* IIR: filtered = 0.75 * old + 0.25 * raw */
    g_motorRpm = ((3U * g_motorRpm) + raw_rpm) / 4U;
}

/**
 * @brief Ramp the reference RPM towards the target at the active ramp rate.
 */
static void prvUpdateSpeedRamp(void)
{
    uint32_t step;
    uint32_t target;
    uint32_t rate;

    taskENTER_CRITICAL();
    target = g_targetRpm;
    rate   = g_activeRampRate;
    taskEXIT_CRITICAL();

    /* step (RPM per control period) = rate (RPM/s) * period (ms) / 1000 */
    step = (rate * MOTOR_CONTROL_PERIOD_MS) / 1000U;
    if (step == 0U)
    {
        step = 1U;     /* Guarantee progress at very low rates. */
    }

    if (g_referenceRpm < target)
    {
        g_referenceRpm += step;
        if (g_referenceRpm > target)
        {
            g_referenceRpm = target;
        }
    }
    else if (g_referenceRpm > target)
    {
        if (g_referenceRpm > step)
        {
            g_referenceRpm -= step;
        }
        else
        {
            g_referenceRpm = 0U;
        }

        if (g_referenceRpm < target)
        {
            g_referenceRpm = target;
        }
    }
}

/**
 * @brief Clamp a signed 32-bit value into [min_value, max_value].
 */
static int32_t prvClampInt32(int32_t value, int32_t min_value, int32_t max_value)
{
    if (value < min_value)
    {
        return min_value;
    }
    else if (value > max_value)
    {
        return max_value;
    }
    else
    {
        return value;
    }
}

/**
 * @brief Run one iteration of the PI speed controller.
 *
 * @param reference_rpm  Ramped target RPM.
 * @param measured_rpm   Latest measured RPM.
 * @return  New duty cycle, clamped to [MOTOR_DUTY_MIN, MOTOR_DUTY_MAX].
 *          Returns 0 if reference_rpm is zero.
 */
static uint32_t prvUpdatePIController(uint32_t reference_rpm,
                                      uint32_t measured_rpm)
{
    int32_t error;
    int32_t p_correction;
    int32_t i_correction;
    int32_t duty;
    int32_t integral_candidate;

    if (reference_rpm == 0U)
    {
        g_piIntegral = 0;
        return 0U;
    }

    /* Positive error -> motor too slow. */
    error              = (int32_t)reference_rpm - (int32_t)measured_rpm;
    p_correction       = (PI_KP * error) / PI_SCALE;
    integral_candidate = g_piIntegral + error;
    integral_candidate = prvClampInt32(integral_candidate,
                                       PI_INTEGRAL_MIN,
                                       PI_INTEGRAL_MAX);
    i_correction       = (PI_KI * integral_candidate) / PI_SCALE;

    duty = p_correction + i_correction;
    duty = prvClampInt32(duty, (int32_t)MOTOR_DUTY_MIN, (int32_t)MOTOR_DUTY_MAX);

    /* Anti-windup: only accumulate if not pushing further into saturation. */
    if (!((duty >= (int32_t)MOTOR_DUTY_MAX && error > 0) ||
          (duty <= (int32_t)MOTOR_DUTY_MIN && error < 0)))
    {
        g_piIntegral = integral_candidate;
    }

    return (uint32_t)duty;
}

/*-----------------------------------------------------------------------------
 * Motor task
 *---------------------------------------------------------------------------*/

/**
 * @brief Motor control task entry point.
 *
 * Polls the outputs-enabled flag each iteration. When disabled, forces duty
 * to zero. When enabled, runs the ramp and PI controller against the
 * current target RPM and active ramp rate.
 */
static void prvMotorTask(void *pvParameters)
{
    uint32_t   last_edge_count        = 0U;
    TickType_t last_speed_sample_time;
    TickType_t last_wake_time;

    (void)pvParameters;

    /* Initialise motor hardware before the loop starts. */
    Motor_Init();

    last_wake_time         = xTaskGetTickCount();
    last_speed_sample_time = last_wake_time;

    for (;;)
    {
        TickType_t now        = xTaskGetTickCount();
        uint32_t   elapsed_ms = (uint32_t)((now - last_speed_sample_time)
                                           * portTICK_PERIOD_MS);

        /* --- Speed measurement at the configured sample rate --- */
        if (elapsed_ms >= SPEED_SAMPLE_MS)
        {
            prvUpdateMeasuredMotorSpeed(elapsed_ms, &last_edge_count);
            last_speed_sample_time = now;
        }

        if (g_outputsEnabled)
        {
            uint32_t duty;

            prvUpdateSpeedRamp();
            duty = prvUpdatePIController(g_referenceRpm, g_motorRpm);

            setDuty(duty);
            g_motorDuty = (uint16_t)duty;
        }
        else
        {
            setDuty(0U);
            stopMotor(1);
            g_motorDuty = 0U;
        }

        vTaskDelayUntil(&last_wake_time,
                        pdMS_TO_TICKS(MOTOR_CONTROL_PERIOD_MS));
    }
}

/*-----------------------------------------------------------------------------
 * Hall sensor ISR
 *---------------------------------------------------------------------------*/

/**
 * @brief Hall sensor edge interrupt handler.
 *
 * Reads the latest hall states, advances commutation via MotorLib, and
 * increments the edge counter used for speed measurement.
 */
void HallSensorHandler(void)
{
    bool hall_a;
    bool hall_b;
    bool hall_c;

    prvReadHallSensors(&hall_a, &hall_b, &hall_c);

    g_hallAState = hall_a;
    g_hallBState = hall_b;
    g_hallCState = hall_c;
    g_hallEdgeCount++;

    /* Clear pending edge interrupts on all three hall input ports. */
    GPIOIntClear(HALL_A_PORT, HALL_A_PIN);
    GPIOIntClear(HALL_B_PORT, HALL_B_PIN);
    GPIOIntClear(HALL_C_PORT, HALL_C_PIN);

    updateMotor(hall_a, hall_b, hall_c);
}

/*-----------------------------------------------------------------------------
 * Lifecycle
 *---------------------------------------------------------------------------*/

void Motor_Init(void)
{
    initMotorLib(MOTOR_PWM_PERIOD);

    /* Start in a known safe state. */
    setDuty(0);
    stopMotor(1);

    g_motorDuty      = 0U;
    g_outputsEnabled = false;

    prvReadHallSensors((bool *)&g_hallAState,
                       (bool *)&g_hallBState,
                       (bool *)&g_hallCState);

    g_hallEdgeCount  = 0U;
    g_motorRpm       = 0U;
    g_targetRpm      = 0U;
    g_referenceRpm   = 0U;
    g_activeRampRate = NORMAL_RAMP_RPM_PER_S;
    g_piIntegral     = 0;

    g_motorInitialised = true;
}

/*-----------------------------------------------------------------------------
 * Intent-based control API
 *---------------------------------------------------------------------------*/

void Motor_Start(uint32_t initial_rpm)
{
    if (!g_motorInitialised)
    {
        return;
    }

    /* Clamp the initial target. */
    if (initial_rpm > MOTOR_SPEED_MAX_RPM)
    {
        initial_rpm = MOTOR_SPEED_MAX_RPM;
    }
    else if ((initial_rpm != 0U) && (initial_rpm < MOTOR_SPEED_MIN_RPM))
    {
        initial_rpm = MOTOR_SPEED_MIN_RPM;
    }

    /* Configure setpoints under critical section so they take effect together. */
    taskENTER_CRITICAL();
    g_targetRpm      = initial_rpm;
    g_activeRampRate = NORMAL_RAMP_RPM_PER_S;
    g_piIntegral     = 0;
    g_outputsEnabled = true;
    taskEXIT_CRITICAL();

    /* Enable hardware and kick commutation. */
    enableMotor();
    setDuty(10);
    g_motorDuty = 10U;
    prvKickStartMotor();
    setDuty(5);
    g_motorDuty = 5U;
}

void Motor_SetSpeed(uint32_t rpm)
{
    if (!g_outputsEnabled)
    {
        return;
    }

    if (rpm == 0U)
    {
        /* Treat as a normal stop. */
        Motor_Stop();
        return;
    }

    if (rpm < MOTOR_SPEED_MIN_RPM)
    {
        rpm = MOTOR_SPEED_MIN_RPM;
    }
    else if (rpm > MOTOR_SPEED_MAX_RPM)
    {
        rpm = MOTOR_SPEED_MAX_RPM;
    }

    taskENTER_CRITICAL();
    g_targetRpm      = rpm;
    g_activeRampRate = NORMAL_RAMP_RPM_PER_S;
    taskEXIT_CRITICAL();
}

void Motor_Stop(void)
{
    taskENTER_CRITICAL();
    g_targetRpm      = 0U;
    g_activeRampRate = NORMAL_RAMP_RPM_PER_S;
    taskEXIT_CRITICAL();
}

void Motor_EStop(void)
{
    taskENTER_CRITICAL();
    g_targetRpm      = 0U;
    g_activeRampRate = ESTOP_RAMP_RPM_PER_S;
    taskEXIT_CRITICAL();
}

void Motor_Disable(void)
{
    taskENTER_CRITICAL();
    g_outputsEnabled = false;
    g_targetRpm      = 0U;
    g_referenceRpm   = 0U;
    g_piIntegral     = 0;
    g_motorDuty      = 0U;
    taskEXIT_CRITICAL();

    setDuty(0);
    stopMotor(1);
}

/*-----------------------------------------------------------------------------
 * Status helpers
 *---------------------------------------------------------------------------*/

uint32_t Motor_GetHallEdgeCount(void)
{
    uint32_t count;

    taskENTER_CRITICAL();
    count = g_hallEdgeCount;
    taskEXIT_CRITICAL();

    return count;
}

bool Motor_HasReachedZero(void)
{
    bool stopped;

    taskENTER_CRITICAL();
    stopped = ((g_referenceRpm == 0U) && (g_motorRpm == 0U));
    taskEXIT_CRITICAL();

    return stopped;
}

/*-----------------------------------------------------------------------------
 * Telemetry
 *---------------------------------------------------------------------------*/

uint32_t Motor_GetSpeed(void)
{
    uint32_t rpm;

    taskENTER_CRITICAL();
    rpm = g_motorRpm;
    taskEXIT_CRITICAL();

    return rpm;
}

uint16_t Motor_GetDuty(void)
{
    uint16_t duty;

    taskENTER_CRITICAL();
    duty = g_motorDuty;
    taskEXIT_CRITICAL();

    return duty;
}

uint32_t Motor_GetReferenceRpm(void)
{
    uint32_t rpm;

    taskENTER_CRITICAL();
    rpm = g_referenceRpm;
    taskEXIT_CRITICAL();

    return rpm;
}

uint32_t Motor_GetTargetRpm(void)
{
    uint32_t rpm;

    taskENTER_CRITICAL();
    rpm = g_targetRpm;
    taskEXIT_CRITICAL();

    return rpm;
}