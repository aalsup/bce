#include "input.h"
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include "error.h"

bool is_space_or_equals(int c) {
    if (isspace(c)) {
        return true;
    } else {
        if (c == '=') {
            return true;
        }
    }
    return false;
}

completion_input_t *create_completion_input(bce_error_t *err) {
    const char *line = getenv(BASH_LINE_VAR);
    if (!line || strlen(line) == 0) {
        *err = ERR_MISSING_ENV_COMP_LINE;
        return NULL;
    }
    const char *str_cursor_pos = getenv(BASH_CURSOR_VAR);
    if (!str_cursor_pos || strlen(str_cursor_pos) == 0) {
        *err = ERR_MISSING_ENV_COMP_POINT;
        return NULL;
    }
    completion_input_t *input = malloc(sizeof(completion_input_t));
    input->line[0] = '\0';
    strncat(input->line, line, MAX_LINE_SIZE);
    input->cursor_pos = (int) strtol(str_cursor_pos, (char **) NULL, 10);
    if ((input->cursor_pos == 0) && (errno != 0)) {
        free(input);
        *err = ERR_INVALID_ENV_COMP_POINT;
        return NULL;
    }
    *err = ERR_NONE;
    return input;
}

completion_input_t *free_completion_input(completion_input_t *input) {
    free(input);
    return NULL;
}

bool get_command_from_input(const completion_input_t *input, char *dest, const size_t bufsize) {
    bool result = false;
    memset(dest, 0, bufsize);
    linked_list_t *list = bash_input_to_list(input->line, MAX_LINE_SIZE);
    if (list) {
        if (list->head) {
            char *data = (char *) list->head->data;
            strncat(dest, data, bufsize);
            result = true;
        }
    }
    return result;
}

bool get_current_word(const completion_input_t *input, char *dest, const size_t bufsize) {
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

bool get_previous_word(const completion_input_t *input, char *dest, size_t bufsize) {
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
linked_list_t *bash_input_to_list(const char *cmd_line, const size_t max_len) {
    linked_list_t *list = ll_create(NULL);
    const char *p;
    char *start_of_word;
    int c;
    size_t counter = 0;
    enum states {DULL, IN_WORD, IN_QUOTE, IN_DBL_QUOTE} state = DULL;
    bool collect_word = false;

    for (p = cmd_line; *p != '\0'; p++) {
        c = (unsigned char) *p;  // convert to unsigned char for is* functions
        switch (state) {
            case DULL:  // not in a word or quotes
                if (isspace(c)) {
                    // ignore
                    continue;
                }
                // not a space - transition to new state
                if (c == '"') {
                    state = IN_DBL_QUOTE;
                    start_of_word = (char *) p + 1;  // word starts at next char
                    continue;
                } else if (c == '\'') {
                    state = IN_QUOTE;
                    start_of_word = (char *) p + 1;  // word starts at next char
                    continue;
                } else {
                    state = IN_WORD;
                    start_of_word = (char *) p;
                    continue;
                }
            case IN_WORD:
               // keep going until we get a space
               if (is_space_or_equals(c)) {
                   collect_word = true;
               }
            case IN_QUOTE:
                // keep going until get a quote
                if (c == '\'') {
                    collect_word = true;
                }
            case IN_DBL_QUOTE:
                if (c == '"') {
                    collect_word = true;
                }
        }
        if (collect_word) {
            // collect the word we just read
            size_t len = p - start_of_word;
            char *word = calloc(len + 1, sizeof(char));
            strncat(word, start_of_word, len);
            ll_append_item(list, word);
            // change state
            state = DULL;
            collect_word = false;
        }
        // make sure we only compare up to `max_len` characters
        counter++;
        if (counter >= max_len) {
            break;
        }
    }

    // check if we have 1 remaining word to include
    if (state != DULL) {
        size_t len = strlen(start_of_word);
        char *word = calloc(len + 1, sizeof(char));
        strncat(word, start_of_word, len);
        ll_append_item(list, word);

    }

    // tokenize based on POSIX whitespace characters and equals
    return list;
}