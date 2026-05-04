# Control Module

High-level control subsystem.

- Owns the control state machine, speed targets, and fault handling
- Contains the control task scaffold in `control_tasks.c`
- Current task behavior: prints status to UART for bring-up

Keep low-level motor hardware code out of this folder.
