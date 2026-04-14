# Control Module

This folder contains the high-level motor control logic for the project.

## Responsibilities
- Motor state machine
- Start/stop behaviour
- E-stop braking logic
- Fault handling and fault latching
- Speed target generation
- Acceleration and deceleration profiles

## Typical Files
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

## Owned By
Person 2 — Control Systems Lead