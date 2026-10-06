// Copyright (c) 2025-2026 JHXStudioriginal
// This file is part of the GNU General Public License v3
// All original author information and file headers must be preserved.
// For full license text, see: [https://github.com/JHXStudioriginal/CVX-Shell/blob/main/LICENSE]

#include "jobs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#define MAX_JOBS 32

struct job {
    int id;
    pid_t pgid;
    char *cmd;
    job_state_t state;
};

static struct job jobs[MAX_JOBS];
static int job_count = 0;
static int next_id = 1;

void jobs_add(pid_t pgid, const char *cmd, job_state_t state) {
    if (job_count >= MAX_JOBS)
        return;

    jobs[job_count].id = next_id++;
    jobs[job_count].pgid = pgid;
    jobs[job_count].cmd = strdup(cmd);
    jobs[job_count].state = state;
    job_count++;
}

void jobs_list(void) {
    for (int i = 0; i < job_count; i++) {
        const char *state =
            (jobs[i].state == JOB_RUNNING) ? "Running" : "Stopped";

        printf("[%d] %-8s %s\n",
               jobs[i].id,
               state,
               jobs[i].cmd);
    }
}

int jobs_last_id(void) {
    if (job_count == 0)
        return 0;
    return jobs[job_count - 1].id;
}

pid_t jobs_last_pgid(void) {
    if (job_count == 0)
        return 0;
    return jobs[job_count - 1].pgid;
}

void jobs_cleanup(void) {
    for (int i = 0; i < job_count; ) {
        int status;
        pid_t pid = waitpid(-jobs[i].pgid, &status, WNOHANG);

        if (pid == 0) {
            i++;
        } else {
            free(jobs[i].cmd);

            for (int j = i; j < job_count - 1; j++) {
                jobs[j] = jobs[j + 1];
            }
            job_count--;
        }
    }
}

pid_t jobs_get_pgid(int id) {
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].id == id)
            return jobs[i].pgid;
    }
    return -1;
}

void jobs_set_state(pid_t pgid, job_state_t state) {
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].pgid == pgid) {
            jobs[i].state = state;
            return;
        }
    }
}
