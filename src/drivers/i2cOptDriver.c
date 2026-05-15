/**************************************************************************************************
*  Filename:       i2cOptDriver.c
*  Description:    Interrupt-driven I2C driver for use with opt3001.c and the TI OPT3001 sensor
*************************************************************************************************/

// ----------------------- Includes -----------------------
#include "i2cOptDriver.h"
#include <stdbool.h>
#include <stdint.h>

#include "inc/hw_memmap.h"
#include "driverlib/i2c.h"
#include "driverlib/interrupt.h"

// FreeRTOS
#include "FreeRTOS.h"
#include "semphr.h"
#include "inc/hw_ints.h"

// ----------------------- Types -----------------------
typedef enum
{
    I2C_IDLE = 0,

    // Write transaction states
    I2C_WRITE_WAIT_REG,
    I2C_WRITE_WAIT_DATA0,
    I2C_WRITE_WAIT_DATA1,

    // Read transaction states
    I2C_READ_WAIT_REG,
    I2C_READ_WAIT_BYTE0,
    I2C_READ_WAIT_BYTE1
} I2CState_t;

// ----------------------- Static Globals -----------------------
// static volatile I2CState_t g_i2cState = I2C_IDLE;
// static volatile uint8_t g_i2cAddr = 0;
// static volatile uint8_t g_i2cReg = 0;
// static volatile uint8_t *g_i2cData = 0;
// static volatile bool g_i2cSuccess = false;

// static SemaphoreHandle_t g_xI2CDoneSemaphore = NULL;

typedef struct {
    I2CState_t state;
    uint8_t addr;
    uint8_t reg;
    uint8_t *data;
    bool success;
    SemaphoreHandle_t doneSemaphore;
    SemaphoreHandle_t mutex;
} I2CTransaction_t;

static volatile I2CTransaction_t I2CTransaction = {
    .state = I2C_IDLE,
    .addr = 0,
    .reg = 0,
    .data = NULL,
    .success = false,
    .doneSemaphore = NULL,
    .mutex = NULL
};

// ----------------------- Public Init -----------------------
void i2cOptDriverInit(void)
{
    if (I2CTransaction.doneSemaphore == NULL)
    {
        I2CTransaction.doneSemaphore = xSemaphoreCreateBinary();
    }

    if (I2CTransaction.mutex == NULL)
    {
        I2CTransaction.mutex = xSemaphoreCreateMutex();
    }

    I2CMasterIntEnable(I2C2_BASE);
    IntEnable(INT_I2C2);
}

// ----------------------- Write -----------------------
/*
 * Sets slave address to ui8Addr
 * Puts ui8Reg followed by two data bytes in *data and transfers over I2C
 */
bool writeI2C(uint8_t ui8Addr, uint8_t ui8Reg, uint8_t *data)
{
    // TAKE MUTEX TO LOCK I2C TRANSACTION STRUCT
    xSemaphoreTake(I2CTransaction.mutex, portMAX_DELAY);

    if ((data == NULL) || (I2CTransaction.doneSemaphore == NULL) || (I2CTransaction.state != I2C_IDLE))
    {
        // Release the mutex before returning
        xSemaphoreGive(I2CTransaction.mutex);
        return false;
    }

    // Clear any old semaphore give
    xSemaphoreTake(I2CTransaction.doneSemaphore, 0);

    I2CTransaction.addr = ui8Addr;
    I2CTransaction.reg = ui8Reg;
    I2CTransaction.data = data;
    I2CTransaction.success = false;
    I2CTransaction.state = I2C_WRITE_WAIT_REG;

    // Start transaction: send register address first
    I2CMasterSlaveAddrSet(I2C2_BASE, ui8Addr, false);
    I2CMasterDataPut(I2C2_BASE, ui8Reg);
    I2CMasterControl(I2C2_BASE, I2C_MASTER_CMD_BURST_SEND_START);

    // Wait for ISR to finish transaction
    if (xSemaphoreTake(I2CTransaction.doneSemaphore, pdMS_TO_TICKS(50)) != pdPASS)
    {
        I2CTransaction.state = I2C_IDLE;
        I2CTransaction.success = false;

        // Release the mutex before returning
        xSemaphoreGive(I2CTransaction.mutex);

        return false;   // timeout
    }

    // RELEASE MUTEX
    xSemaphoreGive(I2CTransaction.mutex);
    return I2CTransaction.success;
}

// ----------------------- Read -----------------------
/*
 * Sets slave address to ui8Addr
 * Writes ui8Reg over I2C to specify register being read from
 * Reads two bytes from I2C slave and stores them into *data
 */
bool readI2C(uint8_t ui8Addr, uint8_t ui8Reg, uint8_t *data)
{
    // TAKE MUTEX TO LOCK I2C TRANSACTION STRUCT
    xSemaphoreTake(I2CTransaction.mutex, portMAX_DELAY);

    if ((data == NULL) || (I2CTransaction.doneSemaphore == NULL) || (I2CTransaction.state != I2C_IDLE))
    {
        // Release the mutex before returning
        xSemaphoreGive(I2CTransaction.mutex);
        return false;
    }

    // Clear any old semaphore give
    xSemaphoreTake(I2CTransaction.doneSemaphore, 0);

    I2CTransaction.addr = ui8Addr;
    I2CTransaction.reg = ui8Reg;
    I2CTransaction.data = data;
    I2CTransaction.success = false;
    I2CTransaction.state = I2C_READ_WAIT_REG;

    // Start transaction: write register address first
    I2CMasterSlaveAddrSet(I2C2_BASE, ui8Addr, false);
    I2CMasterDataPut(I2C2_BASE, ui8Reg);
    I2CMasterControl(I2C2_BASE, I2C_MASTER_CMD_SINGLE_SEND);

    // Wait for ISR to finish transaction
    if (xSemaphoreTake(I2CTransaction.doneSemaphore, pdMS_TO_TICKS(50)) != pdPASS)
    {
        I2CTransaction.state = I2C_IDLE;
        I2CTransaction.success = false;

        // Release the mutex before returning
        xSemaphoreGive(I2CTransaction.mutex);

        return false;   // timeout
    }

    // RELEASE MUTEX
    xSemaphoreGive(I2CTransaction.mutex);
    return I2CTransaction.success;
}

// ----------------------- I2C ISR -----------------------
void I2C2IntHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    I2CMasterIntClear(I2C2_BASE);

    // Error path
    if (I2CMasterErr(I2C2_BASE) != I2C_MASTER_ERR_NONE)
    {
        I2CTransaction.success = false;
        I2CTransaction.state = I2C_IDLE;

        if (I2CTransaction.doneSemaphore != NULL)
        {
            xSemaphoreGiveFromISR(I2CTransaction.doneSemaphore, &xHigherPriorityTaskWoken);
        }

        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        return;
    }

    switch (I2CTransaction.state)
    {
        // ---------- Write transaction ----------
        case I2C_WRITE_WAIT_REG:
            I2CMasterDataPut(I2C2_BASE, I2CTransaction.data[0]);
            I2CMasterControl(I2C2_BASE, I2C_MASTER_CMD_BURST_SEND_CONT);
            I2CTransaction.state = I2C_WRITE_WAIT_DATA0;
            break;

        case I2C_WRITE_WAIT_DATA0:
            I2CMasterDataPut(I2C2_BASE, I2CTransaction.data[1]);
            I2CMasterControl(I2C2_BASE, I2C_MASTER_CMD_BURST_SEND_FINISH);
            I2CTransaction.state = I2C_WRITE_WAIT_DATA1;
            break;

        case I2C_WRITE_WAIT_DATA1:
            I2CTransaction.success = true;
            I2CTransaction.state = I2C_IDLE;

            if (I2CTransaction.doneSemaphore != NULL)
            {
                xSemaphoreGiveFromISR(I2CTransaction.doneSemaphore, &xHigherPriorityTaskWoken);
            }
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
            break;

        // ---------- Read transaction ----------
        case I2C_READ_WAIT_REG:
            I2CMasterSlaveAddrSet(I2C2_BASE, I2CTransaction.addr, true);
            I2CMasterControl(I2C2_BASE, I2C_MASTER_CMD_BURST_RECEIVE_START);
            I2CTransaction.state = I2C_READ_WAIT_BYTE0;
            break;

        case I2C_READ_WAIT_BYTE0:
            I2CTransaction.data[0] = I2CMasterDataGet(I2C2_BASE);
            I2CMasterControl(I2C2_BASE, I2C_MASTER_CMD_BURST_RECEIVE_FINISH);
            I2CTransaction.state = I2C_READ_WAIT_BYTE1;
            break;

        case I2C_READ_WAIT_BYTE1:
            I2CTransaction.data[1] = I2CMasterDataGet(I2C2_BASE);
            I2CTransaction.success = true;
            I2CTransaction.state = I2C_IDLE;

            if (I2CTransaction.doneSemaphore != NULL)
            {
                xSemaphoreGiveFromISR(I2CTransaction.doneSemaphore, &xHigherPriorityTaskWoken);
            }
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
            break;

        default:
            I2CTransaction.success = false;
            I2CTransaction.state = I2C_IDLE;

            if (I2CTransaction.doneSemaphore != NULL)
            {
                xSemaphoreGiveFromISR(I2CTransaction.doneSemaphore, &xHigherPriorityTaskWoken);
            }
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
            break;
    }
}