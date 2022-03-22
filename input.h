#ifndef BCE_INPUT_H
#define BCE_INPUT_H

#include <stdlib.h>
#include <stdbool.h>
#include "linked_list.h"
#include "error.h"

#define MAX_CMD_LINE_SIZE  4096

static const char *BASH_LINE_VAR = "COMP_LINE";
static const char *BASH_CURSOR_VAR = "COMP_POINT";

typedef struct completion_input_t {
    char line[MAX_CMD_LINE_SIZE + 1];
    int cursor_pos;
} completion_input_t;

completion_input_t *create_completion_input(bce_error_t *err);

completion_input_t *free_completion_input(completion_input_t *input);

linked_list_t *bash_input_to_list(const char *str, size_t max_len);

bool get_command_from_input(const completion_input_t *input, char *dest, size_t max_len);

bool get_current_word(const completion_input_t *input, char *dest, size_t max_len);

bool get_previous_word(const completion_input_t *input, char *dest, size_t max_len);

#endif // BCE_INPUT_H
