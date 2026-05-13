#ifndef CONTROL_STATE_H
#define CONTROL_STATE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    CONTROL_STATE_IDLE = 0,
    CONTROL_STATE_READY,
    CONTROL_STATE_STARTING,
    CONTROL_STATE_RUNNING,
    CONTROL_STATE_STOPPING,
    CONTROL_STATE_FAULT,
    CONTROL_STATE_ESTOP
} ControlState_t;

typedef struct
{
    ControlState_t eState;
    uint32_t ui32TargetSpeedRpm;
    uint32_t ui32CommandedSpeedRpm;
    bool bStartRequested;
    bool bStopRequested;
    bool bEstopActive;
    bool bFaultActive;
} ControlContext_t;

void Control_Init(ControlContext_t *pxControl);
void Control_Update(ControlContext_t *pxControl);

#endif /* CONTROL_STATE_H */
