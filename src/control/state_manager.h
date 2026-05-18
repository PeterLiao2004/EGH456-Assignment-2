#ifndef STATE_MANAGER_H
#define STATE_MANAGER_H
#include "system_state.h"

void vCreateStateManagerTasks(void);

void StateManager_Init(void); 

void StateManager_TriggerStart(void);
void StateManager_TriggerEStop(void);
void StateManager_TriggerFaultAck(void);
SystemState_t StateManager_GetState(void);
const char *StateManager_GetStateString(void);
#endif /* STATE_MANAGER_H */
