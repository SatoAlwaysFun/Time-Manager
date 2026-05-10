#ifndef TASK_H
#define TASK_H

#include <time.h>

#define MAX_TITLE_LEN 128
#define MAX_DESC_LEN  512

typedef enum {
    PRIORITY_LOW = 0,
    PRIORITY_MEDIUM,
    PRIORITY_HIGH
} Priority;

typedef struct {
    int      id;
    char     title[MAX_TITLE_LEN];
    char     description[MAX_DESC_LEN];
    Priority priority;
    int      done;          /* 0 = pending, 1 = done */
    time_t   start_time;    /* 0 = no time set */
} Task;

const char *priority_to_string(Priority p);

#endif /* TASK_H */
