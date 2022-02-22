#include <stdlib.h>
#include <stdbool.h>
#include "linked_list.h"
#include "error.h"

#ifndef COMPLETION_INPUT_H
#define COMPLETION_INPUT_H

#define MAX_LINE_SIZE  2048
static const char* BASH_LINE_VAR = "COMP_LINE";
static const char* BASH_CURSOR_VAR = "COMP_POINT";

typedef struct completion_input_t {
    char line[MAX_LINE_SIZE + 1];
    int cursor_pos;
} completion_input_t;

// global instance for completion_input
// completion_input_t completion_input;

completion_input_t* load_completion_input(bce_error_t *err);
linked_list_t* bash_input_to_list(const char *str, const size_t max_len);
bool get_command_from_input(completion_input_t *input, char* dest, size_t bufsize);
bool get_current_word(completion_input_t *input, char* dest, size_t bufsize);
bool get_previous_word(completion_input_t *input, char* dest, size_t bufsize);

#endif // COMPLETION_INPUT_H
