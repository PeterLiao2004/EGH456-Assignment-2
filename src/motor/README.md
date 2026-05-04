# Motor Module

Low-level motor subsystem.

- Owns hall sensing, commutation, and motor feedback
- Contains the motor task scaffold in `motor_tasks.c`
- Current task behavior: prints status to UART for bring-up

Keep high-level control logic out of this folder.
