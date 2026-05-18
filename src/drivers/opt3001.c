/* opt3001.c */

/* ------------------------------------------------------------------------------------------------
 *                                          Includes
 * ------------------------------------------------------------------------------------------------
 */
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include "i2c_driver.h"
#include "opt3001.h"
#include "utils/uartstdio.h"

/* ------------------------------------------------------------------------------------------------
 *                                           Constants
 * ------------------------------------------------------------------------------------------------
 */

/* Slave address */
#define OPT3001_I2C_ADDRESS             0x47

/* Register addresses */
#define REG_RESULT                      0x00
#define REG_CONFIGURATION               0x01
#define REG_LOW_LIMIT                   0x02
#define REG_HIGH_LIMIT                  0x03

#define REG_MANUFACTURER_ID             0x7E
#define REG_DEVICE_ID                   0x7F

/* Register values */
#define MANUFACTURER_ID                 0x5449  // ID check = TI
#define DEVICE_ID                       0x3001  // Device ID = 3001

#define CONFIG_RESET                    0xC810                   
#define CONFIG_TEST                     0xCC10

#define CONFIG_ENABLE                   0x10C4 // stored little-endian: bytes sent on wire are [0xC4, 0x10], so sensor register receives 0xC410 (continuous conversion, 100ms)
#define CONFIG_DISABLE                  0x10C0 // stored little-endian: bytes sent on wire are [0xC0, 0x10], so sensor register receives 0xC010 (shutdown)

/* Bit values */
#define DATA_RDY_BIT                    0x0080  // Data ready

/* Register length */
#define REGISTER_LENGTH                 2

/* Sensor data size */
#define DATA_LENGTH                     2

/* ------------------------------------------------------------------------------------------------
 *                                           Local Functions
 * ------------------------------------------------------------------------------------------------
 */


/* ------------------------------------------------------------------------------------------------
 *                                           Local Variables
 * ------------------------------------------------------------------------------------------------
 */

/* ------------------------------------------------------------------------------------------------
 *                                           Public functions
 * -------------------------------------------------------------------------------------------------
 */


/**************************************************************************************************
 * @fn          sensorOpt3001Init
 *
 * @brief       Initialize the temperature sensor by reseting the sensor
 *
 * @return      none
 **************************************************************************************************/
bool sensorOpt3001Init(void)
{
	//Disable the sensor
	if (!sensorOpt3001Enable(false))
	{
		return false;
	}

	//Enable the sensor
	return sensorOpt3001Enable(true);
}


/**************************************************************************************************
 * @fn          sensorOpt3001Enable
 *
 * @brief       Turn the sensor on or off
 *
 * @return      none
 **************************************************************************************************/
bool sensorOpt3001Enable(bool enable)
{
	uint16_t val;

	if (enable)
	{
		val = CONFIG_ENABLE;
	}
	else
	{
		val = CONFIG_DISABLE;
	}

	return I2C_write(OPT3001_I2C_ADDRESS, REG_CONFIGURATION, (uint8_t*)&val, REGISTER_LENGTH);
}


/**************************************************************************************************
 * @fn          sensorOpt3001Read
 *
 * @brief       Read the result register
 *
 * @param       Buffer to store data in
 *
 * @return      TRUE if valid data
 **************************************************************************************************/
bool sensorOpt3001Read(uint16_t *rawData)
{
	bool data_ready;
	uint16_t val;

	// Read configuration register to check if a conversion result is ready
	if (!I2C_read(OPT3001_I2C_ADDRESS, REG_CONFIGURATION, (uint8_t *)&val, REGISTER_LENGTH))
	{
		return false;
	}

	// Sensor sends MSByte first; swap bytes after storing into little-endian val
	val = (val >> 8) | (val << 8);

	// DATA_RDY is bit 7 of the LSB of the OPT3001 configuration register
	data_ready = (val & DATA_RDY_BIT) != 0;

	if (!data_ready)
	{
		return false;
	}

	// Conversion complete — read the result register
	if (!I2C_read(OPT3001_I2C_ADDRESS, REG_RESULT, (uint8_t *)&val, REGISTER_LENGTH))
	{
		return false;
	}

	// Swap bytes (sensor sends MSByte first, MCU stores little-endian)
	*rawData = (val >> 8) | (val << 8);

	return true;
}


/**************************************************************************************************
 * @fn          sensorOpt3001Test
 *
 * @brief       Run a sensor self-test
 *
 * @return      TRUE if passed, FALSE if failed
 **************************************************************************************************/
bool sensorOpt3001Test(void)
{
	uint16_t val;

	// Check manufacturer ID
	if (!I2C_read(OPT3001_I2C_ADDRESS, REG_MANUFACTURER_ID, (uint8_t *)&val, REGISTER_LENGTH))
	{
		return false;
	}

	// Swap bytes (sensor sends MSByte first, MCU stores little-endian)
	val = (val >> 8) | (val << 8);

	if (val != MANUFACTURER_ID)
	{
		return false;
	}

	UARTprintf("Manufacturer ID Correct: %c%c\n", (val >> 8) & 0x00FF, val & 0x00FF);

	// Check device ID
	if (!I2C_read(OPT3001_I2C_ADDRESS, REG_DEVICE_ID, (uint8_t *)&val, REGISTER_LENGTH))
	{
		return false;
	}

	// Swap bytes (sensor sends MSByte first, MCU stores little-endian)
	val = (val >> 8) | (val << 8);

	if (val != DEVICE_ID)
	{
		return false;
	}

	UARTprintf("Device ID Correct: %02x%02x\n", (val >> 8) & 0x00FF, val & 0x00FF);

	return true;
}

/**************************************************************************************************
 * @fn          sensorOpt3001Convert
 *
 * @brief       Convert raw data to object and ambience temperature
 *
 * @param       rawData - raw data from sensor
 *
 * @param       convertedLux - converted value (lux)
 *
 * @return      none
 **************************************************************************************************/
void sensorOpt3001Convert(uint16_t rawData, float *convertedLux)
{
	uint16_t e, m;

	m = rawData & 0x0FFF;
	e = (rawData & 0xF000) >> 12;

	*convertedLux = m * (0.01 * exp2(e));
}
