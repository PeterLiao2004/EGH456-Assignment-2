# Motor Module

This folder contains the low-level motor hardware and commutation code.

## Responsibilities
- Hall sensor reading
- Hall interrupt handling
- BLDC commutation logic
- Hall-state to phase mapping
- Motor speed feedback calculation
- Low-level motor hardware interaction

## Typical Files
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

## Owned By
Person 1 — Motor Hardware Lead