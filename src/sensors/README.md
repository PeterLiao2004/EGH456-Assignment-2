# Sensor Module

This folder contains code for the external sensors used in the project.

## Current Scaffold

This folder currently includes:

- `sensor_tasks.c`
- `sensor_tasks.h`

`vSensorTaskCreate()` is the public task creation entry point for the sensor subsystem. The placeholder sensor task in `sensor_tasks.c` should be replaced or extended with the real sensor polling or sensor-processing logic.

## Responsibilities

- I2C sensor communication
- Sensor initialisation
- Reading temperature and distance values
- Validating and formatting sensor data
- Providing sensor APIs to the rest of the system

## Typical Files

- `sensor_tasks.c`
- `sensor_tasks.h`
- `i2c_sensors.c`
- `sht31.c`
- `vl53l0x.c`

## Inputs

- I2C bus and connected sensor hardware

## Outputs

- Temperature readings
- Distance readings
- Sensor validity/status flags

## Notes

This module should expose clean sensor read functions and keep all raw I2C sensor handling inside this folder.
Sensor-specific ISRs, private queues, and processing should stay in this folder where possible.

## Owned By

Person 4 - Sensor/Peripheral Lead
