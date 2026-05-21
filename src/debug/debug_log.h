// debug_log.h
#ifndef DEBUG_LOG_H
#define DEBUG_LOG_H

#include <stdbool.h>
#define DBG_MOTOR           "[MOTOR] "
#define DBG_STATE           "[STATE] "
#define DBG_SENSOR          "[SENSOR] "
#define DBG_UI              "[UI] "
#define DBG_SAFETY          "[SAFETY] "
#define DBG_INIT            "[INIT] "
#define DBG_HARDWARE        "[HARDWARE] "
#define DBG_ERROR           "[ERROR] "

#define DEBUG_UART_ENABLED  1 // Set to 1 to enable debug UART output, 0 to disable.

void DebugPrintf(const char *format, ...);

#endif
