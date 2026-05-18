#ifndef UI_TASKS_H
#define UI_TASKS_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "queue.h"

typedef struct
{
    uint32_t sequenceNum;
    TickType_t timestamp;

    float luxRaw;
    float luxFiltered;

    float temperatureC;
    float humidityRH;

    float accelerationFiltered;

    float distanceCm;
} Opt3001Data_t;

extern QueueHandle_t xOpt3001Queue;

void vCreateUiTasks(void);

#endif /* UI_TASKS_H */
