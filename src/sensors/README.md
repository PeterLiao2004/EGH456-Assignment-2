# Sensors Module

Sensor subsystem.

- Owns sensor drivers, sensor reads, and sensor data formatting
- Contains the sensor task scaffold in `sensor_tasks.c`
- Current task behavior: prints status to UART for bring-up

Keep raw sensor and I2C handling inside this folder.
