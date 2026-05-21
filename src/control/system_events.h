/**
 * @file system_events.h
 * @brief FreeRTOS event bits consumed by the control state manager.
 */

#ifndef SYSTEM_EVENTS_H
#define SYSTEM_EVENTS_H

#include <stdint.h>

/** @brief Start request event bit. */
#define EVENT_START         (1 << 0)

/** @brief Emergency-stop request event bit. */
#define EVENT_E_STOP        (1 << 1)

/** @brief Fault acknowledgement request event bit. */
#define EVENT_FAULT_ACK     (1 << 2)

#endif /* SYSTEM_EVENTS_H */
