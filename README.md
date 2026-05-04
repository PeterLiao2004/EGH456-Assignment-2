# EGH456 Assignment 2

Embedded EV motor control project for the Tiva C LaunchPad.

## Current State

The codebase is currently a FreeRTOS scaffold for team development.

- `main.c` sets up hardware, UART, semaphores, and the scheduler.
- `app_tasks.c` wires subsystem tasks into the application.
- `src/motor/`, `src/control/`, `src/sensors/`, and `src/ui/` are owned by different team members.
- Current subsystem tasks are simple UART-printing placeholders for bring-up and integration testing.

## Folder Layout

- `src/motor/` low-level motor code and motor task scaffold
- `src/control/` control logic and control task scaffold
- `src/sensors/` sensor code and sensor task scaffold
- `src/ui/` UI code and UI task scaffold
- `src/drivers/` board and peripheral drivers
- `src/rtos/` FreeRTOS source
- `src/main.c` application entry point
- `src/app_tasks.c` task wiring and ISR helpers

## Team Split

- Motor: hall sensors, commutation, motor feedback
- Control: state machine, speed control, fault handling
- Sensors: I2C sensors and sensor data
- UI: display and touch interaction

## Notes

- UART is used for simple task bring-up and debug output.
- Task creation can fail if FreeRTOS heap is exhausted.
- The current code is a template, not the final integrated application.
