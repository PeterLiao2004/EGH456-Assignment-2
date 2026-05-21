#ifndef BMI160_H
#define BMI160_H

#include <stdint.h>
#include <stdbool.h>

/* Initialize sensor */
bool sensorBMI160Init(void);

/* Basic device check (WHO_AM_I) */
bool sensorBMI160Test(void);

/* Read acceleration in g (or m/s^2 depending on scaling) */
bool sensorBMI160Read(float *ax, float *ay, float *az);

#endif