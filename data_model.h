#ifndef BCE_DATA_MODEL_H
#define BCE_DATA_MODEL_H

#include <stdbool.h>
#include <sqlite3.h>
#include "linked_list.h"
#include "error.h"

#define DB_SCHEMA_VERSION      1

#define UUID_FIELD_SIZE        36
#define NAME_FIELD_SIZE        50
#define SHORTNAME_FIELD_SIZE   5
#define CMD_TYPE_FIELD_SIZE    20
#define DESCRIPTION_FIELD_SIZE 200

#define BCE_DB_FILENAME "completion.db"

typedef struct bce_command_t {
    char uuid[UUID_FIELD_SIZE + 1];
    char name[NAME_FIELD_SIZE + 1];
    char parent_cmd_uuid[UUID_FIELD_SIZE + 1];
    struct linked_list_t *aliases;          /* bce_command_alias_t */
    struct linked_list_t *sub_commands;     /* bce_command_t */
    struct linked_list_t *args              /* bce_command_arg_t */;
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

bce_command_t *bce_command_new(void);

bce_command_alias_t *bce_command_alias_new(void);

bce_command_arg_t *bce_command_arg_new(void);

bce_command_opt_t *bce_command_opt_new(void);

bce_command_t *bce_command_free(bce_command_t *cmd);

bce_command_alias_t *bce_command_alias_free(bce_command_alias_t *alias);

bce_command_arg_t *bce_command_arg_free(bce_command_arg_t *arg);

bce_command_opt_t *bce_command_opt_free(bce_command_opt_t *opt);

/* Create a cache of prepared statements which are used for loading command-data hierarchy */
bce_error_t db_prepare_stmt_cache(struct sqlite3 *conn);

/* Free the cached prepared statements, when done loading the command-data hierarchy */
bce_error_t db_free_stmt_cache(struct sqlite3 *conn);

/* Query to root command names stored in SQLite */
bce_error_t db_query_root_command_names(struct sqlite3 *conn, linked_list_t *cmd_names);

/* Query a specific command in SQLite */
bce_error_t db_query_command(struct sqlite3 *conn, bce_command_t *cmd, const char *command_name);

/* Query the command aliases */
bce_error_t db_query_command_aliases(struct sqlite3 *conn, bce_command_t *parent_cmd);

/* Query the sub-commands */
bce_error_t db_query_sub_commands(struct sqlite3 *conn, bce_command_t *parent_cmd);

/* Query the command args */
bce_error_t db_query_command_args(struct sqlite3 *conn, bce_command_t *parent_cmd);

/* Query the argument options */
bce_error_t db_query_command_opts(struct sqlite3 *conn, bce_command_arg_t *parent_arg);

bce_error_t db_store_command(struct sqlite3 *conn, const bce_command_t *completion_command);

bce_error_t db_store_command_alias(struct sqlite3 *conn, const bce_command_alias_t *alias);

bce_error_t db_store_command_arg(struct sqlite3 *conn, const bce_command_arg_t *arg);

bce_error_t db_store_command_opt(struct sqlite3 *conn, const bce_command_opt_t *opt);

/* Delete the command from the database (recursively deletes all child records) */
bce_error_t db_delete_command(struct sqlite3 *conn, const char *command_name);

#endif // BCE_DATA_MODEL_H
