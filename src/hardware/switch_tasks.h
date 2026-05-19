/**
 * @file switch_tasks.h
 * @brief Board switch interrupt and task interface.
 *
 * The switch module owns the physical board pushbuttons and translates switch
 * presses into state-manager events. UI code should not configure or consume
 * the board switch GPIO directly.
 */

#ifndef SWITCH_TASKS_H
#define SWITCH_TASKS_H

/**
 * @brief Create switch semaphores, configure GPIO interrupts, and start tasks.
 *
 * SW1 posts a start request. SW2 posts an E-stop request, or a fault
 * acknowledgement request if the system is already fault-latched.
 */
void vCreateSwitchTasks(void);

/**
 * @brief GPIO Port J interrupt handler for the board switches.
 *
 * This function is referenced by the startup vector table. It performs minimal
 * ISR work: clears the interrupt, debounces the edge, and wakes the relevant
 * switch task.
 */
void xButtonsHandler(void);

#endif /* SWITCH_TASKS_H */
