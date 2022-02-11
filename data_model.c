#include "data_model.h"
#include "linked_list.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sqlite3.h>

// SQL statements used for BASH completion
static const char* COMMAND_READ_SQL =
        " SELECT c.uuid, c.name, c.parent_cmd "
        " FROM command c "
        " JOIN command_alias a ON a.cmd_uuid = c.uuid "
        " WHERE c.name = ?1 OR a.name = ?2 ";

static const char* COMMAND_ALIAS_READ_SQL =
        " SELECT a.uuid, a.cmd_uuid, a.name "
        " FROM command_alias a "
        " WHERE a.cmd_uuid = ?1 ";

static const char* SUB_COMMAND_READ_SQL =
        " SELECT c.uuid, c.name, c.parent_cmd "
        " FROM command c "
        " WHERE c.parent_cmd = ?1 "
        " ORDER BY c.name ";

static const char* COMMAND_ARG_READ_SQL =
        " SELECT ca.uuid, ca.cmd_uuid, ca.arg_type, ca.description, ca.long_name, ca.short_name "
        " FROM command_arg ca "
        " JOIN command c ON c.uuid = ca.cmd_uuid "
        " WHERE c.uuid = ?1 "
        " ORDER BY ca.long_name, ca.short_name ";

static const char* COMMAND_OPT_READ_SQL =
        " SELECT co.uuid, co.cmd_arg_uuid, co.name "
        " FROM command_opt co "
        " JOIN command_arg ca ON ca.uuid = co.cmd_arg_uuid "
        " WHERE ca.uuid = ?1 "
        " ORDER BY co.name ";

// SQL statements used for IMPORT/EXPORT
static const char* COMMAND_NAMES_SQL =
        " SELECT c.name "
        " FROM command c "
        " WHERE c.parent_cmd IS NULL "
        " ORDER BY c.name ";

static const char* COMMAND_WRITE_SQL =
        " INSERT INTO command "
        " (uuid, name, parent_cmd) "
        " VALUES "
        " (?1, ?2, ?3) ";

static const char* COMMAND_ALIAS_WRITE_SQL =
        " INSERT INTO command_alias "
        " (uuid, cmd_uuid, name) "
        " VALUES "
        " (?1, ?2, ?3) ";

static const char* COMMAND_ARG_WRITE_SQL =
        " INSERT INTO command_arg "
        " (uuid, cmd_uuid, arg_type, description, long_name, short_name) "
        " VALUES "
        " (?1, ?2, ?3, ?4, ?5, ?6) ";

static const char* COMMAND_OPT_WRITE_SQL =
        " INSERT INTO command_opt "
        " (uuid, cmd_arg_uuid, name) "
        " VALUES "
        " (?1, ?2, ?3) ";

// DB schema should perform cascade deletes
static const char* COMMAND_DELETE_SQL =
        " DELETE FROM command "
        " WHERE name = ?1 "
        " AND parent_cmd IS NULL ";

// cache statements that are used multiple times (prevent extra parse SQL operations)
static sqlite3_stmt *bce_command_read_stmt;
static sqlite3_stmt *bce_command_alias_read_stmt;
static sqlite3_stmt *bce_sub_command_read_stmt;
static sqlite3_stmt *bce_command_arg_read_stmt;
static sqlite3_stmt *bce_command_opt_read_stmt;

int prepare_statement_cache(struct sqlite3 *conn) {
    int rc;
    unsigned int prep_flags = SQLITE_PREPARE_PERSISTENT;

    rc = sqlite3_prepare_v3(conn, COMMAND_READ_SQL, -1, prep_flags, &bce_command_read_stmt, NULL);
    if (rc != SQLITE_OK) {
        goto done;
    }
    rc = sqlite3_prepare_v3(conn, COMMAND_ALIAS_READ_SQL, -1, prep_flags, &bce_command_alias_read_stmt, NULL);
    if (rc != SQLITE_OK) {
        goto done;
    }
    rc = sqlite3_prepare_v3(conn, SUB_COMMAND_READ_SQL, -1, prep_flags, &bce_sub_command_read_stmt, NULL);
    if (rc != SQLITE_OK) {
        goto done;
    }
    rc = sqlite3_prepare_v3(conn, COMMAND_ARG_READ_SQL, -1, prep_flags, &bce_command_arg_read_stmt, NULL);
    if (rc != SQLITE_OK) {
        goto done;
    }
    rc = sqlite3_prepare_v3(conn, COMMAND_OPT_READ_SQL, -1, prep_flags, &bce_command_opt_read_stmt, NULL);
    if (rc != SQLITE_OK) {
        goto done;
    }

    done:
    return rc;
}

int free_statement_cache(struct sqlite3 *conn) {
    int rc;

    if (bce_command_read_stmt) {
        rc = sqlite3_finalize(bce_command_read_stmt);
        bce_command_read_stmt = NULL;
    }
    if (bce_command_alias_read_stmt) {
        rc = sqlite3_finalize(bce_command_alias_read_stmt);
        bce_command_alias_read_stmt = NULL;
    }
    if (bce_sub_command_read_stmt) {
        rc = sqlite3_finalize(bce_sub_command_read_stmt);
        bce_sub_command_read_stmt = NULL;
    }
    if (bce_command_arg_read_stmt) {
        rc = sqlite3_finalize(bce_command_arg_read_stmt);
        bce_command_arg_read_stmt = NULL;
    }
    if (bce_command_opt_read_stmt) {
        rc = sqlite3_finalize(bce_command_opt_read_stmt);
        bce_command_opt_read_stmt = NULL;
    }

    return rc;
}

void print_command_tree(struct sqlite3 *conn, const bce_command_t *cmd, const int level) {
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
            bce_command_alias_t *alias = (bce_command_alias_t *)alias_node->data;
            printf("%s ", alias->name);
            alias_node = alias_node->next;
        }
        printf("\n");
    }

    if (cmd->args) {
        linked_list_node_t *arg_node = cmd->args->head;
        while (arg_node) {
            bce_command_arg_t *arg = (bce_command_arg_t *)arg_node->data;
            if (arg) {
                for (int i = 0; i < level; i++) {
                    printf("  ");
                }
                printf("  arg: %s (%s): %s\n", arg->long_name, arg->short_name, arg->arg_type);

                // print opts
                if (arg->opts) {
                    linked_list_node_t *opt_node = arg->opts->head;
                    while (opt_node) {
                        bce_command_opt_t *opt = (bce_command_opt_t *)opt_node->data;
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
            bce_command_t *sub_cmd = (bce_command_t *) node->data;
            print_command_tree(conn, sub_cmd, level + 1);
            node = node->next;
        }
    }
}

int get_db_command_names(struct sqlite3 *conn, linked_list_t *cmd_names) {
    if (!conn || !cmd_names) {
        return 1;   // TODO: fix this
    }

    // try to find the command by name
    sqlite3_stmt *stmt;
    // try to find the command by name
    int rc = sqlite3_prepare(conn, COMMAND_NAMES_SQL, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        int step = sqlite3_step(bce_command_read_stmt);
        while (step == SQLITE_ROW) {
            char *cmd_name = calloc(NAME_FIELD_SIZE + 1, sizeof(char));
            strncat(cmd_name, (const char *) sqlite3_column_text(stmt, 0), NAME_FIELD_SIZE);
            ll_append_item(cmd_names, cmd_name);
            step = sqlite3_step(stmt);
        }
    }

    return rc;
}

int get_db_command(struct sqlite3 *conn, bce_command_t *cmd, const char* command_name) {
    int rc;

    memset(cmd->uuid, 0, UUID_FIELD_SIZE + 1);
    memset(cmd->name, 0, NAME_FIELD_SIZE + 1);
    memset(cmd->parent_cmd_uuid, 0, UUID_FIELD_SIZE + 1);
    cmd->aliases = NULL;
    cmd->sub_commands = NULL;
    cmd->args = NULL;

    // pull statement from cache
    sqlite3_stmt *stmt = bce_command_read_stmt;
    rc = sqlite3_reset(stmt);

    // try to find the command by name
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
        ll_free_node_func free_command = (ll_free_node_func) &free_bce_command;
        ll_free_node_func free_alias = (ll_free_node_func) &free_bce_command_alias;
        ll_free_node_func free_arg = (ll_free_node_func) &free_bce_command_arg;

        cmd->aliases = ll_create(free_alias);
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

done:
    return rc;
}

int get_db_command_aliases(struct sqlite3 *conn, bce_command_t *parent_cmd) {
    int rc;

    // pull statement from cache
    sqlite3_stmt *stmt = bce_command_alias_read_stmt;
    rc = sqlite3_reset(stmt);

    sqlite3_bind_text(stmt, 1, parent_cmd->uuid, -1, NULL);
    int step = sqlite3_step(stmt);
    while (step == SQLITE_ROW) {
        bce_command_alias_t *alias = create_bce_command_alias();
        strncat(alias->uuid, (const char *)sqlite3_column_text(stmt, 0), UUID_FIELD_SIZE);
        strncat(alias->cmd_uuid, (const char *)sqlite3_column_text(stmt, 1), UUID_FIELD_SIZE);
        strncat(alias->name, (const char *)sqlite3_column_text(stmt, 2), NAME_FIELD_SIZE);

        // add this alias to the parent
        ll_append_item(parent_cmd->aliases, alias);

        step = sqlite3_step(stmt);
    }

    return rc;
}

int get_db_sub_commands(struct sqlite3 *conn, bce_command_t *parent_cmd) {
    // pull statement from cache
    sqlite3_stmt *stmt = bce_sub_command_read_stmt;
    int rc = sqlite3_reset(stmt);

    sqlite3_bind_text(stmt, 1, parent_cmd->uuid, -1, NULL);
    int step = sqlite3_step(stmt);
    while (step == SQLITE_ROW) {
        // create bce_command_t
        bce_command_t *sub_cmd = create_bce_command();
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

    done:
    return rc;
}

int get_db_command_args(sqlite3 *conn, bce_command_t *parent_cmd) {
    if (parent_cmd == NULL) {
        return -1;
    }

    // ensure cmd->args is fresh
    ll_destroy(&parent_cmd->args);
    ll_free_node_func free_arg = (ll_free_node_func) &free_bce_command_arg;
    parent_cmd->args = ll_create(free_arg);

    char *command_uuid = parent_cmd->uuid;

    // pull statement from cache
    sqlite3_stmt *stmt = bce_command_arg_read_stmt;
    int rc = sqlite3_reset(stmt);

    sqlite3_bind_text(stmt, 1, command_uuid, -1, NULL);
    int step = sqlite3_step(stmt);
    while (step == SQLITE_ROW) {
        bce_command_arg_t *arg = create_bce_command_arg();
        // ca.uuid, ca.cmd_uuid, ca.arg_type, ca.long_name, ca.short_name
        strncat(arg->uuid, (const char *) sqlite3_column_text(stmt, 0), UUID_FIELD_SIZE);
        strncat(arg->cmd_uuid, (const char *) sqlite3_column_text(stmt, 1), UUID_FIELD_SIZE);
        strncat(arg->arg_type, (const char *) sqlite3_column_text(stmt, 2), CMD_TYPE_FIELD_SIZE);
        if (sqlite3_column_type(stmt, 4) == SQLITE_TEXT) {
            strncat(arg->description, (const char *) sqlite3_column_text(stmt, 3), DESCRIPTION_FIELD_SIZE);
        } else {
            memset(arg->description, 0, DESCRIPTION_FIELD_SIZE + 1);
        }
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

    return rc;
}

int get_db_command_opts(struct sqlite3 *conn, bce_command_arg_t *parent_arg) {
    if (parent_arg == NULL) {
        return -1;
    }

    // ensure arg->opts is fresh
    ll_destroy(&parent_arg->opts);
    ll_free_node_func free_opt = (ll_free_node_func) &free_bce_command_opt;
    parent_arg->opts = ll_create(free_opt);

    // pull statement from cache
    sqlite3_stmt *stmt = bce_command_opt_read_stmt;
    int rc = sqlite3_reset(stmt);

    char *arg_uuid = parent_arg->uuid;
    sqlite3_bind_text(stmt, 1, arg_uuid, -1, NULL);
    int step = sqlite3_step(stmt);
    while (step == SQLITE_ROW) {
        bce_command_opt_t *opt = create_bce_command_opt();
        // co.uuid, co.cmd_arg_uuid, co.name
        strncat(opt->uuid, (const char *) sqlite3_column_text(stmt, 0), UUID_FIELD_SIZE);
        strncat(opt->cmd_arg_uuid, (const char *) sqlite3_column_text(stmt, 1), UUID_FIELD_SIZE);
        strncat(opt->name, (const char *) sqlite3_column_text(stmt, 2), NAME_FIELD_SIZE);
        ll_append_item(parent_arg->opts, opt);

        step = sqlite3_step(stmt);
    }

    return rc;
}

bce_command_t* create_bce_command(void) {
    bce_command_t *cmd = malloc(sizeof(bce_command_t));
    if (cmd) {
        memset(cmd->uuid, 0, UUID_FIELD_SIZE + 1);
        memset(cmd->parent_cmd_uuid, 0, UUID_FIELD_SIZE + 1);
        memset(cmd->name, 0, NAME_FIELD_SIZE + 1);

        ll_free_node_func free_command = (ll_free_node_func) &free_bce_command;
        ll_free_node_func free_alias = (ll_free_node_func) &free_bce_command_alias;
        ll_free_node_func free_arg = (ll_free_node_func) &free_bce_command_arg;

        cmd->aliases = ll_create(free_alias);
        cmd->sub_commands = ll_create(free_command);
        cmd->args = ll_create(free_arg);
        cmd->is_present_on_cmdline = false;
    }
    return cmd;
}

bce_command_alias_t* create_bce_command_alias(void) {
    bce_command_alias_t *alias = malloc(sizeof(bce_command_alias_t));
    if (alias) {
        memset(alias->uuid, 0, UUID_FIELD_SIZE + 1);
        memset(alias->cmd_uuid, 0, UUID_FIELD_SIZE + 1);
        memset(alias->name, 0, NAME_FIELD_SIZE + 1);
    }
    return alias;
}

bce_command_arg_t* create_bce_command_arg(void) {
    bce_command_arg_t *arg = malloc(sizeof(bce_command_arg_t));
    if (arg) {
        memset(arg->uuid, 0, UUID_FIELD_SIZE + 1);
        memset(arg->cmd_uuid, 0, UUID_FIELD_SIZE + 1);
        memset(arg->arg_type, 0, CMD_TYPE_FIELD_SIZE + 1);
        memset(arg->description, 0, DESCRIPTION_FIELD_SIZE + 1);
        memset(arg->long_name, 0, NAME_FIELD_SIZE + 1);
        memset(arg->short_name, 0, SHORTNAME_FIELD_SIZE + 1);
        arg->is_present_on_cmdline = false;

        ll_free_node_func free_opt = (ll_free_node_func) free_bce_command_opt;
        arg->opts = ll_create(free_opt);
    }
    return arg;
}

bce_command_opt_t* create_bce_command_opt(void) {
    bce_command_opt_t *opt = malloc(sizeof(bce_command_opt_t));
    if (opt) {
        memset(opt->uuid, 0, UUID_FIELD_SIZE + 1);
        memset(opt->name, 0, NAME_FIELD_SIZE + 1);
        memset(opt->cmd_arg_uuid, 0, UUID_FIELD_SIZE + 1);
    }
    return opt;
}

void free_bce_command(bce_command_t **ppcmd) {
    if (!ppcmd) {
        return;
    }
    bce_command_t *cmd = *ppcmd;
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

void free_bce_command_alias(bce_command_alias_t **ppalias) {
    if (!ppalias) {
        return;
    }
    bce_command_alias_t *alias = *ppalias;
    if (!alias) {
        return;
    }

    free(alias);
    *ppalias = NULL;
}

void free_bce_command_arg(bce_command_arg_t **pparg) {
    if (!pparg) {
        return;
    }
    bce_command_arg_t *arg = *pparg;
    if (!arg) {
        return;
    }

    ll_destroy(&arg->opts);

    free(arg);
    *pparg = NULL;
}

void free_bce_command_opt(bce_command_opt_t **ppopt) {
    if (!ppopt) {
        return;
    }
    bce_command_opt_t *opt = *ppopt;
    if (!opt) {
        return;
    }

    // no internal data to free here

    free(opt);
    *ppopt = NULL;
}

int write_db_command(struct sqlite3 *conn, bce_command_t *cmd) {
    if (!cmd) {
        return 1;   // TODO: fix this
    }

    sqlite3_stmt *stmt;
    int rc;

    // insert the command
    rc = sqlite3_prepare(conn, COMMAND_WRITE_SQL, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        goto done;
    }
    sqlite3_bind_text(stmt, 1, cmd->uuid, -1, NULL);
    sqlite3_bind_text(stmt, 2, cmd->name, -1, NULL);
    if (strlen(cmd->parent_cmd_uuid) > 0) {
        sqlite3_bind_text(stmt, 3, cmd->parent_cmd_uuid, -1, NULL);
    } else {
        sqlite3_bind_null(stmt, 3);
    }
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        goto done;
    }

    // insert the aliases
    if (cmd->aliases) {
        linked_list_node_t *node = cmd->aliases->head;
        while (node) {
            bce_command_alias_t *alias = (bce_command_alias_t *)node->data;
            if (alias) {
                rc = write_db_command_alias(conn, alias);
                if (rc != SQLITE_OK) {
                    goto done;
                }
            }
            node = node->next;
        }
    }

    // insert each sub-command
    if (cmd->sub_commands) {
        linked_list_node_t *node = cmd->sub_commands->head;
        while (node) {
            bce_command_t *subcmd = (bce_command_t *)node->data;
            if (subcmd) {
                rc = write_db_command(conn, subcmd);
                if (rc != SQLITE_OK) {
                    goto done;
                }
            }
            node = node->next;
        }
    }

    // insert each of the command_args
    if (cmd->args) {
        linked_list_node_t *node = cmd->args->head;
        while (node) {
            bce_command_arg_t *arg = (bce_command_arg_t *)node->data;
            if (arg) {
                rc = write_db_command_arg(conn, arg);
                if (rc != SQLITE_OK) {
                    goto done;
                }
            }
            node = node->next;
        }
    }

    done:
    sqlite3_finalize(stmt);
    if (rc == SQLITE_DONE) {
        rc = SQLITE_OK;
    }
    return rc;
}

int write_db_command_alias(struct sqlite3 *conn, bce_command_alias_t *alias) {
    if (!alias) {
        return 1;
    }

    sqlite3_stmt *stmt;
    int rc;

    // insert the alias
    rc = sqlite3_prepare(conn, COMMAND_ALIAS_WRITE_SQL, -1, &stmt, NULL);
    // uuid, cmd_uuid, arg_type, description, long_name, short_name
    if (rc != SQLITE_OK) {
        goto done;
    }
    sqlite3_bind_text(stmt, 1, alias->uuid, -1, NULL);
    sqlite3_bind_text(stmt, 2, alias->cmd_uuid, -1, NULL);
    sqlite3_bind_text(stmt, 3, alias->name, -1, NULL);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        goto done;
    }

    done:
    sqlite3_finalize(stmt);
    if (rc == SQLITE_DONE) {
        rc = SQLITE_OK;
    }
    return rc;
}

int write_db_command_arg(struct sqlite3 *conn, bce_command_arg_t *arg) {
    if (!arg) {
        return 1;   // TODO: fix this
    }

    sqlite3_stmt *stmt;
    int rc;

    // insert the arg
    rc = sqlite3_prepare(conn, COMMAND_ARG_WRITE_SQL, -1, &stmt, NULL);
    // uuid, cmd_uuid, arg_type, description, long_name, short_name
    if (rc != SQLITE_OK) {
        goto done;
    }
    sqlite3_bind_text(stmt, 1, arg->uuid, -1, NULL);
    sqlite3_bind_text(stmt, 2, arg->cmd_uuid, -1, NULL);
    sqlite3_bind_text(stmt, 3, arg->arg_type, -1, NULL);
    if (strlen(arg->description) > 0) {
        sqlite3_bind_text(stmt, 4, arg->description, -1, NULL);
    } else {
        sqlite3_bind_null(stmt, 4);
    }
    if (strlen(arg->long_name) > 0) {
        sqlite3_bind_text(stmt, 5, arg->long_name, -1, NULL);
    } else {
        sqlite3_bind_null(stmt, 5);
    }
    if (strlen(arg->short_name) > 0) {
        sqlite3_bind_text(stmt, 6, arg->short_name, -1, NULL);
    } else {
        sqlite3_bind_null(stmt, 6);
    }
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        goto done;
    }

    // write each of the opts
    if (arg->opts) {
        linked_list_node_t *node = arg->opts->head;
        while (node) {
            bce_command_opt_t *opt = (bce_command_opt_t *)node->data;
            if (opt) {
                rc = write_db_command_opt(conn, opt);
                if (rc != SQLITE_OK) {
                    goto done;
                }
            }
            node = node->next;
        }
    }

    done:
    sqlite3_finalize(stmt);
    if (rc == SQLITE_DONE) {
        rc = SQLITE_OK;
    }
    return rc;
}

int write_db_command_opt(struct sqlite3 *conn, bce_command_opt_t *opt) {
    if (!opt) {
        return 1;   // TODO: fix this
    }

    sqlite3_stmt *stmt;
    int rc;

    // insert the opt
    rc = sqlite3_prepare(conn, COMMAND_OPT_WRITE_SQL, -1, &stmt, NULL);
    // uuid, cmd_arg_uuid, name
    if (rc != SQLITE_OK) {
        goto done;
    }
    sqlite3_bind_text(stmt, 1, opt->uuid, -1, NULL);
    sqlite3_bind_text(stmt, 2, opt->cmd_arg_uuid, -1, NULL);
    sqlite3_bind_text(stmt, 3, opt->name, -1, NULL);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        goto done;
    }

    done:
    sqlite3_finalize(stmt);
    if (rc == SQLITE_DONE) {
        rc = SQLITE_OK;
    }
    return rc;
}

int delete_db_command(struct sqlite3 *conn, const char *command_name) {
    if (!conn || !command_name) {
        return 1;   // TODO: fix this
    }

    sqlite3_stmt *stmt;
    int rc;

    // delete the command (CASCADE should happen for all FKs)
    rc = sqlite3_prepare(conn, COMMAND_DELETE_SQL, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        goto done;
    }
    sqlite3_bind_text(stmt, 1, command_name, -1, NULL);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        goto done;
    }

done:
    sqlite3_finalize(stmt);
    if (rc == SQLITE_DONE) {
        rc = SQLITE_OK;
    }
    return rc;
}