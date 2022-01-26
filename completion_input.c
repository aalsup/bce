#include "completion_input.h"
#include "completion.h"
#include <string.h>
#include <stdio.h>

int load_completion_input() {
    const char* line = getenv(BASH_LINE_VAR);
    if (strlen(line) == 0) {
        fprintf(stderr, "No %s env var", BASH_LINE_VAR);
        return(ERR_MISSING_ENV_COMP_LINE);
    }
    const char* str_cursor_pos = getenv(BASH_CURSOR_VAR);
    if (strlen(str_cursor_pos) == 0) {
        fprintf(stderr, "No %s env var", BASH_CURSOR_VAR);
        return(ERR_MISSING_ENV_COMP_POINT);
    }
    strncat(completion_input.line, line, MAX_LINE_SIZE);
    completion_input.cursor_pos = (int) strtol(str_cursor_pos, (char **)NULL, 10);
    return 0;
}

bool get_command(char* dest, size_t bufsize) {
    bool result = false;
    memset(dest, 0, bufsize);
    linked_list_t *list = string_to_list(completion_input.line, " ", MAX_LINE_SIZE);
    if (list != NULL) {
        if (list->head != NULL) {
            char *data = (char *)list->head->data;
            strncat(dest, data, bufsize);
            result = true;
        }
    }
    return result;
}

bool get_current_word(char* dest, size_t bufsize) {
    memset(dest, 0, bufsize);
    bool result = false;

    // build a list of words, up to the current cursor position
    linked_list_t *list = string_to_list(completion_input.line, " ", completion_input.cursor_pos);
    if (list != NULL) {
        size_t last_word = list->size - 1;
        char *data = (char *)ll_get_nth_element(list, last_word);
        strncat(dest, data, bufsize);
        result = true;
    }
    ll_destroy(&list);
    return result;
}


