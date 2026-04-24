# EGH456 Assignment 2

EGH456 group assignment project for a modular embedded motor-control system built on the Tiva C Launchpad and FreeRTOS.

## Project Summary

This repository is set up as a shared team template for an embedded control system that combines:

- low-level BLDC motor commutation
- high-level motor control and fault handling
- LCD and touchscreen UI
- sensor integration
- FreeRTOS task-based coordination

The project is organised so each team member can work mostly within their own subsystem folder before final integration.

## Current File Structure

The current structure of the repository is:

```text
src/
  main.c
  app_tasks.c
  FreeRTOSConfig.h
  startup_gcc.c
  platformio_linker.ld
  macros.ini_initial
  control/
  motor/
  sensors/
  ui/
  drivers/
  driver_lib/
  rtos/
  utils/
```

## What Each Top-Level Source Area Is For

- `src/main.c`
  Application entry point. Sets up hardware, creates shared RTOS primitives, calls `vCreateTasks()`, and starts the scheduler.

- `src/app_tasks.c`
  Application-level task registration file. This is where the team should create subsystem tasks such as motor, control, sensor, and UI tasks.

- `src/FreeRTOSConfig.h`
  FreeRTOS configuration for the project.

- `src/startup_gcc.c`
  Startup code and interrupt vector table for the target board.

- `src/platformio_linker.ld`
  Linker script for the build.

- `src/macros.ini_initial`
  Toolchain support/configuration file included with the project template.

- `src/control/`
  High-level control logic module owned by the control lead.

- `src/motor/`
  Low-level motor and commutation module owned by the motor lead.

- `src/sensors/`
  Sensor and peripheral interface module owned by the sensors lead.

- `src/ui/`
  Display and touchscreen interface module owned by the UI lead.

- `src/drivers/`
  Board-specific and shared hardware driver code already included in the template.

- `src/driver_lib/`
  Vendor/library driver sources for the hardware platform.

- `src/rtos/`
  FreeRTOS kernel source files. These should generally not be edited unless there is a specific RTOS-level reason.

- `src/utils/`
  Shared helper utilities such as UART support and small common helpers.

## Team Module Ownership

- Person 1: `src/motor/`
- Person 2: `src/control/`
- Person 3: `src/ui/`
- Person 4: `src/sensors/`

Shared integration files:

- `src/main.c`
- `src/app_tasks.c`
- shared interfaces the team agrees to add later

## Important Naming Note

The repository currently has both:

- `src/app_tasks.c`
- `src/rtos/tasks.c`

This is valid, but easy to confuse.

- `src/app_tasks.c` is your application task setup file.
- `src/rtos/tasks.c` is part of the FreeRTOS kernel.

This naming keeps the application task setup file clearly separate from the FreeRTOS kernel file.

## Recommended Next Files To Add

Right now the subsystem folders are good placeholders, but they only contain README files. A practical next step is to add starter source and header files for each module, for example:

- `src/motor/motor_tasks.c`
- `src/motor/motor_tasks.h`
- `src/control/control_tasks.c`
- `src/control/control_tasks.h`
- `src/sensors/sensor_tasks.c`
- `src/sensors/sensor_tasks.h`
- `src/ui/ui_tasks.c`
- `src/ui/ui_tasks.h`

Then `vCreateTasks()` in `src/app_tasks.c` can call module-level create functions such as:

```c
vMotorTaskCreate();
vControlTaskCreate();
vSensorTaskCreate();
vUiTaskCreate();
```

## Suggested Working Boundaries

- The motor lead should own hall sensors, commutation, and low-level motor hardware code.
- The control lead should own state logic, ramping, start/stop logic, and fault handling.
- The UI lead should own LCD output and touchscreen interaction.
- The sensors lead should own I2C or other sensor interfaces and sensor data formatting.

## Team Workflow Notes

- Keep shared interfaces agreed and stable before integration.
- Avoid editing `src/rtos/` unless absolutely necessary.
- Prefer keeping each team member inside their subsystem folder most of the time.
- Use `src/main.c` and `src/app_tasks.c` as the integration point rather than putting subsystem logic there.
- Commit small changes often and communicate before editing shared files.
