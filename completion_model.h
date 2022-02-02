#include "linked_list.h"
#include <stdbool.h>
#include <sqlite3.h>
#include "error.h"

#ifndef COMPLETION_MODEL_H
#define COMPLETION_MODEL_H

static const int UUID_FIELD_SIZE = 36;
static const int NAME_FIELD_SIZE = 50;
static const int SHORTNAME_FIELD_SIZE = 5;
static const int CMD_TYPE_FIELD_SIZE = 20;

typedef struct completion_command_t {
    char uuid[UUID_FIELD_SIZE + 1];
    char name[NAME_FIELD_SIZE + 1];
    char parent_cmd_uuid[UUID_FIELD_SIZE + 1];
    struct linked_list_t* aliases;
    struct linked_list_t* sub_commands;
    struct linked_list_t* command_args;
    bool is_present_on_cmdline;
} completion_command_t;

typedef struct completion_command_arg_t {
    char uuid[UUID_FIELD_SIZE + 1];
    char cmd_uuid[UUID_FIELD_SIZE + 1];
    char arg_type[CMD_TYPE_FIELD_SIZE + 1];
    char description[NAME_FIELD_SIZE + 1];
    char long_name[NAME_FIELD_SIZE + 1];
    char short_name[SHORTNAME_FIELD_SIZE + 1];
    bool is_present_on_cmdline;
    struct linked_list_t* opts;
} completion_command_arg_t;

typedef struct completion_command_opt_t {
    char uuid[UUID_FIELD_SIZE + 1];
    char cmd_arg_uuid[UUID_FIELD_SIZE + 1];
    char name[NAME_FIELD_SIZE + 1];
} completion_command_opt_t;

struct sqlite3* open_database(const char *filename, int *result);
int get_db_command(completion_command_t *cmd, struct sqlite3 *conn, const char* command_name);
int get_db_command_aliases(struct sqlite3 *conn, completion_command_t *parent_cmd);
int get_db_sub_commands(struct sqlite3 *conn, completion_command_t *parent_cmd);
int get_db_command_args(struct sqlite3 *conn, completion_command_t *parent_cmd);
int get_db_command_opts(struct sqlite3 *conn, completion_command_arg_t *parent_arg);
completion_command_t* create_completion_command();
completion_command_arg_t* create_completion_command_arg();
completion_command_opt_t* create_completion_command_opt();
void free_completion_command(completion_command_t **ppcmd);
void free_completion_command_arg(completion_command_arg_t **pparg);
void free_completion_command_opt(completion_command_opt_t **ppopt);
void print_command_tree(struct sqlite3 *conn, const completion_command_t *cmd, int level);

#endif // COMPLETION_MODEL_H