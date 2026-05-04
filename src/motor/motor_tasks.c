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
void Motor_Enable(void);
void Motor_Disable(void);

void Motor_Kickstart(void);
void Motor_SetDuty(float duty);

float Motor_GetSpeedRPM(void);
float Motor_GetDuty(void);
bool Motor_HasValidHallFeedback(void);
bool Motor_IsStopped(void);
/*-----------------------------------------------------------*/
// Private functions
static void prvMotorTask( void *pvParameters );
static void prvKickStartMotor( void );
static void prvReadHallSensors( bool *hall_a, bool *hall_b, bool *hall_c );
static void prvLogHallState( const char *tag, bool hall_a, bool hall_b, bool hall_c );

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
/*
 * Hall sensor inputs from BoosterPack 1:
 * Hall A -> PM3, Hall B -> PH2, Hall C -> PN2
 */
#define HALL_A_PORT GPIO_PORTM_BASE
#define HALL_A_PIN  GPIO_PIN_3
#define HALL_B_PORT GPIO_PORTH_BASE
#define HALL_B_PIN  GPIO_PIN_2
#define HALL_C_PORT GPIO_PORTN_BASE
#define HALL_C_PIN  GPIO_PIN_2

static volatile bool g_hallAState;
static volatile bool g_hallBState;
static volatile bool g_hallCState;
static volatile uint32_t g_hallEdgeCount;
static volatile bool g_hallStateChanged;

/*-----------------------------------------------------------*/

static void prvReadHallSensors( bool *hall_a, bool *hall_b, bool *hall_c )
{
    *hall_a = (GPIOPinRead(HALL_A_PORT, HALL_A_PIN) != 0U);
    *hall_b = (GPIOPinRead(HALL_B_PORT, HALL_B_PIN) != 0U);
    *hall_c = (GPIOPinRead(HALL_C_PORT, HALL_C_PIN) != 0U);
}
/*-----------------------------------------------------------*/

static void prvLogHallState( const char *tag, bool hall_a, bool hall_b, bool hall_c )
{
    UARTprintf("%s Hall state A=%d B=%d C=%d edges=%u\r\n",
               tag,
               hall_a ? 1 : 0,
               hall_b ? 1 : 0,
               hall_c ? 1 : 0,
               (unsigned int)g_hallEdgeCount);
}
/*-----------------------------------------------------------*/


/*-----------------------------------------------------------*/
/* Motor control task implementation. */
static void prvMotorTask( void *pvParameters )
{
    uint16_t duty_value = 7; // >10% to get it to start, duty cycle in microseconds
    uint16_t pwm_period = 50;
    bool hall_a = false;
    bool hall_b = false;
    bool hall_c = false;

    ( void ) pvParameters;

    /* Initialise the motors and set the duty cycle (speed) in microseconds */
    initMotorLib(pwm_period);
    enableMotor();

    /* Kick start the motor */
    // Do an initial read of the hall effect sensor GPIO lines
    // give the read hall effect sensor lines to updateMotor() to move the motor
    // one single phase
    // Recommendation is to use an interrupt on the hall effect sensors GPIO lines 
    // So that the motor continues to be updated every time the GPIO lines change from high to low
    // or low to high
    // Include the updateMotor function call in the ISR to achieve this behaviour.
    
    /* Set at >10% to get it to start */
    setDuty(10);

    //vTaskDelay(pdMS_TO_TICKS( 2000 ));
    UARTprintf("Starting Motor Test\r\n");
    // Kick start the motor
    prvKickStartMotor();
    vTaskDelay(pdMS_TO_TICKS( 100 ));
    // Set duty cycle back to the initial value after kick starting the motor
    setDuty(5);

    //prvReadHallSensors(&hall_a, &hall_b, &hall_c);
    //prvLogHallState("Initial", hall_a, hall_b, hall_c);

    /* Motor test - ramp up the duty cycle from 10% to 100%, than stop the motor */
    for (;;)
    {

        if(duty_value>=pwm_period){
            setDuty(0);
            stopMotor(1);
            UARTprintf("Motor Stopped\r\n");
            continue;
        }

        // log hall sensor state
        if(g_hallStateChanged){
            g_hallStateChanged = false;
            prvLogHallState("Edge", g_hallAState, g_hallBState, g_hallCState);
        }

        setDuty(duty_value);
        prvReadHallSensors(&hall_a, &hall_b, &hall_c);
        prvLogHallState("Poll", hall_a, hall_b, hall_c);
        vTaskDelay(pdMS_TO_TICKS( 250 ));
        duty_value++;

    }
}
/*-----------------------------------------------------------*/

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

