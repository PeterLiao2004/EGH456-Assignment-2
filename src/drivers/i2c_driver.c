/* i2c_driver.c */

// ----------------------- Includes -----------------------
#include "i2c_driver.h"
#include <stdbool.h>
#include <stdint.h>

#include "inc/hw_memmap.h"
#include "driverlib/i2c.h"
#include "driverlib/interrupt.h"

// FreeRTOS
#include "FreeRTOS.h"
#include "semphr.h"
#include "inc/hw_ints.h"

#include "driverlib/uart.h"
#include "utils/uartstdio.h"

// ----------------------- Types -----------------------
typedef enum
{
    I2C_IDLE = 0,

    // Write transaction states
    I2C_WRITE_REG,
    I2C_WRITE_DATA,

    // Read transaction states
    I2C_READ_REG,
    I2C_READ_BYTE
} I2CState_t;

// Structure for state tracking and mutex
typedef struct
{
    I2CState_t state;
    uint8_t addr;
    uint8_t reg;
    uint8_t *data;
    uint32_t length;
    uint32_t index;
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
    .mutex = NULL};

// ----------------------- Public Init -----------------------
void I2CDriverInit(void)
{
    //  Create semaphore and mutex if they haven't been created yet
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
 * Puts ui8Reg followed by data bytes in *data and transfers over I2C
 */
bool I2C_write(uint8_t ui8Addr, uint8_t ui8Reg, uint8_t *data, uint32_t length)
{
    // TAKE MUTEX TO LOCK I2C TRANSACTION STRUCT
    xSemaphoreTake(I2CTransaction.mutex, portMAX_DELAY);

    if ((data == NULL) || (length == 0) || (I2CTransaction.doneSemaphore == NULL) || (I2CTransaction.state != I2C_IDLE))
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
    I2CTransaction.length = length;
    I2CTransaction.index = 0;
    I2CTransaction.success = false;
    I2CTransaction.state = I2C_WRITE_REG;

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

        return false; // timeout
    }

    // RELEASE MUTEX
    xSemaphoreGive(I2CTransaction.mutex);
    return I2CTransaction.success;
}

// ----------------------- Read -----------------------
/*
 * Sets slave address to ui8Addr
 * Writes ui8Reg over I2C to specify register being read from
 * Reads data bytes from I2C slave and stores them into *data
 */
bool I2C_read(uint8_t ui8Addr, uint8_t ui8Reg, uint8_t *data, uint32_t length)
{
    // TAKE MUTEX TO LOCK I2C TRANSACTION STRUCT
    xSemaphoreTake(I2CTransaction.mutex, portMAX_DELAY);

    if ((data == NULL) || (length == 0) || (I2CTransaction.doneSemaphore == NULL) || (I2CTransaction.state != I2C_IDLE))
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
    I2CTransaction.length = length;
    I2CTransaction.success = false;
    I2CTransaction.state = I2C_READ_REG;

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

        return false; // timeout
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
    case I2C_WRITE_REG:
        I2CMasterDataPut(I2C2_BASE, I2CTransaction.data[0]);
        if (I2CTransaction.length == 1)
        {
            I2CMasterControl(I2C2_BASE, I2C_MASTER_CMD_BURST_SEND_FINISH);
        }
        else
        {
            I2CMasterControl(I2C2_BASE, I2C_MASTER_CMD_BURST_SEND_CONT);
        }
        I2CTransaction.index = 0;
        I2CTransaction.state = I2C_WRITE_DATA;
        break;

    case I2C_WRITE_DATA:
        I2CTransaction.index++;

        if (I2CTransaction.index < I2CTransaction.length)
        {
            I2CMasterDataPut(I2C2_BASE, I2CTransaction.data[I2CTransaction.index]);

            if (I2CTransaction.index == (I2CTransaction.length - 1))
            {
                I2CMasterControl(I2C2_BASE, I2C_MASTER_CMD_BURST_SEND_FINISH);
            }
            else
            {
                I2CMasterControl(I2C2_BASE, I2C_MASTER_CMD_BURST_SEND_CONT);
            }
        }
        else
        {
            I2CTransaction.success = true;
            I2CTransaction.state = I2C_IDLE;

            xSemaphoreGiveFromISR(I2CTransaction.doneSemaphore, &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
        break;

    // ---------- Read transaction ----------
    case I2C_READ_REG:
        I2CMasterSlaveAddrSet(I2C2_BASE, I2CTransaction.addr, true);
        if (I2CTransaction.length == 1)
        {
            I2CMasterControl(I2C2_BASE, I2C_MASTER_CMD_SINGLE_RECEIVE);
        }
        else
        {
            I2CMasterControl(I2C2_BASE, I2C_MASTER_CMD_BURST_RECEIVE_START);
        }
        I2CTransaction.index = 0;
        I2CTransaction.state = I2C_READ_BYTE;
        break;

    case I2C_READ_BYTE:
        I2CTransaction.data[I2CTransaction.index] = I2CMasterDataGet(I2C2_BASE);

        I2CTransaction.index++;

        if (I2CTransaction.index < I2CTransaction.length)
        {
            if (I2CTransaction.index == (I2CTransaction.length - 1))
            {
                I2CMasterControl(I2C2_BASE, I2C_MASTER_CMD_BURST_RECEIVE_FINISH);
            }
            else
            {
                I2CMasterControl(I2C2_BASE, I2C_MASTER_CMD_BURST_RECEIVE_CONT);
            }
        }
        else
        {
            I2CTransaction.success = true;
            I2CTransaction.state = I2C_IDLE;

            xSemaphoreGiveFromISR(I2CTransaction.doneSemaphore, &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
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