/*
 * example game task
 *
 */

/******************************************************************************
 *
 * This example is provided to illustrate how to use FreeRTOS and a pushbutton
 * interrupt to update a display.  It is not intended to be a complete game implementation,
 * but just to show how to set up the RTOS tasks and ISRs for a game application.  You will need to add
 * additional tasks and ISRs to implement a complete game.
 *
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
#include "inc/hw_types.h"
#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"
#include "driverlib/sysctl.h"
#include "driverlib/rom_map.h"
#include "driverlib/timer.h"
#include "drivers/rtos_hw_drivers.h"
#include "driverlib/uart.h"
#include "driverlib/pin_map.h"
#include "utils/uartstdio.h"

/* Display includes. */
#include "grlib.h"
#include "widget.h"
#include "canvas.h"

#include "drivers/Kentec320x240x16_ssd2119_spi.h"
#include "drivers/touch.h"

/*-----------------------------------------------------------*/
/*
 * Time stamp global variable.
 */
volatile uint32_t g_ui32TimeStamp = 0;

extern volatile uint32_t g_ui32SysClock;

/*
 * Global variable to log the last GPIO button pressed.
 * Make sure to protect this variable if accessed from multiple tasks
 */
volatile static uint32_t g_pui32ButtonPressed = 0;

/*
 * The binary semaphore used by the switch ISR & task.
 */
extern SemaphoreHandle_t xGameUpdateSemaphore;
extern SemaphoreHandle_t xGameStartSemaphore;
extern SemaphoreHandle_t xDisplaySemaphore;

//-------------Hazards----------------//
typedef struct
{
    int x;
    int y;
    int speed;
    int direction; // 1 = right, -1 = left
} Hazard;

//-------------Hazard Properties----------------//
#define NUM_HAZARDS 8
#define HAZARD_WIDTH 80
#define HAZARD_HEIGHT 20

// Y positions for the 4 lanes of hazards
#define LANE_1_Y 40
#define LANE_2_Y 80
#define LANE_3_Y 120
#define LANE_4_Y 160

//--------------Frog Properties---------------//
#define FROG_WIDTH 16
#define FROG_HEIGHT 16
#define FROG_STEP_X 10
#define FROG_STEP_Y 5
#define FROG_START_X (160 - 5)
#define FROG_START_Y 220

//-------------Game Properties----------------//
#define MAX_LEVELS 3
#define CROSSINGS_PER_LEVEL 1

//-------------Others---------------------//
#define START_LINE_Y 220
#define FINISH_LINE_Y 0
#define LINE_HEIGHT 20

//-------------Shared Game State-----------//
typedef struct
{
    // Frog State
    int frogX;
    int frogY;

    // Hazard State
    Hazard hazards[NUM_HAZARDS];

    // Game State
    int lives;
    int level;
    int crossings;
    bool gameStarted;
    bool gameOver;
    bool gameWon;
} GameState;

// -------------Initialise State--------------//
GameState gameState = {
    // Frog State
    .frogX = FROG_START_X, // center minus half width
    .frogY = FROG_START_Y, // near bottom

    // Hazard State
    .hazards = {
        {.x = 0, .y = LANE_1_Y, .speed = 4, .direction = 1},
        {.x = 180, .y = LANE_1_Y, .speed = 4, .direction = 1},

        {.x = 60, .y = LANE_2_Y, .speed = 2, .direction = -1},
        {.x = 220, .y = LANE_2_Y, .speed = 2, .direction = -1},

        {.x = 30, .y = LANE_3_Y, .speed = 3, .direction = 1},
        {.x = 200, .y = LANE_3_Y, .speed = 3, .direction = 1},

        {.x = 100, .y = LANE_4_Y, .speed = 2, .direction = -1},
        {.x = 260, .y = LANE_4_Y, .speed = 2, .direction = -1}},

    // Game State
    .lives = 3,
    .level = 1,
    .crossings = 0,
    .gameStarted = false,
    .gameOver = false,
    .gameWon = false};

//--------------Movement Flags----------------//
volatile bool moveLeft = false;
volatile bool moveRight = false;

/*
 * The tasks as described in the comments at the top of this file.
 */
static void prvDisplayTask(void *pvParameters);

static void prvGameLogicTask(void *pvParameters);

static void prvUpdateHazards(void);

static void prvResetFrog(void);

static void prvCheckHazardCollisions(void);

static void prvCheckTopReached(void);

static int prvGetActiveHazardCount(void);

static int prvGetLevelSpeedBonus(void);

static void prvAdvanceLevel(void);

static void prvDrawGameOverScreen(tContext *psContext, tRectangle *psRect);

static void prvDrawWinScreen(tContext *psContext, tRectangle *psRect);

static void prvDrawBus(tContext *psContext, int x, int y);

static void prvDrawFrog(tContext *psContext, int x, int y);

static void prvEraseBus(tContext *psContext, int x, int y);

static void prvDrawPlayfield(tContext *psContext);


/*
 * Called by main() to do example specific hardware configurations and to
 * create the Process Switch task.
 */
void vCreateTask(void);

/*
 * Hardware configuration for the LEDs.
 */
static void prvConfigureLED(void);

/*
 * Timer configuration
 */
static void prvConfigureHWTimer(void);

/*
 * Hardware configuration for the buttons SW1 and SW2 to generate interrupts.
 */
static void prvConfigureButton(void);
/*-----------------------------------------------------------*/

void vCreateDisplayTask(void)
{
    /* Light the initial LED. */
    prvConfigureLED();

    /* Configure the button to generate interrupts. */
    prvConfigureButton();

    /* Configure the hardware timer to run in periodic mode. */
    prvConfigureHWTimer();

    /* Create the task as described in the comments at the top of this file.
     *
     * The xTaskCreate parameters in order are:
     *  - The function that implements the task.
     *  - The text name for the LED Task - for debug only as it is not used by
     *    the kernel.
     *  - The size of the stack to allocate to the task.
     *  - The parameter passed to the task - just to check the functionality.
     *  - The priority assigned to the task.
     *  - The task handle is not required, so NULL is passed. */
    xTaskCreate(prvDisplayTask,
                "Display",
                configMINIMAL_STACK_SIZE,
                NULL,
                tskIDLE_PRIORITY + 1,
                NULL);

    xTaskCreate(prvGameLogicTask,
                "Game Logic",
                configMINIMAL_STACK_SIZE,
                NULL,
                tskIDLE_PRIORITY + 2,
                NULL);
}

static void prvConfigureLED(void)
{
    /* Configure initial LED state.  PinoutSet() has already configured
     * LED I/O. */
    LEDWrite(LED_D1, LED_D1);
}
/*-----------------------------------------------------------*/

static void prvConfigureButton(void)
{
    /* Initialize the LaunchPad Buttons. */
    ButtonsInit();

    /* Configure both switches to trigger an interrupt on a falling edge. */
    GPIOIntTypeSet(BUTTONS_GPIO_BASE, ALL_BUTTONS, GPIO_FALLING_EDGE);

    /* Enable the interrupt for LaunchPad GPIO Port in the GPIO peripheral. */
    GPIOIntEnable(BUTTONS_GPIO_BASE, ALL_BUTTONS);

    /* Enable the Port F interrupt in the NVIC. */
    IntEnable(INT_GPIOJ);

    /* Enable global interrupts in the NVIC. */
    IntMasterEnable();
}

/*-----------------------------------------------------------*/

static void prvConfigureHWTimer(void)
{
    /* The Timer 0 peripheral must be enabled for use. */
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);

    /* Configure Timer 0 in full-width periodic mode. */
    TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);

    /* Set the Timer 0A load value to run at 5 Hz. */
    TimerLoadSet(TIMER0_BASE, TIMER_A, g_ui32SysClock / 5);

    /* Configure the Timer 0A interrupt for timeout. */
    TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);

    /* Enable the Timer 0A interrupt in the NVIC. */
    IntEnable(INT_TIMER0A);

    /* Enable global interrupts in the NVIC. */
    IntMasterEnable();

    //
    // Start the timer used in this example Task
    // You may need change where this timer is enabled
    //
    TimerEnable(TIMER0_BASE, TIMER_A);
}

/*-----------------------------------------------------------*/

void xTimerHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);

    /* Wake the game task every tick */
    xSemaphoreGiveFromISR(xGameUpdateSemaphore, &xHigherPriorityTaskWoken);

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void xButtonsHandler(void)
{
    BaseType_t xTaskWoken;
    uint32_t ui32Status;

    /* Initialize the xTaskWoken as pdFALSE.  This is required as the
     * FreeRTOS interrupt safe API will change it if needed should a
     * context switch be required. */
    xTaskWoken = pdFALSE;

    /* Read the buttons interrupt status to find the cause of the interrupt. */
    ui32Status = GPIOIntStatus(BUTTONS_GPIO_BASE, true);

    /* Clear the interrupt. */
    GPIOIntClear(BUTTONS_GPIO_BASE, ui32Status);

    /* Debounce the input with 100ms filter */
    // Can reduce this value to increase response time of button
    // but if too small can lead to debouncing issues
    if ((xTaskGetTickCount() - g_ui32TimeStamp) > 50)
    {
        /* Log which button was pressed to trigger the ISR. */
        if ((ui32Status & USR_SW1) == USR_SW1)
        {
            moveLeft = true;
        }
        else if ((ui32Status & USR_SW2) == USR_SW2)
        {
            moveRight = true;
        }

        /* Give the semaphore to unblock prvProcessSwitchInputTask.  */
        // xSemaphoreGiveFromISR(xGameUpdateSemaphore, &xTaskWoken);

        // Give the start semaphore to unblock the display task if we are still on the intro screen
        if (!gameState.gameStarted)
        {
            xSemaphoreGiveFromISR(xGameStartSemaphore, &xTaskWoken);
            gameState.gameStarted = true;
        }

        /* This FreeRTOS API call will handle the context switch if it is
         * required or have no effect if that is not needed. */
        portYIELD_FROM_ISR(xTaskWoken);
    }

    /* Update the time stamp. */
    g_ui32TimeStamp = xTaskGetTickCount();
}
/*-----------------------------------------------------------*/

/*--------------------------TASK FUNCTIONS ---------------------------------*/
// You could either add a second game logic task function and setup in this file
// or create a second src (.c) file and define a game logic task function there
static void prvGameLogicTask(void *pvParameters)
{

    /* ===================== WAIT FOR BUTTON ===================== */

    /* Wait until ISR gives semaphore */
    xSemaphoreTake(xGameStartSemaphore, portMAX_DELAY);

    for (;;)
    {
        /* Block until the Push Button ISR gives the semaphore. */
        if (xSemaphoreTake(xGameUpdateSemaphore, portMAX_DELAY) == pdPASS)
        {
            taskENTER_CRITICAL();

            if (gameState.gameStarted && !gameState.gameOver && !gameState.gameWon)
            {

                //--------Forward Movement--------//
                gameState.frogY -= FROG_STEP_Y;

                //--------Button Movement--------//
                if (moveLeft)
                {
                    gameState.frogX -= FROG_STEP_X;
                    moveLeft = false;
                }

                if (moveRight)
                {
                    gameState.frogX += FROG_STEP_X;
                    moveRight = false;
                }

                /* Clamp to screen */
                if (gameState.frogX < 0)
                    gameState.frogX = 0;

                if (gameState.frogX > 320 - FROG_WIDTH)
                    gameState.frogX = 320 - FROG_WIDTH;

                if (gameState.frogY < 0)
                    gameState.frogY = 0;

                //--------Update Hazards--------//
                prvUpdateHazards();

                //--------Check Collisions--------//
                prvCheckHazardCollisions();

                //--------Check if Top Reached--------//
                prvCheckTopReached();
            }

            taskEXIT_CRITICAL();
        }

        // Signal the display task to update the screen with the new game state
        static bool endScreenSignalled = false;

        if (gameState.gameOver || gameState.gameWon)
        {
            if (!endScreenSignalled)
            {
                xSemaphoreGive(xDisplaySemaphore);
                endScreenSignalled = true;
            }
        }
        else
        {
            xSemaphoreGive(xDisplaySemaphore);
        }
    }
}

static void prvDisplayTask(void *pvParameters)
{
    tContext sContext;
    tRectangle sRect;

    /* Initialise the screen */
    Kentec320x240x16_SSD2119Init(g_ui32SysClock);
    GrContextInit(&sContext, &g_sKentec320x240x16_SSD2119);

    /* ===================== INTRO SCREEN ===================== */

    sRect.i16XMin = 0;
    sRect.i16YMin = 0;
    sRect.i16XMax = GrContextDpyWidthGet(&sContext) - 1;
    sRect.i16YMax = GrContextDpyHeightGet(&sContext) - 1;

    /* Clear screen */
    GrContextForegroundSet(&sContext, ClrBlack);
    GrRectFill(&sContext, &sRect);

    /* Title */
    GrContextForegroundSet(&sContext, ClrWhite);
    GrContextFontSet(&sContext, &g_sFontCm20);
    GrStringDrawCentered(&sContext, "FROGGER", -1, 160, 40, 0);

    /* Instructions */
    GrContextFontSet(&sContext, &g_sFontCm16);

    GrStringDrawCentered(&sContext, "SW1: Move Left", -1, 160, 100, 0);
    GrStringDrawCentered(&sContext, "SW2: Move Right", -1, 160, 130, 0);
    GrStringDrawCentered(&sContext, "Avoid Hazards!", -1, 160, 160, 0);

    // Draw demo frog and bus on intro screen
    prvDrawFrog(&sContext, 40, 40);
    prvDrawBus(&sContext, 220, 80);

    /* Start prompt */
    GrContextForegroundSet(&sContext, ClrYellow);
    GrStringDrawCentered(&sContext, "Press any button to start", -1, 160, 200, 0);

    // Wait until the game logic task signals that the game has started before proceeding to draw the initial game state
    xSemaphoreTake(xDisplaySemaphore, portMAX_DELAY);
    /* ===================== GAME START ===================== */

    /* Clear screen */
    GrContextForegroundSet(&sContext, ClrBlack);
    GrRectFill(&sContext, &sRect);

    //-----Draw start/finish lines-----//
    prvDrawPlayfield(&sContext);

    //----------Initialis hazard positions----------//
    int oldHazardX[NUM_HAZARDS];
    int oldHazardY[NUM_HAZARDS];

    //----------Initialise level/lives display----------//
    int lastLevel = gameState.level;
    int lastLives = gameState.lives;

    for (;;)
    {
        // Update the display with the new game state after the game logic task signals that the state has been updated
        xSemaphoreTake(xDisplaySemaphore, portMAX_DELAY);

        taskENTER_CRITICAL();

        UARTprintf("TEST\n");

        /* ================= GAME OVER SCREEN ================= */
        if (gameState.gameOver)
        {
            prvDrawGameOverScreen(&sContext, &sRect);
            break;
            ;
        }
        // =================== WIN SCREEN =================== //
        if (gameState.gameWon)
        {
            prvDrawWinScreen(&sContext, &sRect);
            break;
            ;
        }

        //---------------------Erase Previous Buses---------------------//
        int oldActiveHazards = prvGetActiveHazardCount();
        for (int i = 0; i < oldActiveHazards; i++)
        {
            prvEraseBus(&sContext, oldHazardX[i], oldHazardY[i]);
        }

        //---------Draw Frog--------//

        prvDrawFrog(&sContext, gameState.frogX, gameState.frogY);

        //---------Draw Hazards as Buses--------//
        int activeHazards = prvGetActiveHazardCount();
        for (int i = 0; i < activeHazards; i++)
        {
            prvDrawBus(&sContext, gameState.hazards[i].x, gameState.hazards[i].y);
        }

        // Redraw full playfield if level changed or life was lost
        if (gameState.level != lastLevel || gameState.lives != lastLives)
        {
            prvDrawPlayfield(&sContext);

            lastLevel = gameState.level;
            lastLives = gameState.lives;
        }

        //---------Store Hazard Positions for Next Erase--------//
        for (int i = 0; i < activeHazards; i++)
        {
            oldHazardX[i] = gameState.hazards[i].x;
            oldHazardY[i] = gameState.hazards[i].y;
        }

        //---------Draw Lives and Level--------//
        char statusText[50];

        GrContextForegroundSet(&sContext, ClrWhite);
        GrContextFontSet(&sContext, &g_sFontCm16b);

        usprintf(statusText, "Lives: %d   Level: %d",
                 gameState.lives, gameState.level);
        GrStringDraw(&sContext, statusText, -1, 10, 10, 0);

        taskEXIT_CRITICAL();

        // vTaskDelay(pdMS_TO_TICKS(10)); // Small delay to control refresh rate
    }
}

// --------------------------Helper Functions ---------------------------------*/
// ------------------Update Hazard Speeds Based on Level------------------//
static void prvUpdateHazards(void)
{
    int activeHazards = prvGetActiveHazardCount();
    int speedBonus = prvGetLevelSpeedBonus();

    for (int i = 0; i < activeHazards; i++)
    {
        int currentSpeed = gameState.hazards[i].speed + speedBonus;
        gameState.hazards[i].x += currentSpeed * gameState.hazards[i].direction;

        /* Wrap around screen */
        if (gameState.hazards[i].direction > 0)
        {
            if (gameState.hazards[i].x > 320)
            {
                gameState.hazards[i].x = -HAZARD_WIDTH;
            }
        }
        else
        {
            if (gameState.hazards[i].x < -HAZARD_WIDTH)
            {
                gameState.hazards[i].x = 320;
            }
        }
    }
}

//------------------Reset Frog Position------------------//
static void prvResetFrog(void)
{
    gameState.frogX = FROG_START_X;
    gameState.frogY = FROG_START_Y;
}

//------------------Check for Collision------------------//
static bool prvRectsOverlap(int x1, int y1, int w1, int h1,
                            int x2, int y2, int w2, int h2)
{
    if (x1 + w1 <= x2)
        return false;
    if (x2 + w2 <= x1)
        return false;
    if (y1 + h1 <= y2)
        return false;
    if (y2 + h2 <= y1)
        return false;
    return true;
}

//------------------Check for Hazard Collisions------------------//
static void prvCheckHazardCollisions(void)
{
    int activeHazards = prvGetActiveHazardCount();

    for (int i = 0; i < activeHazards; i++)
    {
        if (prvRectsOverlap(gameState.frogX, gameState.frogY, FROG_WIDTH, FROG_HEIGHT,
                            gameState.hazards[i].x, gameState.hazards[i].y, HAZARD_WIDTH, HAZARD_HEIGHT))
        {
            gameState.lives--;

            prvResetFrog();

            if (gameState.lives <= 0)
            {
                gameState.gameOver = true;
            }

            break;
        }
    }
}

//------------------Level Progression------------------//
static void prvAdvanceLevel(void)
{
    gameState.crossings++;

    if (gameState.level < MAX_LEVELS)
    {
        gameState.level++;
        prvResetFrog();
    }
    else
    {
        gameState.gameWon = true;
    }
}

static void prvCheckTopReached(void)
{
    if (gameState.frogY <= 0)
    {
        prvAdvanceLevel();
    }
}

//------------------Difficulty Scaling (Optional)------------------//
//------------------Get Active Hazard Count Based on Level------------------//
static int prvGetActiveHazardCount(void)
{
    if (gameState.level == 1)
        return 4;
    else if (gameState.level == 2)
        return 6;
    else
        return NUM_HAZARDS;
}
//------------------Get Speed Bonus Based on Level------------------//
static int prvGetLevelSpeedBonus(void)
{
    if (gameState.level == 1)
        return 0;
    else if (gameState.level == 2)
        return 2;
    else
        return 4;
}

//------------------Game Over and Win Screens------------------//
static void prvDrawGameOverScreen(tContext *psContext, tRectangle *psRect)
{
    GrContextForegroundSet(psContext, ClrBlack);
    GrRectFill(psContext, psRect);

    GrContextForegroundSet(psContext, ClrRed);
    GrContextFontSet(psContext, &g_sFontCm20);
    GrStringDrawCentered(psContext, "GAME OVER", -1, 160, 80, 0);

    GrContextForegroundSet(psContext, ClrWhite);
    GrContextFontSet(psContext, &g_sFontCm16);
    GrStringDrawCentered(psContext, "You lost all lives", -1, 160, 120, 0);
    GrStringDrawCentered(psContext, "Press reset to play again", -1, 160, 160, 0);

    // Optionally draw buses
    prvDrawBus(psContext, 40, 40);
    prvDrawBus(psContext, 120, 90);
    prvDrawBus(psContext, 200, 200);
}

static void prvDrawWinScreen(tContext *psContext, tRectangle *psRect)
{
    GrContextForegroundSet(psContext, ClrBlack);
    GrRectFill(psContext, psRect);

    GrContextForegroundSet(psContext, ClrGreen);
    GrContextFontSet(psContext, &g_sFontCm20);
    GrStringDrawCentered(psContext, "YOU WIN!", -1, 160, 80, 0);

    GrContextForegroundSet(psContext, ClrWhite);
    GrContextFontSet(psContext, &g_sFontCm16);
    GrStringDrawCentered(psContext, "You cleared all levels", -1, 160, 120, 0);
    GrStringDrawCentered(psContext, "Great job!", -1, 160, 160, 0);

    // Optionally draw a celebratory frog
    prvDrawFrog(psContext, 40, 40);
    prvDrawFrog(psContext, 120, 90);
    prvDrawFrog(psContext, 200, 200);
}

//------------------Draw Bus (Optional for Hazard Variety)------------------//
static void prvDrawBus(tContext *psContext, int x, int y)
{
    tRectangle busBody;
    tRectangle window;
    tRectangle wheel;

    // ---------------- Bus body ----------------
    busBody.i16XMin = x;
    busBody.i16YMin = y + 4;
    busBody.i16XMax = x + HAZARD_WIDTH - 1;
    busBody.i16YMax = y + HAZARD_HEIGHT - 1;

    GrContextForegroundSet(psContext, ClrHoneydew);
    GrRectFill(psContext, &busBody);

    // ---------------- Bus roof/front ----------------
    tRectangle topPart;
    topPart.i16XMin = x + 8;
    topPart.i16YMin = y;
    topPart.i16XMax = x + HAZARD_WIDTH - 12;
    topPart.i16YMax = y + 8;

    GrContextForegroundSet(psContext, ClrOrange);
    GrRectFill(psContext, &topPart);

    // ---------------- Windows ----------------
    GrContextForegroundSet(psContext, ClrCyan);

    for (int i = 0; i < 4; i++)
    {
        window.i16XMin = x + 10 + (i * 16);
        window.i16YMin = y + 6;
        window.i16XMax = window.i16XMin + 10;
        window.i16YMax = y + 13;
        GrRectFill(psContext, &window);
    }

    // ---------------- Door ----------------
    tRectangle door;
    door.i16XMin = x + HAZARD_WIDTH - 14;
    door.i16YMin = y + 8;
    door.i16XMax = x + HAZARD_WIDTH - 6;
    door.i16YMax = y + HAZARD_HEIGHT - 2;

    GrContextForegroundSet(psContext, ClrBlue);
    GrRectFill(psContext, &door);

    // ---------------- Wheels ----------------
    GrContextForegroundSet(psContext, ClrBlack);

    wheel.i16XMin = x + 10;
    wheel.i16YMin = y + HAZARD_HEIGHT - 4;
    wheel.i16XMax = x + 20;
    wheel.i16YMax = y + HAZARD_HEIGHT;
    GrRectFill(psContext, &wheel);

    wheel.i16XMin = x + HAZARD_WIDTH - 22;
    wheel.i16XMax = x + HAZARD_WIDTH - 12;
    GrRectFill(psContext, &wheel);
}

//------------------Draw Frog (Optional for More Detailed Sprite)------------------//
static void prvDrawFrog(tContext *psContext, int x, int y)
{
    tRectangle part;

    static int oldFrogX = FROG_START_X;
    static int oldFrogY = FROG_START_Y;

    /* erase old frog */
    part.i16XMin = oldFrogX;
    part.i16YMin = oldFrogY;
    part.i16XMax = oldFrogX + FROG_WIDTH - 1;
    part.i16YMax = oldFrogY + FROG_HEIGHT - 1;

    GrContextForegroundSet(psContext, ClrBlack);
    GrRectFill(psContext, &part);

    // ---------------- Body ----------------
    part.i16XMin = x + 2;
    part.i16YMin = y + 4;
    part.i16XMax = x + FROG_WIDTH - 3;
    part.i16YMax = y + FROG_HEIGHT - 2;

    GrContextForegroundSet(psContext, ClrLime);
    GrRectFill(psContext, &part);

    // ---------------- Head ----------------
    part.i16XMin = x + 3;
    part.i16YMin = y;
    part.i16XMax = x + FROG_WIDTH - 4;
    part.i16YMax = y + 6;

    GrRectFill(psContext, &part);

    // ---------------- Eyes ----------------
    GrContextForegroundSet(psContext, ClrWhite);

    // Left eye
    part.i16XMin = x + 2;
    part.i16YMin = y;
    part.i16XMax = x + 4;
    part.i16YMax = y + 2;
    GrRectFill(psContext, &part);

    // Right eye
    part.i16XMin = x + FROG_WIDTH - 5;
    part.i16XMax = x + FROG_WIDTH - 3;
    GrRectFill(psContext, &part);

    // Pupils
    GrContextForegroundSet(psContext, ClrBlack);

    part.i16XMin = x + 3;
    part.i16XMax = x + 3;
    part.i16YMin = y + 1;
    part.i16YMax = y + 1;
    GrRectFill(psContext, &part);

    part.i16XMin = x + FROG_WIDTH - 4;
    part.i16XMax = x + FROG_WIDTH - 4;
    GrRectFill(psContext, &part);

    // ---------------- Legs ----------------
    GrContextForegroundSet(psContext, ClrLimeGreen);

    // Left leg
    part.i16XMin = x;
    part.i16YMin = y + FROG_HEIGHT - 4;
    part.i16XMax = x + 3;
    part.i16YMax = y + FROG_HEIGHT - 1;
    GrRectFill(psContext, &part);

    // Right leg
    part.i16XMin = x + FROG_WIDTH - 4;
    part.i16XMax = x + FROG_WIDTH - 1;
    GrRectFill(psContext, &part);

    // Update old frog position
    oldFrogX = x;
    oldFrogY = y;
}

//------------------Erase Bus/Hazards------------------//
static void prvEraseBus(tContext *psContext, int x, int y)
{
    tRectangle eraseRect;

    eraseRect.i16XMin = x;
    eraseRect.i16YMin = y;
    eraseRect.i16XMax = x + HAZARD_WIDTH - 1;
    eraseRect.i16YMax = y + HAZARD_HEIGHT - 1;

    GrContextForegroundSet(psContext, ClrBlack);
    GrRectFill(psContext, &eraseRect);
}

//------------------Draw Static Playfield------------------//
static void prvDrawPlayfield(tContext *psContext)
{
    tRectangle rect;
    tRectangle startLine;
    tRectangle finishLine;

    // Clear whole screen
    rect.i16XMin = 0;
    rect.i16YMin = 0;
    rect.i16XMax = 319;
    rect.i16YMax = 239;

    GrContextForegroundSet(psContext, ClrBlack);
    GrRectFill(psContext, &rect);

    // Finish line
    finishLine.i16XMin = 0;
    finishLine.i16YMin = FINISH_LINE_Y;
    finishLine.i16XMax = 319;
    finishLine.i16YMax = FINISH_LINE_Y + LINE_HEIGHT;
    GrContextForegroundSet(psContext, ClrGreen);
    GrRectFill(psContext, &finishLine);

    // Start line
    startLine.i16XMin = 0;
    startLine.i16YMin = START_LINE_Y;
    startLine.i16XMax = 319;
    startLine.i16YMax = START_LINE_Y + LINE_HEIGHT;
    GrContextForegroundSet(psContext, ClrYellow);
    GrRectFill(psContext, &startLine);
}
/*-----------------------------------------------------------*/
