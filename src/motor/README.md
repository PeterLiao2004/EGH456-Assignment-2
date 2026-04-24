# Motor Module

This folder contains the low-level motor hardware and commutation code.

## Current Scaffold

This folder currently includes:

- `motor_tasks.c`
- `motor_tasks.h`

`vMotorTaskCreate()` is the public task creation entry point for the motor subsystem. The placeholder motor task in `motor_tasks.c` should be replaced or extended with the real motor task logic.

## Responsibilities

- Hall sensor reading
- Hall interrupt handling
- BLDC commutation logic
- Hall-state to phase mapping
- Motor speed feedback calculation
- Low-level motor hardware interaction

## Typical Files

- `motor_tasks.c`
- `motor_tasks.h`
- `motor_commutation.c`
- `motor_feedback.c`
- `motor_driver_wrapper.c`

## Inputs

- Hall sensor signals
- Motor control requests from the control module

## Outputs

- Updated motor phase outputs
- Measured motor speed
- Hall state feedback

## Notes

This is the main hardware-dependent motor subsystem and should be kept separate from high-level control logic.
Keep motor-specific ISRs, private semaphores, and hardware-facing code in this folder where possible.

## Owned By

Person 1 - Motor Hardware Lead
