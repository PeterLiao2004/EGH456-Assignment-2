# RTOS Module

This folder contains FreeRTOS-related code for the project.

## Responsibilities
- Task creation
- Queue and semaphore setup
- Scheduling-related application structure
- Task wrappers for control, UI, and sensors

## Typical Files
- `tasks.c`
- `queues.c`
- `sync.c`

## Notes
Keep subsystem logic inside its own module. This folder should mainly contain RTOS setup and task management code.

## Owned By
Shared as needed