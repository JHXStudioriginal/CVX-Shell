// Copyright (c) 2025-2026 JHXStudioriginal
// This file is part of the GNU General Public License v3
// All original author information and file headers must be preserved.
// For full license text, see: [https://github.com/JHXStudioriginal/CVX-Shell/blob/main/LICENSE]

#ifndef JOBS_H
#define JOBS_H

#include <sys/types.h>

typedef enum {
    JOB_RUNNING,
    JOB_STOPPED
} job_state_t;

void jobs_add(pid_t pgid, const char *cmd, job_state_t state);
void jobs_remove(pid_t pgid);
void jobs_list(void);
pid_t jobs_get_pgid(int id);
int jobs_last_id(void);
pid_t jobs_last_pgid(void);
void jobs_cleanup(void);
void jobs_set_state(pid_t pgid, job_state_t state);

#endif
