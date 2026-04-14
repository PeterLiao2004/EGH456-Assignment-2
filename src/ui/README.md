# UI Module

This folder contains the LCD display and touchscreen interface code.

## Responsibilities
- LCD output
- Touchscreen input
- Screen layout and status display
- User commands such as start, stop, and fault acknowledgement
- Display of motor state, speed, and sensor values

## Typical Files
- `ui_display.c`
- `touch_input.c`

## Inputs
- Motor state from the control module
- Speed feedback from the motor module
- Sensor values from the sensor module

## Outputs
- User control requests
- Updated on-screen information

## Notes
This folder should only contain user interface logic, not raw sensor drivers or motor control logic.

## Owned By
Person 3 — User Interface Lead