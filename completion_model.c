#include "completion_model.h"
#include "linked_list.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sqlite3.h>

static const char* COMPLETION_COMMAND_SQL =
        " SELECT c.uuid, c.name, c.parent_cmd "
        " FROM command c "
        " JOIN command_alias a ON a.cmd_uuid = c.uuid "
        " WHERE c.name = ?1 OR a.name = ?2 ";

static const char* COMPLETION_COMMAND_ALIAS_SQL =
        " SELECT a.uuid, a.cmd_uuid, a.name "
        " FROM command_alias a "
        " WHERE a.cmd_uuid = ?1 ";

static const char* COMPLETION_SUB_COMMAND_SQL =
        " SELECT c.uuid, c.name, c.parent_cmd "
        " FROM command c "
        " WHERE c.parent_cmd = ?1 ";

static const char* COMPLETION_COMMAND_ARG_SQL =
        " SELECT ca.uuid, ca.cmd_uuid, ca.arg_type, ca.description, ca.long_name, ca.short_name "
        " FROM command_arg ca "
        " JOIN command c ON c.uuid = ca.cmd_uuid "
        " WHERE c.uuid = ?1 ";

static const char* COMPLETION_COMMAND_OPT_SQL =
        " SELECT co.uuid, co.cmd_arg_uuid, co.name "
        " FROM command_opt co "
        " JOIN command_arg ca ON ca.uuid = co.cmd_arg_uuid "
        " WHERE ca.uuid = ?1 "
        " ORDER BY ca.long_name ";

sqlite3* open_database(const char *filename, int *result) {
    char *err_msg = 0;
    sqlite3 *conn;

    int rc = sqlite3_open(filename, &conn);
    if (rc != SQLITE_OK) {
        sqlite3_close(conn);

        *result = ERR_OPEN_DATABASE;
        return NULL;
    }

    rc = sqlite3_exec(conn, "PRAGMA journal_mode = 'WAL'", 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        sqlite3_close(conn);

        *result = ERR_DATABASE_PRAGMA;
        return NULL;
    }

    rc = sqlite3_exec(conn, "PRAGMA foreign_keys = 1", 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        sqlite3_close(conn);

        *result = ERR_DATABASE_PRAGMA;
        return NULL;
    }

    *result = SQLITE_OK;
    return conn;
}

void print_command_tree(struct sqlite3 *conn, const completion_command_t *cmd, const int level) {
    // indent
    for (int i = 0; i < level; i++) {
        printf("  ");
    }
    printf("command: %s\n", cmd->name);

    if (cmd->aliases && (cmd->aliases->size > 0)) {
        for (int i = 0; i < level; i++) {
            printf("  ");
        }
        printf("  aliases: ");
        linked_list_node_t *alias_node = cmd->aliases->head;
        while (alias_node != NULL) {
            char *str = (char *)alias_node->data;
            printf("%s ", str);
            alias_node = alias_node->next;
        }
        printf("\n");
    }

    if (cmd->args) {
        linked_list_node_t *arg_node = cmd->args->head;
        while (arg_node) {
            completion_command_arg_t *arg = (completion_command_arg_t *)arg_node->data;
            if (arg) {
                for (int i = 0; i < level; i++) {
                    printf("  ");
                }
                printf("  arg: %s (%s): %s\n", arg->long_name, arg->short_name, arg->arg_type);

                // print opts
                if (arg->opts) {
                    linked_list_node_t *opt_node = arg->opts->head;
                    while (opt_node) {
                        completion_command_opt_t *opt = (completion_command_opt_t *)opt_node->data;
                        if (opt) {
                            for (int i = 0; i < level; i++) {
                                printf("  ");
                            }
                            printf("    opt: %s\n", opt->name);
                        }
                        opt_node = opt_node->next;
                    }
                }
            }
            arg_node = arg_node->next;
        }
    }

    if (cmd->sub_commands) {
        linked_list_node_t *node = cmd->sub_commands->head;
        while (node) {
            completion_command_t *sub_cmd = (completion_command_t *) node->data;
            print_command_tree(conn, sub_cmd, level + 1);
            node = node->next;
        }
    }
}

int get_db_command(completion_command_t *cmd, struct sqlite3 *conn, const char* command_name) {
    memset(cmd->uuid, 0, UUID_FIELD_SIZE + 1);
    memset(cmd->name, 0, NAME_FIELD_SIZE + 1);
    memset(cmd->parent_cmd_uuid, 0, UUID_FIELD_SIZE + 1);
    cmd->aliases = NULL;
    cmd->sub_commands = NULL;
    cmd->args = NULL;

    sqlite3_stmt *stmt;
    // try to find the command by name
    int rc = sqlite3_prepare(conn, COMPLETION_COMMAND_SQL, -1, &stmt, 0);
    if (rc == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, command_name, -1, NULL);
        sqlite3_bind_text(stmt, 2, command_name, -1, NULL);
        int step = sqlite3_step(stmt);
        if (step == SQLITE_ROW) {
            strncat(cmd->uuid, (const char *) sqlite3_column_text(stmt, 0), UUID_FIELD_SIZE);
            strncat(cmd->name, (const char *) sqlite3_column_text(stmt, 1), NAME_FIELD_SIZE);
            if (sqlite3_column_type(stmt, 2) == SQLITE_TEXT) {
                strncat(cmd->parent_cmd_uuid, (const char *) sqlite3_column_text(stmt, 2), UUID_FIELD_SIZE);
            } else {
                memset(cmd->parent_cmd_uuid, 0, UUID_FIELD_SIZE + 1);
            }
            ll_free_node_func free_command = (ll_free_node_func) &free_completion_command;
            ll_free_node_func free_arg = (ll_free_node_func) &free_completion_command_arg;
            cmd->aliases = ll_create(NULL);
            cmd->sub_commands = ll_create(free_command);
            cmd->args = ll_create(free_arg);

            // populate child aliases
            rc = get_db_command_aliases(conn, cmd);
            if (rc != SQLITE_OK) {
                goto done;
            }

            // populate child args
            rc = get_db_command_args(conn, cmd);
            if (rc != SQLITE_OK) {
                goto done;
            }

            // populate child sub-cmds
            rc = get_db_sub_commands(conn, cmd);
            if (rc != SQLITE_OK) {
                goto done;
            }
        }
    }

done:
    sqlite3_finalize(stmt);
    return rc;
}

int get_db_command_aliases(struct sqlite3 *conn, completion_command_t *parent_cmd) {
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare(conn, COMPLETION_COMMAND_ALIAS_SQL, -1, &stmt, 0);
    if (rc == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, parent_cmd->uuid, -1, NULL);
        int step = sqlite3_step(stmt);
        while (step == SQLITE_ROW) {
            // 'name' field is all we need
            char *alias_name = calloc(NAME_FIELD_SIZE + 1, sizeof(char));
            strncat(alias_name, (const char *)sqlite3_column_text(stmt, 2), NAME_FIELD_SIZE);

            // add this alias to the parent
            ll_append_item(parent_cmd->aliases, alias_name);

            step = sqlite3_step(stmt);
        }
    }
    sqlite3_finalize(stmt);

    return rc;
}

int get_db_sub_commands(struct sqlite3 *conn, completion_command_t *parent_cmd) {
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare(conn, COMPLETION_SUB_COMMAND_SQL, -1, &stmt, 0);
    if (rc == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, parent_cmd->uuid, -1, NULL);
        int step = sqlite3_step(stmt);
        while (step == SQLITE_ROW) {
            // create completion_command_t
            completion_command_t *sub_cmd = create_completion_command();
            strncat(sub_cmd->uuid, (const char *)sqlite3_column_text(stmt, 0), UUID_FIELD_SIZE);
            strncat(sub_cmd->name, (const char *)sqlite3_column_text(stmt, 1), NAME_FIELD_SIZE);
            if (sqlite3_column_type(stmt, 2) == SQLITE_TEXT) {
                strncat(sub_cmd->parent_cmd_uuid, (const char *)sqlite3_column_text(stmt, 2), UUID_FIELD_SIZE);
            } else {
                memset(sub_cmd->parent_cmd_uuid, 0, UUID_FIELD_SIZE + 1);
            }

            // populate child aliases
            rc = get_db_command_aliases(conn, sub_cmd);
            if (rc != SQLITE_OK) {
                goto done;
            }

            // populate child args
            rc = get_db_command_args(conn, sub_cmd);
            if (rc != SQLITE_OK) {
                goto done;
            }

            // populate child sub-cmds
            rc = get_db_sub_commands(conn, sub_cmd);
            if (rc != SQLITE_OK) {
                goto done;
            }

            // add this sub_cmd to the parent
            ll_append_item(parent_cmd->sub_commands, sub_cmd);

            step = sqlite3_step(stmt);
        }
    }
    done:
    sqlite3_finalize(stmt);

    return rc;
}

int get_db_command_args(sqlite3 *conn, completion_command_t *parent_cmd) {
    if (parent_cmd == NULL) {
        return -1;
    }

    // ensure cmd->args is fresh
    ll_destroy(&parent_cmd->args);
    ll_free_node_func free_arg = (ll_free_node_func) &free_completion_command_arg;
    parent_cmd->args = ll_create(free_arg);

    char *command_uuid = parent_cmd->uuid;
    sqlite3_stmt *stmt;
    // try to find the command by name
    int rc = sqlite3_prepare(conn, COMPLETION_COMMAND_ARG_SQL, -1, &stmt, 0);
    if (rc == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, command_uuid, -1, NULL);
        int step = sqlite3_step(stmt);
        while (step == SQLITE_ROW) {
            completion_command_arg_t *arg = create_completion_command_arg();
            // ca.uuid, ca.cmd_uuid, ca.arg_type, ca.long_name, ca.short_name
            strncat(arg->uuid, (const char *) sqlite3_column_text(stmt, 0), UUID_FIELD_SIZE);
            strncat(arg->cmd_uuid, (const char *) sqlite3_column_text(stmt, 1), UUID_FIELD_SIZE);
            strncat(arg->arg_type, (const char *) sqlite3_column_text(stmt, 2), CMD_TYPE_FIELD_SIZE);
            strncat(arg->description, (const char *) sqlite3_column_text(stmt, 3), NAME_FIELD_SIZE);
            if (sqlite3_column_type(stmt, 4) == SQLITE_TEXT) {
                strncat(arg->long_name, (const char *) sqlite3_column_text(stmt, 4), NAME_FIELD_SIZE);
            } else {
                memset(arg->long_name, 0, NAME_FIELD_SIZE + 1);
            }
            if (sqlite3_column_type(stmt, 5) == SQLITE_TEXT) {
                strncat(arg->short_name, (const char *) sqlite3_column_text(stmt, 5), SHORTNAME_FIELD_SIZE);
            } else {
                memset(arg->short_name, 0, SHORTNAME_FIELD_SIZE + 1);
            }
            rc = get_db_command_opts(conn, arg);

            ll_append_item(parent_cmd->args, arg);

            step = sqlite3_step(stmt);
        }
    }
    sqlite3_finalize(stmt);

    return rc;
}

int get_db_command_opts(struct sqlite3 *conn, completion_command_arg_t *parent_arg) {
    if (parent_arg == NULL) {
        return -1;
    }

    // ensure arg->opts is fresh
    ll_destroy(&parent_arg->opts);
    ll_free_node_func free_opt = (ll_free_node_func) &free_completion_command_opt;
    parent_arg->opts = ll_create(free_opt);

    char *arg_uuid = parent_arg->uuid;
    sqlite3_stmt *stmt;
    // try to find the command by name
    int rc = sqlite3_prepare(conn, COMPLETION_COMMAND_OPT_SQL, -1, &stmt, 0);
    if (rc == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, arg_uuid, -1, NULL);
        int step = sqlite3_step(stmt);
        while (step == SQLITE_ROW) {
            completion_command_opt_t *opt = create_completion_command_opt();
            // co.uuid, co.cmd_arg_uuid, co.name
            strncat(opt->uuid, (const char *) sqlite3_column_text(stmt, 0), UUID_FIELD_SIZE);
            strncat(opt->cmd_arg_uuid, (const char *) sqlite3_column_text(stmt, 1), UUID_FIELD_SIZE);
            strncat(opt->name, (const char *) sqlite3_column_text(stmt, 2), NAME_FIELD_SIZE);
            ll_append_item(parent_arg->opts, opt);

            step = sqlite3_step(stmt);
        }
    }
    sqlite3_finalize(stmt);

    return rc;
}

completion_command_t* create_completion_command(void) {
    completion_command_t *cmd = malloc(sizeof(completion_command_t));
    if (cmd) {
        memset(cmd->uuid, 0, UUID_FIELD_SIZE + 1);
        memset(cmd->parent_cmd_uuid, 0, UUID_FIELD_SIZE + 1);
        memset(cmd->name, 0, NAME_FIELD_SIZE + 1);

        ll_free_node_func free_command = (ll_free_node_func) &free_completion_command;
        ll_free_node_func free_arg = (ll_free_node_func) &free_completion_command_arg;

        cmd->aliases = ll_create(NULL);
        cmd->sub_commands = ll_create(free_command);
        cmd->args = ll_create(free_arg);
        cmd->is_present_on_cmdline = false;
    }
    return cmd;
}

completion_command_arg_t* create_completion_command_arg(void) {
    completion_command_arg_t *arg = malloc(sizeof(completion_command_arg_t));
    if (arg) {
        memset(arg->uuid, 0, UUID_FIELD_SIZE + 1);
        memset(arg->cmd_uuid, 0, UUID_FIELD_SIZE + 1);
        memset(arg->arg_type, 0, CMD_TYPE_FIELD_SIZE + 1);
        memset(arg->description, 0, NAME_FIELD_SIZE + 1);
        memset(arg->long_name, 0, NAME_FIELD_SIZE + 1);
        memset(arg->short_name, 0, SHORTNAME_FIELD_SIZE + 1);
        arg->is_present_on_cmdline = false;

        ll_free_node_func free_opt = (ll_free_node_func) free_completion_command_opt;
        arg->opts = ll_create(free_opt);
    }
    return arg;
}

completion_command_opt_t* create_completion_command_opt(void) {
    completion_command_opt_t *opt = malloc(sizeof(completion_command_opt_t));
    if (opt) {
        memset(opt->uuid, 0, UUID_FIELD_SIZE + 1);
        memset(opt->name, 0, NAME_FIELD_SIZE + 1);
        memset(opt->cmd_arg_uuid, 0, UUID_FIELD_SIZE + 1);
    }
    return opt;
}

void free_completion_command(completion_command_t **ppcmd) {
    if (!ppcmd) {
        return;
    }
    completion_command_t *cmd = *ppcmd;
    if (!cmd) {
        return;
    }

    // free dynamic internals
    ll_destroy(&cmd->aliases);
    ll_destroy(&cmd->sub_commands);
    ll_destroy(&cmd->args);

    free(cmd);
    *ppcmd = NULL;
}

void free_completion_command_arg(completion_command_arg_t **pparg) {
    if (!pparg) {
        return;
    }
    completion_command_arg_t *arg = *pparg;
    if (!arg) {
        return;
    }

    ll_destroy(&arg->opts);

    free(arg);
    *pparg = NULL;
}

void free_completion_command_opt(completion_command_opt_t **ppopt) {
    if (!ppopt) {
        return;
    }
    completion_command_opt_t *opt = *ppopt;
    if (!opt) {
        return;
    }

    // no internal data to free here

    free(opt);
    *ppopt = NULL;
}
