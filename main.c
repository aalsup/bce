#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sqlite3.h>
#include <wordexp.h>
#include "linked_list.h"
#include "completion.h"
#include "completion_input.h"
#include "error.h"

int main() {
    int result = 0;
    completion_command_t *completion_command = NULL;
    char command_name[MAX_LINE_SIZE + 1];
    char current_word[MAX_LINE_SIZE + 1];
    char previous_word[MAX_LINE_SIZE + 1];

    printf("SQLite version %s\n", sqlite3_libversion());

    int rc = 0;
    sqlite3 *conn = open_database("completion.db", &rc);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Error %d opening database", rc);
        result = ERR_OPEN_DATABASE;
        goto done;
    }

    if (!ensure_schema(conn)) {
        fprintf(stderr, "Invalid database schema\n");
        result = ERR_DATABASE_SCHEMA;
        goto done;
    }

    result = load_completion_input();
    if (result != 0) {
        switch (result) {
            case ERR_MISSING_ENV_COMP_LINE:
                fprintf(stderr, "No %s env var\n", BASH_LINE_VAR);
            case ERR_MISSING_ENV_COMP_POINT:
                fprintf(stderr, "No %s env var\n", BASH_CURSOR_VAR);
            default:
                fprintf(stderr, "Unknown error: %d\n", result);
        }
        goto done;
    }

    // load the data provided by environment
    get_command(command_name, MAX_LINE_SIZE);
    get_current_word(current_word, MAX_LINE_SIZE);
    get_previous_word(previous_word, MAX_LINE_SIZE);

    printf("input: %s\n", completion_input.line);
    printf("command: %s\n", command_name);
    printf("current_word: %s\n", current_word);
    printf("previous_word: %s\n", previous_word);

    // search for the command directly (load all descendents)
    completion_command = create_completion_command();
    rc = get_db_command(completion_command, conn, command_name);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "get_db_command() returned %d\n", rc);
        goto done;
    }

    print_command_tree(conn, completion_command, 0);

    done:
    free_completion_command(completion_command);
    sqlite3_close(conn);

    return result;
}



