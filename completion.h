#include "linked_list.h"
#include <stdbool.h>
#include <sqlite3.h>

#ifndef COMPLETION_H
#define COMPLETION_H

#define UUID_FIELD_SIZE 36
#define NAME_FIELD_SIZE 50
#define SHORTNAME_FIELD_SIZE 5
#define CMD_TYPE_FIELD_SIZE 20

#define ERR_MISSING_ENV_COMP_LINE 1
#define ERR_MISSING_ENV_COMP_POINT 2
#define ERR_OPEN_DATABASE 3
#define ERR_DATABASE_PRAGMA 4
#define ERR_DATABASE_SCHEMA 5



extern const char* ENSURE_SCHEMA_SQL;
extern const char* COMPLETION_COMMAND_SQL;
extern const char* COMPLETION_SUB_COMMAND_SQL;
extern const char* COMPLETION_SUB_COMMAND_COUNT_SQL;

typedef struct completion_command_t {
    char uuid[UUID_FIELD_SIZE + 1];
    char name[NAME_FIELD_SIZE + 1];
    char parent_cmd_uuid[UUID_FIELD_SIZE + 1];
    struct linked_list_t* sub_commands;
    struct linked_list_t* command_args;
} completion_command_t;

typedef struct completion_command_arg_t {
    char uuid[UUID_FIELD_SIZE + 1];
    char cmd_uuid[UUID_FIELD_SIZE + 1];
    char cmd_type[CMD_TYPE_FIELD_SIZE + 1];
    char long_name[NAME_FIELD_SIZE + 1];
    char short_name[SHORTNAME_FIELD_SIZE + 1];
} completion_command_arg_t;

typedef struct completion_command_opt_t {
    char uuid[UUID_FIELD_SIZE + 1];
    char cmd_arg_uuid[UUID_FIELD_SIZE + 1];
    char name[NAME_FIELD_SIZE + 1];
} completion_command_opt_t;

sqlite3* open_database(const char *filename, int *result);
bool ensure_schema(sqlite3 *conn);
void print_command_tree(sqlite3 *conn, completion_command_t *cmd, int level);
int get_db_command(completion_command_t *dest, sqlite3 *conn, char* command_name);
int get_db_sub_commands(sqlite3 *conn, completion_command_t *parent_cmd);
void free_completion_command(completion_command_t *cmd);
void free_completion_command_arg(completion_command_arg_t *arg);

#endif // COMPLETION_H