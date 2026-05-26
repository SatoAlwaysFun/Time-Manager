#include "file_manager.h"
#include "task_manager.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*
 * File format (one task per line):
 *   id|done|priority|start_time|deadline|title|description\n
 */

int fm_save(void) {
    FILE *f = fopen(SAVE_FILE, "w");
    if (!f) return 0;
    for (int i = 0; i < task_count; i++) {
        Task *t = &tasks[i];
        fprintf(f, "%d|%d|%d|%lld|%lld|%s|%s\n",
                t->id, t->done, (int)t->priority,
                (long long)t->start_time,
                (long long)t->deadline,
                t->title, t->description);
    }
    fclose(f);
    return 1;
}

int fm_load(void) {
    FILE *f = fopen(SAVE_FILE, "r");
    if (!f) return 0;

    tm_init();
    char line[700];
    int  loaded = 0;

    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';

        char *tok;
        int id, done, prio;
        long long st = 0, dl = 0;
        char title[MAX_TITLE_LEN];
        char desc[MAX_DESC_LEN];
        title[0] = '\0';
        desc[0]  = '\0';

        tok = strtok(line, "|"); if (!tok) continue;
        id  = atoi(tok);

        tok  = strtok(NULL, "|"); if (!tok) continue;
        done = atoi(tok);

        tok  = strtok(NULL, "|"); if (!tok) continue;
        prio = atoi(tok);

        tok = strtok(NULL, "|"); if (!tok) continue;
        st  = atoll(tok);

        /* deadline field — may be absent in old files */
        tok = strtok(NULL, "|"); if (!tok) goto store;
        /* If this token looks like a timestamp (all digits), treat as deadline */
        {
            int is_num = 1;
            for (int ci = 0; tok[ci]; ci++) {
                if (tok[ci] < '0' || tok[ci] > '9') { is_num = 0; break; }
            }
            if (is_num) {
                dl = atoll(tok);
                tok = strtok(NULL, "|");   /* advance to title */
            }
            /* else tok is already the title (old format without deadline) */
        }
        if (tok) strncpy(title, tok, MAX_TITLE_LEN - 1);

        tok = strtok(NULL, "|");
        if (tok) strncpy(desc, tok, MAX_DESC_LEN - 1);

        store:
        if (task_count < MAX_TASKS) {
            Task *t = &tasks[task_count++];
            t->id         = id;
            t->done       = done;
            t->priority   = (Priority)prio;
            t->start_time = (time_t)st;
            t->deadline   = (time_t)dl;
            strncpy(t->title,       title, MAX_TITLE_LEN - 1);
            strncpy(t->description, desc,  MAX_DESC_LEN  - 1);
            t->title[MAX_TITLE_LEN - 1]      = '\0';
            t->description[MAX_DESC_LEN - 1] = '\0';
            loaded++;
        }
    }
    fclose(f);
    return loaded;
}