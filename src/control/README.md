# Control Module

This folder contains the high-level motor control logic for the project.

## Current Scaffold

This folder currently includes:

- `control_tasks.c`
- `control_tasks.h`

`vControlTaskCreate()` is the public task creation entry point for the control subsystem. The placeholder control task in `control_tasks.c` should be replaced or extended with the real control logic.

## Responsibilities

- Motor state machine
- Start/stop behaviour
- E-stop braking logic
- Fault handling and fault latching
- Speed target generation
- Acceleration and deceleration profiles

## Typical Files

- `control_tasks.c`
- `control_tasks.h`
- `motor_control.c`
- `motor_control.h`
- `speed_profile.c`

## Inputs

- Hall/speed feedback from the motor module
- User commands from the UI
- Optional sensor-based fault conditions

## Outputs

- Target motor speed
- Motor control requests
- Current motor state

## Notes

This folder should contain high-level control behaviour, not low-level motor commutation code.

## Owned By

Person 2 - Control Systems Lead
