#include <stdlib.h>
#include <stdbool.h>

#ifndef COMPLETION_INPUT_H
#define COMPLETION_INPUT_H

static const int MAX_LINE_SIZE = 2048;
static const char* BASH_LINE_VAR = "COMP_LINE";
static const char* BASH_CURSOR_VAR = "COMP_POINT";

typedef struct completion_input_t {
    char line[MAX_LINE_SIZE + 1];
    int  cursor_pos;
} completion_input_t;

// global instance for completion_input
completion_input_t completion_input;

int load_completion_input();
bool get_command(char* dest, size_t bufsize);
bool get_current_word(char* dest, size_t bufsize);


#endif // COMPLETION_INPUT_H
