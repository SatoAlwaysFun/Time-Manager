#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

#include "task.h"

#define MAX_TASKS 256

/* In-memory task list */
extern Task   tasks[MAX_TASKS];
extern int    task_count;

void  tm_init(void);
int   tm_add(const char *title, const char *desc, Priority priority, time_t start_time);
int   tm_delete(int id);
int   tm_toggle_done(int id);
Task *tm_find(int id);

#endif /* TASK_MANAGER_H */
