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
    time_t   deadline;      /* 0 = no deadline set */
} Task;

/* Deadline urgency levels */
typedef enum {
    DEADLINE_NONE    = 0,  /* no deadline */
    DEADLINE_NORMAL  = 1,  /* > 3 days away */
    DEADLINE_WARNING = 2,  /* <= 3 days away */
    DEADLINE_URGENT  = 3,  /* <= 1 day away */
    DEADLINE_OVERDUE = 4   /* past deadline */
} DeadlineStatus;

const char *priority_to_string(Priority p);

#endif /* TASK_H */