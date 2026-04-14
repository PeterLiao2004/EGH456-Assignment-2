# Drivers Module

This folder contains low-level driver code and third-party or board-support code used by the project.

## Responsibilities
- Hardware-specific driver implementations
- Vendor or library driver code
- Display, touch, motor driver, and I2C support layers
- Board-level peripheral wrappers

## Typical Subfolders
- `display_driver/`
- `touch/`
- `motor_driver/`
- `i2c/`

## Notes
Project-specific logic should not be placed here unless it is directly related to low-level hardware access.

## Owned By
Shared as needed