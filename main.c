#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sqlite3.h>
#include <wordexp.h>
#include "linked_list.h"
#include "completion.h"

const int MAX_LINE_SIZE = 2048;
const char* BASH_LINE_VAR = "COMP_LINE";
const char* BASH_CURSOR_VAR = "COMP_POINT";

typedef struct completion_input_t {
    char line[MAX_LINE_SIZE + 1];
    int  cursor_pos;
} completion_input_t;

// global instance for completion_input
completion_input_t completion_input;

bool get_command(char* dest, size_t bufsize) {
    bool result = false;
    memset(dest, 0, bufsize);
    linked_list_t *list = string_to_list(completion_input.line, " ", MAX_LINE_SIZE);
    if (list != NULL) {
        if (list->head != NULL) {
            char *data = (char *)list->head->data;
            strncpy(dest, data, bufsize);
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
        int last_word = list->size - 1;
        char *data = (char *)ll_get_nth_element(list, last_word);
        strncpy(dest, data, bufsize);
        result = true;
    }
    ll_destroy(list);
    return result;
}

bool get_previous_word(char* dest, size_t bufsize) {
    memset(dest, 0, bufsize);
    bool result = false;

    // build a list of words, up to the current cursor position
    linked_list_t *list = string_to_list(completion_input.line, " ", completion_input.cursor_pos);
    if (list != NULL) {
        int elem = list->size - 2;
        char *data = (char *)ll_get_nth_element(list, elem);
        strncpy(dest, data, bufsize);
        result = true;
    }
    ll_destroy(list);
    return result;
}

int load_completion_input() {
    const char* line = getenv(BASH_LINE_VAR);
    if (strlen(line) == 0) {
        printf("No %s env var", BASH_LINE_VAR);
        return(ERR_MISSING_ENV_COMP_LINE);
    }
    const char* str_cursor_pos = getenv(BASH_CURSOR_VAR);
    if (strlen(str_cursor_pos) == 0) {
        printf("No %s env var", BASH_CURSOR_VAR);
        return(ERR_MISSING_ENV_COMP_POINT);
    }
    strncpy(completion_input.line, line, MAX_LINE_SIZE);
    completion_input.cursor_pos = (int) strtol(str_cursor_pos, (char **)NULL, 10);
    return 0;
}

int main() {
    char *err_msg = 0;

    printf("SQLite version %s\n", sqlite3_libversion());

    int rc = 0;
    sqlite3 *conn = open_database("completion.db", &rc);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Error %d opening database", rc);
        return rc;
    }

    if (!ensure_schema(conn)) {
        fprintf(stderr, "Invalid database schema\n");
        sqlite3_close(conn);
        return ERR_DATABASE_SCHEMA;
    }

    int result = load_completion_input();
    if (result != 0) {
        switch (result) {
            case ERR_MISSING_ENV_COMP_LINE:
                printf("No %s env var\n", BASH_LINE_VAR);
            case ERR_MISSING_ENV_COMP_POINT:
                printf("No %s env var\n", BASH_CURSOR_VAR);
            default:
                printf("Unknown error: %d\n", result);
        }
    }

    char command_name[MAX_LINE_SIZE + 1];
    char current_word[MAX_LINE_SIZE + 1];
    char previous_word[MAX_LINE_SIZE + 1];

    get_command(command_name, MAX_LINE_SIZE);
    get_current_word(current_word, MAX_LINE_SIZE);
    get_previous_word(previous_word, MAX_LINE_SIZE);

    printf("input: %s\n", completion_input.line);
    printf("command: %s\n", command_name);
    printf("current_word: %s\n", current_word);
    printf("previous_word: %s\n", previous_word);

    // search for the command directly
    completion_command_t completion_command;
    rc = get_db_command(&completion_command, conn, command_name);
    if (rc != SQLITE_OK) {
        printf("get_db_command() returned %d\n", rc);
        goto done;
    }

    print_command_tree(conn, &completion_command, 0);

done:
    free_completion_command(&completion_command);
    sqlite3_close(conn);

    return 0;
}



