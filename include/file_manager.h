#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#define SAVE_FILE "tasks.txt"

/* Save all tasks to SAVE_FILE. Returns 1 on success, 0 on failure. */
int fm_save(void);

/* Load tasks from SAVE_FILE into the task manager. Returns number of tasks loaded. */
int fm_load(void);

#endif /* FILE_MANAGER_H */