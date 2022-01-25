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


char** get_line_as_array(int max_chars) {
    // count the argument elements
    int arg_count = 0;
    {
        char str[max_chars + 1];
        memset(str, 0, max_chars + 1);
        strncpy(str, completion_input.line, max_chars);
        char *arg = strtok(str, " ");
        while (arg != NULL) {
            arg_count++;
            arg = strtok(NULL, " ");
        }
    }

    // allocate the array
    char **line_array;
    {
        line_array = calloc(arg_count + 1, sizeof(char *));
        for (int i = 0; i < arg_count; i++) {
            line_array[i] = calloc(max_chars + 1, sizeof(char *));
        }
        line_array[arg_count] = NULL;
    }

    // iterate through the line again
    {
        char str[max_chars + 1];
        memset(str, 0, max_chars + 1);
        strncpy(str, completion_input.line, max_chars);
        char *arg = strtok(str, " ");
        int i = 0;
        while (arg != NULL) {
            strncpy(line_array[i], arg, max_chars);
            i++;
            arg = strtok(NULL, " ");
        }
    }

    return line_array;
}

void free_line_array(char** line_array) {
    if (line_array != NULL) {
        int i = 0;
        while (line_array[i] != NULL) {
            free(line_array[i]);
            i++;
        }
        free(line_array);
        line_array = NULL;
    }
}

bool get_command(char* dest, size_t bufsize) {
    memset(dest, 0, bufsize);
    char** line_array = get_line_as_array(MAX_LINE_SIZE);
    if (line_array[0] != NULL) {
        strncpy(dest, line_array[0], bufsize);
        free_line_array(line_array);
        return true;
    }
    free_line_array(line_array);
    return false;
}

bool get_current_word(char* dest, size_t bufsize) {
    memset(dest, 0, bufsize);
    // convert the line into array, up to the current cursor position
    char** line_array = get_line_as_array(completion_input.cursor_pos);
    int elements = 0;
    while (line_array[elements] != NULL) {
        elements++;
    }
    if (elements >= 1) {
        strncpy(dest, line_array[elements - 1], bufsize);
        free_line_array(line_array);
        return true;
    }
    free_line_array(line_array);
    return false;
}

bool get_previous_word(char* dest, size_t bufsize) {
    memset(dest, 0, bufsize);
    // convert the line into array, up to the current cursor position
    char** line_array = get_line_as_array(completion_input.cursor_pos);
    int elements = 0;
    while (line_array[elements] != NULL) {
        elements++;
    }
    if (elements >= 2) {
        // get the word prior to the current word
        strncpy(dest, line_array[elements - 2], bufsize);
        free_line_array(line_array);
        return true;
    }
    free_line_array(line_array);
    return false;
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
        exit(ERR_MISSING_ENV_COMP_POINT);
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

    printf("completion_command (from db): %s\n", completion_command.uuid);

    print_command_tree(conn, &completion_command, 0);

done:
    free_completion_command(&completion_command);
    sqlite3_close(conn);

    return 0;
}



