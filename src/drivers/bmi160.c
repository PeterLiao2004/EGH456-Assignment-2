#include "bmi160.h"
#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"
#include "driverlib/uart.h"
#include "utils/uartstdio.h"
#include "drivers/i2c_driver.h"

#define BMI160_ADDR        0x69

#define BMI160_CHIP_ID     0x00
#define BMI160_CHIP_ID_VAL 0xD1

#define BMI160_ACC_X_L     0x12
#define BMI160_ACC_X_H     0x13
#define BMI160_ACC_Y_L     0x14
#define BMI160_ACC_Y_H     0x15
#define BMI160_ACC_Z_L     0x16
#define BMI160_ACC_Z_H     0x17

#define BMI160_CMD_REG     0x7E
#define BMI160_ACC_CONF    0x40
#define BMI160_ACC_RANGE   0x41

static int16_t combine(uint8_t lsb, uint8_t msb)
{
    return (int16_t)((msb << 8) | lsb);
}

bool sensorBMI160Init(void)
{
    uint8_t id = 0;

    I2C_read(BMI160_ADDR, BMI160_CHIP_ID, &id, 1);

    if (id != BMI160_CHIP_ID_VAL)
    {
        UARTprintf("BMI160 wrong ID: %d\n", id);
        return false;
    }

    /* Soft reset */
    uint8_t cmd = 0xB6;
    I2C_write(BMI160_ADDR, BMI160_CMD_REG, &cmd, 1);

    /* Delay for reset */
    vTaskDelay(pdMS_TO_TICKS(100));

    /* Enable accel normal mode */
    cmd = 0x11;
    I2C_write(BMI160_ADDR, BMI160_CMD_REG, &cmd, 1);

    vTaskDelay(pdMS_TO_TICKS(50));

    return true;
}

bool sensorBMI160Test(void)
{
    uint8_t id = 0;

    bool ok = I2C_read(BMI160_ADDR, BMI160_CHIP_ID, &id, 1);

    return (ok && id == BMI160_CHIP_ID_VAL);
}

bool sensorBMI160Read(float *ax, float *ay, float *az)
{
    uint8_t raw[6];

    if (!I2C_read(BMI160_ADDR, BMI160_ACC_X_L, raw, 6))
    {
        return false;
    }

    int16_t x = combine(raw[0], raw[1]);
    int16_t y = combine(raw[2], raw[3]);
    int16_t z = combine(raw[4], raw[5]);

    /* Scale factor (depends on range; assume ±2g default) */
    const float scale = 16384.0f;

    *ax = x / scale;
    *ay = y / scale;
    *az = z / scale;

    return true;
}