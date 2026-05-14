#ifndef CONTROL_TASKS_H
#define CONTROL_TASKS_H

void vCreateControlTasks(void);
static void prvStateManagerTask(void *pvParameters);
static void prvSafetyMonitorTask(void *pvParameters);
#endif /* CONTROL_TASKS_H */
