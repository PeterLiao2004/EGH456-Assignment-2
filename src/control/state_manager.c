#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"
#include "semphr.h"

// Event bits
#define EVENT_START         (1 << 0)
#define EVENT_E_STOP        (1 << 1)
#define EVENT_STOP          (1 << 2)
#define EVENT_FAULT_ACK     (1 << 3)
#define EVENT_SPEED_VALID   (1 << 4)
#define EVENT_MOTOR_STOPPED (1 << 5)