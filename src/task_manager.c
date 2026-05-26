#include "task_manager.h"
#include <string.h>
#include <stdio.h>

Task tasks[MAX_TASKS];
int  task_count = 0;

static int next_id = 1;

void tm_init(void) {
    task_count = 0;
    next_id    = 1;
}

int tm_add(const char *title, const char *desc, Priority priority, time_t start_time) {
    if (task_count >= MAX_TASKS) return -1;
    Task *t = &tasks[task_count++];
    t->id   = next_id++;
    strncpy(t->title,       title ? title : "", MAX_TITLE_LEN - 1);
    strncpy(t->description, desc  ? desc  : "", MAX_DESC_LEN  - 1);
    t->title[MAX_TITLE_LEN - 1]      = '\0';
    t->description[MAX_DESC_LEN - 1] = '\0';
    t->priority   = priority;
    t->done       = 0;
    t->start_time = start_time;
    t->deadline   = 0;
    return t->id;
}

int tm_delete(int id) {
    for (int i = 0; i < task_count; i++) {
        if (tasks[i].id == id) {
            for (int j = i; j < task_count - 1; j++)
                tasks[j] = tasks[j + 1];
            task_count--;
            return 1;
        }
    }
    return 0;
}

int tm_toggle_done(int id) {
    Task *t = tm_find(id);
    if (!t) return 0;
    t->done = !t->done;
    return 1;
}

Task *tm_find(int id) {
    for (int i = 0; i < task_count; i++)
        if (tasks[i].id == id) return &tasks[i];
    return NULL;
}