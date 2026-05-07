#ifndef TASK_H
#define TASK_H

#define MAX_TITLE_LEN 128
#define MAX_DESC_LEN  256

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
    int      done;        /* 0 = pending, 1 = done */
} Task;

const char *priority_to_string(Priority p);

#endif /* TASK_H */