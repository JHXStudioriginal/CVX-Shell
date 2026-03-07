#ifndef FUNCTIONS_H
#define FUNCTIONS_H

void add_function(const char *name, const char *body);
const char* get_function(const char *name);
void remove_function(const char *name);
int cmd_functions(int argc, char **argv);
int cmd_delfunc(int argc, char **argv);

#endif
