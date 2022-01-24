#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sqlite3.h>
#include <wordexp.h>

const int UUID_FIELD_SIZE = 36;
const int NAME_FIELD_SIZE = 50;
const int SHORTNAME_FIELD_SIZE = 5;
const int CMD_TYPE_FIELD_SIZE = 20;

const int MAX_LINE_SIZE = 2048;
const char* BASH_LINE_VAR = "COMP_LINE";
const char* BASH_CURSOR_VAR = "COMP_POINT";

const int ERR_MISSING_ENV_COMP_LINE = 1;
const int ERR_MISSING_ENV_COMP_POINT = 2;
const int ERR_OPEN_DATABASE = 3;
const int ERR_DATABASE_PRAGMA = 4;
const int ERR_DATABASE_SCHEMA = 5;

const char* ENSURE_SCHEMA_SQL =
" WITH table_count (n) AS "
" ( "
"     SELECT COUNT(name) AS n "
"     FROM sqlite_master "
"     WHERE type = 'table' "
"     AND name IN ('command', 'command_alias', 'command_arg', 'command_opt') "
" ) "
" SELECT "
"     CASE "
"         WHEN table_count.n = 4 THEN 1 "
"         ELSE 0 "
"     END AS pass "
" FROM table_count ";

const char* COMPLETION_COMMAND_SQL =
" SELECT c.uuid, c.name, c.parent_cmd "
" FROM command c "
" WHERE c.name = ?1 ";

struct completion_command_t {
    char uuid[UUID_FIELD_SIZE + 1];
    char name[NAME_FIELD_SIZE + 1];
    char parent_cmd_uuid[UUID_FIELD_SIZE + 1];
};

struct completion_command_arg_t {
    char uuid[UUID_FIELD_SIZE + 1];
    char cmd_uuid[UUID_FIELD_SIZE + 1];
    char cmd_type[CMD_TYPE_FIELD_SIZE + 1];
    char long_name[NAME_FIELD_SIZE + 1];
    char short_name[SHORTNAME_FIELD_SIZE + 1];
};

struct completion_command_opt_t {
    char uuid[UUID_FIELD_SIZE + 1];
    char cmd_arg_uuid[UUID_FIELD_SIZE + 1];
    char name[NAME_FIELD_SIZE + 1];
};

struct completion_input_t {
    char line[MAX_LINE_SIZE + 1];
    int  cursor_pos;
} completion_input;

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

int get_db_command(struct completion_command_t *dest, sqlite3 *conn, char* command_name) {
    memset(dest->uuid, 0, UUID_FIELD_SIZE);
    memset(dest->name, 0, NAME_FIELD_SIZE);
    memset(dest->parent_cmd_uuid, 0, UUID_FIELD_SIZE);

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare(conn, COMPLETION_COMMAND_SQL, -1, &stmt, 0);
    if (rc == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, command_name, -1, NULL);
        int step = sqlite3_step(stmt);
        if (step == SQLITE_ROW) {
            strncpy(dest->uuid, (const char *)sqlite3_column_text(stmt, 0), UUID_FIELD_SIZE);
            strncpy(dest->name, (const char *)sqlite3_column_text(stmt, 1), NAME_FIELD_SIZE);
            if (sqlite3_column_type(stmt, 2) == SQLITE_TEXT) {
                strncpy(dest->parent_cmd_uuid, (const char *)sqlite3_column_text(stmt, 2), UUID_FIELD_SIZE);
            }
        }
    }
    sqlite3_finalize(stmt);

    return rc;
}

int ensure_schema(sqlite3 *conn) {
    char *err_msg = 0;
    int rc = sqlite3_exec(conn, ENSURE_SCHEMA_SQL, 0, 0, &err_msg);
    return rc;
}

int main() {
    char *err_msg = 0;

    printf("SQLite version %s\n", sqlite3_libversion());

    sqlite3 *conn;
    int rc = sqlite3_open("completion.db", &conn);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(conn));
        sqlite3_close(conn);

        return ERR_OPEN_DATABASE;
    }

    rc = sqlite3_exec(conn, "PRAGMA journal_mode = 'WAL'", 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Unable to set journal_mode pragma\n");
        sqlite3_close(conn);
        return ERR_DATABASE_PRAGMA;
    }

    rc = sqlite3_exec(conn, "PRAGMA foreign_keys = 1", 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Unable to set foreign_keys pragma\n");
        sqlite3_close(conn);
        return ERR_DATABASE_PRAGMA;
    }

    rc = ensure_schema(conn);
    if (rc != SQLITE_OK) {
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
    struct completion_command_t completion_command;
    result = get_db_command(&completion_command, conn, command_name);
    if (result == SQLITE_OK) {
        printf("completion_command: %s", completion_command.uuid);
    }

    sqlite3_close(conn);

    return 0;
}


