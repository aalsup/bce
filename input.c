#include "input.h"
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include "error.h"

static inline bool is_space_or_equals(int c) {
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
    strncat(input->line, line, MAX_CMD_LINE_SIZE);
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

bool get_command_from_input(const completion_input_t *input, char *dest, const size_t max_len) {
    bool result = false;
    memset(dest, 0, max_len);
    linked_list_t *list = bash_input_to_list(input->line, MAX_CMD_LINE_SIZE);
    if (list) {
        if (list->head) {
            char *data = (char *) list->head->data;
            strncat(dest, data, max_len);
            result = true;
        }
    }
    return result;
}

bool get_current_word(const completion_input_t *input, char *dest, const size_t max_len) {
    memset(dest, 0, max_len);
    bool result = false;

    // build a list of words, up to the current cursor position
    linked_list_t *list = bash_input_to_list(input->line, input->cursor_pos);
    if (list) {
        if (list->size > 0) {
            size_t elem = list->size - 1;
            char *data = (char *) ll_get_nth_item(list, elem);
            strncat(dest, data, max_len);
            result = true;
        }
    }
    list = ll_destroy(list);
    return result;
}

bool get_previous_word(const completion_input_t *input, char *dest, size_t max_len) {
    memset(dest, 0, max_len);
    bool result = false;

    // build a list of words, up to the current cursor position
    linked_list_t *list = bash_input_to_list(input->line, input->cursor_pos);
    if (list) {
        if (list->size > 1) {
            size_t elem = list->size - 2;   // next-to-last element (0-based)
            char *data = (char *) ll_get_nth_item(list, elem);
            strncat(dest, data, max_len);
            result = true;
        }
    }
    list = ll_destroy(list);
    return result;
}

/*
 * Split the cmd_line into discrete items, based on the same rules that BASH uses.
 */
linked_list_t *bash_input_to_list(const char *cmd_line, const size_t max_len) {
    linked_list_t *list = ll_create(NULL);

    enum states {
        NADA, IN_WORD, IN_QUOTE, IN_DBL_QUOTE
    } state = NADA;

    const char *p;                  // pointer to the current character
    char *start_of_word = NULL;     // pointer to the start of a word
    size_t c_count = 0;             // num of parsed characters (don't exceed max_len)

    for (p = cmd_line; *p != '\0'; p++) {
        bool got_word = false;
        int c = (unsigned char) *p;  // convert to unsigned char for is* functions
        switch (state) {
            case NADA:  // not in a word or quotes
                if (isspace(c)) {
                    // ignore
                    break;
                }
                // not a space - transition to new state
                if (c == '\'') {
                    state = IN_QUOTE;
                    start_of_word = (char *) p + 1;  // word starts at next char
                    break;
                } else if (c == '"') {
                    state = IN_DBL_QUOTE;
                    start_of_word = (char *) p + 1;  // word starts at next char
                    break;
                } else {
                    state = IN_WORD;
                    start_of_word = (char *) p;
                    break;
                }
            case IN_WORD:
                // keep going until we get a space
                got_word = is_space_or_equals(c);
                break;
            case IN_QUOTE:
                // keep going until get a quote
                got_word = (c == '\'');
                break;
            case IN_DBL_QUOTE:
                got_word = (c == '"');
                break;
        }
        if (got_word) {
            // collect the word we just read, from start_of_word..p (exclusive)
            size_t word_len = p - start_of_word;
            char *word = calloc(word_len + 1, sizeof(char));
            strncat(word, start_of_word, word_len);
            ll_append_item(list, word);
            // change state
            state = NADA;
            got_word = false;
            start_of_word = NULL;
        }
        // make sure we only compare up to `max_len` characters
        c_count++;
        if (c_count >= max_len) {
            break;
        }
    }

    // check if we have a remaining word in the buffer
    if ((state != NADA) && (start_of_word)) {
        // make sure we don't consider more characters than allowed
        // `(p + 1) - start_of_word` gets all characters (inclusive)
        size_t word_len = (p + 1) - start_of_word;
        char *word = calloc(word_len + 1, sizeof(char));
        strncat(word, start_of_word, word_len);
        ll_append_item(list, word);
    }

    return list;
}