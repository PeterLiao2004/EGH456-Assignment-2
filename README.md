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

## Team Allocation

This is a suitable group split because each person gets a clear subsystem folder,
task scaffold, and assignment responsibility.

| Area | Folder | Main responsibilities | Integration outputs |
| --- | --- | --- | --- |
| Motor | `src/motor/` | Hall sensor interrupts, BLDC commutation, motor phase updates, motor speed/current feedback | Motor status, measured speed, fault flags |
| Control | `src/control/` | Motor state machine, start/stop behaviour, speed target ramping, e-stop and fault handling | Control mode, requested motor speed/duty, system fault state |
| Sensors | `src/sensors/` | I2C sensor setup, temperature/humidity sensor, time-of-flight distance sensor, sensor data formatting | Latest sensor readings, sensor validity/fault flags |
| UI | `src/ui/` | LCD display, touch input, user commands, status screens | User commands, displayed state, requested speed/setpoints |

## Integration Boundaries

- `src/app_tasks.c` should only create and wire tasks. Subsystem logic should stay
  inside the relevant module folder.
- Motor owns low-level hardware timing for hall transitions and commutation.
  Control should request behaviour, not directly drive motor phases.
- Control owns the assignment state machine and decides when start, stop, e-stop,
  and speed-ramp transitions occur.
- Sensors owns raw I2C device access and should expose cleaned readings to the
  rest of the application.
- UI owns LCD/touch behaviour and should communicate user requests through shared
  state, queues, or clearly named interface functions.
- The touchscreen uses ADC0 and Timer1 in the reference material, so avoid
  reusing those resources for current sensing or other module timers without
  checking for conflicts.

## Notes

- UART is used for simple task bring-up and debug output.
- Task creation can fail if FreeRTOS heap is exhausted.
- The current code is a template, not the final integrated application.
