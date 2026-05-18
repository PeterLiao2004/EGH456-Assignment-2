#include "sht31.h"
#include "i2c_driver.h"

#define SHT31_I2C_ADDRESS 0x44

// Commands
#define CMD_MEAS_HIGHREP_STRETCH 0x2C06
#define CMD_READ_STATUS          0xF32D

static void swapBytes(uint16_t *val)
{
    *val = (*val >> 8) | (*val << 8);
}

bool sensorSHT31Init(void)
{
    return true;
}

bool sensorSHT31Test(void)
{
    uint8_t rxData[3];

    uint16_t cmd = CMD_READ_STATUS;
    swapBytes(&cmd);

    if (!I2C_write(SHT31_I2C_ADDRESS, ((uint8_t*)&cmd)[0],
                   &((uint8_t*)&cmd)[1], 1))
    {
        return false;
    }

    if (!I2C_read(SHT31_I2C_ADDRESS, 0x00, rxData, 3))
    {
        return false;
    }

    return true;
}

bool sensorSHT31Read(float *temperature, float *humidity)
{
    uint8_t rxData[6];

    uint16_t cmd = CMD_MEAS_HIGHREP_STRETCH;
    swapBytes(&cmd);

    // Send measurement command
    if (!I2C_write(SHT31_I2C_ADDRESS,
                   ((uint8_t*)&cmd)[0],
                   &((uint8_t*)&cmd)[1],
                   1))
    {
        return false;
    }

    // Small delay for conversion
    // vTaskDelay(pdMS_TO_TICKS(20));

    // Read 6 bytes
    if (!I2C_read(SHT31_I2C_ADDRESS, 0x00, rxData, 6))
    {
        return false;
    }

    uint16_t rawTemp =
        ((uint16_t)rxData[0] << 8) | rxData[1];

    uint16_t rawHum =
        ((uint16_t)rxData[3] << 8) | rxData[4];

    *temperature =
        -45.0f + (175.0f * ((float)rawTemp / 65535.0f));

    *humidity =
        100.0f * ((float)rawHum / 65535.0f);

    return true;
}