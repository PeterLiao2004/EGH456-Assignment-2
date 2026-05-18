/* SHT31 Sensor Driver */

#ifndef SHT31_H_
#define SHT31_H_

#include <stdint.h>
#include <stdbool.h>

bool sensorSHT31Init(void);

bool sensorSHT31Read(float *temperature, float *humidity);

bool sensorSHT31Test(void);

#endif