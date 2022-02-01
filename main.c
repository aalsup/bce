#define VERBOSE_OUTPUT

#include <stdio.h>
#include <string.h>
#include <sqlite3.h>
#include <wordexp.h>
#include "linked_list.h"
#include "dbutil.h"
#include "completion_model.h"
#include "completion_input.h"
#include "error.h"
#include "prune.h"

int main() {
    int err = 0;    // custom error values
    int rc = 0;     // SQLite return values
    completion_command_t *completion_command = NULL;
    char command_name[MAX_LINE_SIZE + 1];
    char current_word[MAX_LINE_SIZE + 1];
    char previous_word[MAX_LINE_SIZE + 1];

#ifdef VERBOSE_OUTPUT
    printf("SQLite version %s\n", sqlite3_libversion());
#endif

    sqlite3 *conn = open_database("completion.db", &rc);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Error %d opening database", rc);
        err = ERR_OPEN_DATABASE;
        goto done;
    }

    if (!ensure_schema(conn)) {
        if (!create_schema(conn, &rc)) {
            fprintf(stderr, "Unable to create database schema\n");
            err = ERR_DATABASE_SCHEMA;
            goto done;
        }
    }

    err = load_completion_input();
    if (err != 0) {
        switch (err) {
            case ERR_MISSING_ENV_COMP_LINE:
                fprintf(stderr, "No %s env var\n", BASH_LINE_VAR);
            case ERR_MISSING_ENV_COMP_POINT:
                fprintf(stderr, "No %s env var\n", BASH_CURSOR_VAR);
            default:
                fprintf(stderr, "Unknown error: %d\n", err);
        }
        goto done;
    }

    // load the data provided by environment
    if (!get_command_input(command_name, MAX_LINE_SIZE)) {
        fprintf(stderr, "Unable to determine command\n");
        goto done;
    }
    get_current_word(current_word, MAX_LINE_SIZE);
    get_previous_word(previous_word, MAX_LINE_SIZE);

#ifdef VERBOSE_OUTPUT
    printf("input: %s\n", completion_input.line);
    printf("command: %s\n", command_name);
    printf("current_word: %s\n", current_word);
    printf("previous_word: %s\n", previous_word);
#endif

    // search for the command directly (load all descendents)
    completion_command = create_completion_command();
    rc = get_db_command(completion_command, conn, command_name);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "get_db_command() returned %d\n", rc);
        goto done;
    }

#ifdef VERBOSE_OUTPUT
    printf("\nCommand Tree (Database)\n");
    print_command_tree(conn, completion_command, 0);
#endif

    prune_command(completion_command);

#ifdef VERBOSE_OUTPUT
    printf("\nCommand Tree (Pruned)\n");
    print_command_tree(conn, completion_command, 0);
#endif

    done:
    free_completion_command(&completion_command);
    sqlite3_close(conn);

    return err;
}



