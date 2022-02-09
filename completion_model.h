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
static const int DESCRIPTION_FIELD_SIZE = 200;

typedef struct completion_command_t {
    char uuid[UUID_FIELD_SIZE + 1];
    char name[NAME_FIELD_SIZE + 1];
    char parent_cmd_uuid[UUID_FIELD_SIZE + 1];
    struct linked_list_t* aliases;
    struct linked_list_t* sub_commands;
    struct linked_list_t* args;
    bool is_present_on_cmdline;
} completion_command_t;

typedef struct completion_command_alias_t {
    char uuid[UUID_FIELD_SIZE + 1];
    char cmd_uuid[UUID_FIELD_SIZE + 1];
    char name[NAME_FIELD_SIZE + 1];
} completion_command_alias_t;

typedef struct completion_command_arg_t {
    char uuid[UUID_FIELD_SIZE + 1];
    char cmd_uuid[UUID_FIELD_SIZE + 1];
    char arg_type[CMD_TYPE_FIELD_SIZE + 1];
    char description[DESCRIPTION_FIELD_SIZE + 1];
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

int get_db_command(completion_command_t *cmd, struct sqlite3 *conn, const char* command_name);
int get_db_command_aliases(struct sqlite3 *conn, completion_command_t *parent_cmd);
int get_db_sub_commands(struct sqlite3 *conn, completion_command_t *parent_cmd);
int get_db_command_args(struct sqlite3 *conn, completion_command_t *parent_cmd);
int get_db_command_opts(struct sqlite3 *conn, completion_command_arg_t *parent_arg);
completion_command_t* create_completion_command(void);
completion_command_alias_t* create_completion_command_alias(void);
completion_command_arg_t* create_completion_command_arg(void);
completion_command_opt_t* create_completion_command_opt(void);
void free_completion_command(completion_command_t **ppcmd);
void free_completion_command_alias(completion_command_alias_t **ppalias);
void free_completion_command_arg(completion_command_arg_t **pparg);
void free_completion_command_opt(completion_command_opt_t **ppopt);
void print_command_tree(struct sqlite3 *conn, const completion_command_t *cmd, int level);
int write_db_command(completion_command_t *completion_command, struct sqlite3 *conn);
int write_db_command_alias(completion_command_alias_t *alias, struct sqlite3 *conn);
int write_db_command_arg(struct sqlite3 *conn, completion_command_arg_t *arg);
int write_db_command_opt(struct sqlite3 *conn, completion_command_opt_t *opt);

#endif // COMPLETION_MODEL_H