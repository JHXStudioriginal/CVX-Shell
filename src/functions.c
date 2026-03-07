#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "functions.h"

typedef struct shell_function {
    char *name;
    char *body;
    struct shell_function *next;
} shell_function_t;

static shell_function_t *functions_head = NULL;

void add_function(const char *name, const char *body) {
    shell_function_t *curr = functions_head;
    while (curr) {
        if (strcmp(curr->name, name) == 0) {
            free(curr->body);
            curr->body = strdup(body);
            return;
        }
        curr = curr->next;
    }

    shell_function_t *new_func = malloc(sizeof(shell_function_t));
    new_func->name = strdup(name);
    new_func->body = strdup(body);
    new_func->next = functions_head;
    functions_head = new_func;
}

const char* get_function(const char *name) {
    shell_function_t *curr = functions_head;
    while (curr) {
        if (strcmp(curr->name, name) == 0) {
            return curr->body;
        }
        curr = curr->next;
    }
    return NULL;
}

void remove_function(const char *name) {
    shell_function_t *curr = functions_head;
    shell_function_t *prev = NULL;

    while (curr) {
        if (strcmp(curr->name, name) == 0) {
            if (prev) {
                prev->next = curr->next;
            } else {
                functions_head = curr->next;
            }
            free(curr->name);
            free(curr->body);
            free(curr);
            return;
        }
        prev = curr;
        curr = curr->next;
    }
}

int cmd_functions(int argc, char **argv) {
    (void)argc;
    (void)argv;
    shell_function_t *curr = functions_head;
    while (curr) {
        printf("%s() {\n%s\n}\n", curr->name, curr->body);
        curr = curr->next;
    }
    return 0;
}

int cmd_delfunc(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: delfunc <name>\n");
        return 1;
    }
    remove_function(argv[1]);
    return 0;
}
