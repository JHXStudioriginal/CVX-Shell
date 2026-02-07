// Copyright (c) 2025-2026 JHXStudioriginal
// This file is part of the Elasna Open Source License v3.
// All original author information and file headers must be preserved.
// For full license text, see: [https://github.com/JHXStudioriginal/Elasna-License/blob/main/LICENSE]

#ifndef PROMPT_H
#define PROMPT_H

extern char current_dir[512];
extern char cvx_prompt[2048];

void print_prompt(void);
const char* get_prompt(void);

#endif
