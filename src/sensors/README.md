# Sensor Module

This folder contains code for the external sensors used in the project.

## Responsibilities
- I2C sensor communication
- Sensor initialisation
- Reading temperature and distance values
- Validating and formatting sensor data
- Providing sensor APIs to the rest of the system

## Typical Files
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

## Owned By
Person 4 — Sensor/Peripheral Lead