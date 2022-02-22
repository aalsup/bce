#include "input.h"
#include "data_model.h"
#include <string.h>
#include <errno.h>
#include "error.h"

 completion_input_t* create_completion_input(bce_error_t *err) {
    const char* line = getenv(BASH_LINE_VAR);
    if (!line || strlen(line) == 0) {
        *err = ERR_MISSING_ENV_COMP_LINE;
        return NULL;
    }
    const char* str_cursor_pos = getenv(BASH_CURSOR_VAR);
    if (!str_cursor_pos || strlen(str_cursor_pos) == 0) {
        *err = ERR_MISSING_ENV_COMP_POINT;
        return NULL;
    }
    completion_input_t *input = (completion_input_t *) malloc(sizeof(completion_input_t));
    input->line[0] = '\0';
    strncat(input->line, line, MAX_LINE_SIZE);
    input->cursor_pos = (int) strtol(str_cursor_pos, (char **)NULL, 10);
    if ((input->cursor_pos == 0) && (errno != 0)) {
        free(input);
        *err = ERR_INVALID_ENV_COMP_POINT;
        return NULL;
    }
    *err = ERR_NONE;
    return input;
}

completion_input_t* free_completion_input(completion_input_t *input) {
    free(input);
    return NULL;
}

bool get_command_from_input(completion_input_t *input, char* dest, size_t bufsize) {
    bool result = false;
    memset(dest, 0, bufsize);
    linked_list_t *list = bash_input_to_list(input->line, MAX_LINE_SIZE);
    if (list) {
        if (list->head) {
            char *data = (char *)list->head->data;
            strncat(dest, data, bufsize);
            result = true;
        }
    }
    return result;
}

bool get_current_word(completion_input_t *input, char* dest, size_t bufsize) {
    memset(dest, 0, bufsize);
    bool result = false;

    // build a list of words, up to the current cursor position
    linked_list_t *list = bash_input_to_list(input->line, input->cursor_pos);
    if (list) {
        if (list->size > 0) {
            size_t elem = list->size - 1;
            char *data = (char *) ll_get_nth_item(list, elem);
            strncat(dest, data, bufsize);
            result = true;
        }
    }
    list = ll_destroy(list);
    return result;
}

bool get_previous_word(completion_input_t *input, char* dest, size_t bufsize) {
    memset(dest, 0, bufsize);
    bool result = false;

    // build a list of words, up to the current cursor position
    linked_list_t *list = bash_input_to_list(input->line, input->cursor_pos);
    if (list) {
        if (list->size > 1) {
            size_t elem = list->size - 2;
            char *data = (char *) ll_get_nth_item(list, elem);
            strncat(dest, data, bufsize);
            result = true;
        }
    }
    list = ll_destroy(list);
    return result;
}

/*
 * TODO: Implement the following logic
 *
 * [1] Split on quotes
 *     beep --boop="/home/robot/fav sounds" -v
 *     [beep --boop=] [/home/robot/fav sounds] [-v]
 *     [    false   ] [        true          ] [ f]
 *
 * [2] Split on whitespace or equals (where not quoted)
 *     [beep] [--boop] [/home/robot/fav sounds] [-v]
 *
 * TODO: How to handle nested, escaped quotes?
 */
linked_list_t* bash_input_to_list(const char *str, const size_t max_len) {
    // POSIX whitespace characters and equals
    char delim[] = " \t\r\n\v\f=";

    linked_list_t *list = ll_string_to_list(str, delim, max_len);

    return list;
}