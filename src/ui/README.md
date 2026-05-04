# UI Module

User interface subsystem.

- Owns display output and touch input
- Contains the UI task scaffold in `ui_tasks.c`
- Current task behavior: prints status to UART for bring-up

Keep UI logic separate from motor control and sensor drivers.
