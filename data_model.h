#ifndef BCE_DATA_MODEL_H
#define BCE_DATA_MODEL_H

#include <stdbool.h>
#include <sqlite3.h>
#include "linked_list.h"
#include "error.h"

#define UUID_FIELD_SIZE        36
#define NAME_FIELD_SIZE        50
#define SHORTNAME_FIELD_SIZE   5
#define CMD_TYPE_FIELD_SIZE    20
#define DESCRIPTION_FIELD_SIZE 200

#define BCE_DB__FILENAME "completion.db"

typedef struct bce_command_t {
    char uuid[UUID_FIELD_SIZE + 1];
    char name[NAME_FIELD_SIZE + 1];
    char parent_cmd_uuid[UUID_FIELD_SIZE + 1];
    struct linked_list_t *aliases;
    struct linked_list_t *sub_commands;
    struct linked_list_t *args;
    bool is_present_on_cmdline;
} bce_command_t;

typedef struct bce_command_alias_t {
    char uuid[UUID_FIELD_SIZE + 1];
    char cmd_uuid[UUID_FIELD_SIZE + 1];
    char name[NAME_FIELD_SIZE + 1];
} bce_command_alias_t;

typedef struct bce_command_arg_t {
    char uuid[UUID_FIELD_SIZE + 1];
    char cmd_uuid[UUID_FIELD_SIZE + 1];
    char arg_type[CMD_TYPE_FIELD_SIZE + 1];
    char description[DESCRIPTION_FIELD_SIZE + 1];
    char long_name[NAME_FIELD_SIZE + 1];
    char short_name[SHORTNAME_FIELD_SIZE + 1];
    bool is_present_on_cmdline;
    struct linked_list_t *opts;
} bce_command_arg_t;

typedef struct bce_command_opt_t {
    char uuid[UUID_FIELD_SIZE + 1];
    char cmd_arg_uuid[UUID_FIELD_SIZE + 1];
    char name[NAME_FIELD_SIZE + 1];
} bce_command_opt_t;

bce_error_t prepare_statement_cache(struct sqlite3 *conn);

bce_error_t free_statement_cache(struct sqlite3 *conn);

bce_error_t load_db_command_names(struct sqlite3 *conn, linked_list_t *cmd_names);

bce_error_t load_db_command(struct sqlite3 *conn, bce_command_t *cmd, const char *command_name);

bce_error_t load_db_command_aliases(struct sqlite3 *conn, bce_command_t *parent_cmd);

bce_error_t load_db_sub_commands(struct sqlite3 *conn, bce_command_t *parent_cmd);

bce_error_t load_db_command_args(struct sqlite3 *conn, bce_command_t *parent_cmd);

bce_error_t load_db_command_opts(struct sqlite3 *conn, bce_command_arg_t *parent_arg);

bce_command_t *create_bce_command(void);

bce_command_alias_t *create_bce_command_alias(void);

bce_command_arg_t *create_bce_command_arg(void);

bce_command_opt_t *create_bce_command_opt(void);

bce_command_t *free_bce_command(bce_command_t *cmd);

bce_command_alias_t *free_bce_command_alias(bce_command_alias_t *alias);

bce_command_arg_t *free_bce_command_arg(bce_command_arg_t *arg);

bce_command_opt_t *free_bce_command_opt(bce_command_opt_t *opt);

void print_command_tree(const bce_command_t *cmd, int level);

bce_error_t write_db_command(struct sqlite3 *conn, const bce_command_t *completion_command);

bce_error_t write_db_command_alias(struct sqlite3 *conn, const bce_command_alias_t *alias);

bce_error_t write_db_command_arg(struct sqlite3 *conn, const bce_command_arg_t *arg);

bce_error_t write_db_command_opt(struct sqlite3 *conn, const bce_command_opt_t *opt);

bce_error_t delete_db_command(struct sqlite3 *conn, const char *command_name);

#endif // BCE_DATA_MODEL_H
