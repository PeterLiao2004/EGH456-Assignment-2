// debug_log.c
#include <stdarg.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include "utils/uartstdio.h"
#include "debug_log.h"

extern SemaphoreHandle_t xUARTMutex;

void DebugPrintf(const char *format, ...)
{
#if DEBUG_UART_ENABLED
    va_list args;

    if (xUARTMutex != NULL)
    {
        // Avoid blocking forever if the UART is busy. If it can't get the mutex within a short time, drop this message.
        if (xSemaphoreTake(xUARTMutex, pdMS_TO_TICKS(5)) != pdTRUE)
        {
            return;  // UART busy, drop this message
        }
    }

    va_start(args, format);
    UARTvprintf(format, args);
    va_end(args);

    if (xUARTMutex != NULL)
    {
        xSemaphoreGive(xUARTMutex);
    }
#else
    (void)format;
#endif
}