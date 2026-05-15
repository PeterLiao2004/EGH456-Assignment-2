/* i2c_driver.h */



#ifndef _I2C_DRIVER_H_
#define _I2C_DRIVER_H_



// ----------------------- Includes -----------------------
#include <stdbool.h>
#include <stdint.h>



// ----------------------- Exported prototypes -----------------------
void I2CDriverInit(void);
bool I2C_write(uint8_t ui8Addr, uint8_t ui8Reg, uint8_t *Data, uint32_t length);
bool I2C_read(uint8_t ui8Addr, uint8_t ui8Reg, uint8_t *Data, uint32_t length);
// extern uint32_t i2cGetLastTime_us(void);



#endif /* _I2C_DRIVER_H_ */
