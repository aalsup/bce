#include "data_model.h"
#include "linked_list.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sqlite3.h>

// SQL statements used for BASH completion
static const char *COMMAND_READ_SQL =
        " SELECT c.uuid, c.name, c.parent_cmd "
        " FROM command c "
        " JOIN command_alias a ON a.cmd_uuid = c.uuid "
        " WHERE c.name = ?1 OR a.name = ?2 ";

static const char *COMMAND_ALIAS_READ_SQL =
        " SELECT a.uuid, a.cmd_uuid, a.name "
        " FROM command_alias a "
        " WHERE a.cmd_uuid = ?1 ";

static const char *SUB_COMMAND_READ_SQL =
        " SELECT c.uuid, c.name, c.parent_cmd "
        " FROM command c "
        " WHERE c.parent_cmd = ?1 "
        " ORDER BY c.name ";

static const char *COMMAND_ARG_READ_SQL =
        " SELECT ca.uuid, ca.cmd_uuid, ca.arg_type, ca.description, ca.long_name, ca.short_name "
        " FROM command_arg ca "
        " JOIN command c ON c.uuid = ca.cmd_uuid "
        " WHERE c.uuid = ?1 "
        " ORDER BY ca.long_name, ca.short_name ";

static const char *COMMAND_OPT_READ_SQL =
        " SELECT co.uuid, co.cmd_arg_uuid, co.name "
        " FROM command_opt co "
        " JOIN command_arg ca ON ca.uuid = co.cmd_arg_uuid "
        " WHERE ca.uuid = ?1 "
        " ORDER BY co.name ";

// SQL statements used for IMPORT/EXPORT
static const char *ROOT_COMMAND_NAMES_SQL =
        " SELECT c.name "
        " FROM command c "
        " WHERE c.parent_cmd IS NULL "
        " ORDER BY c.name ";

static const char *COMMAND_WRITE_SQL =
        " INSERT INTO command "
        " (uuid, name, parent_cmd) "
        " VALUES "
        " (?1, ?2, ?3) ";

static const char *COMMAND_ALIAS_WRITE_SQL =
        " INSERT INTO command_alias "
        " (uuid, cmd_uuid, name) "
        " VALUES "
        " (?1, ?2, ?3) ";

static const char *COMMAND_ARG_WRITE_SQL =
        " INSERT INTO command_arg "
        " (uuid, cmd_uuid, arg_type, description, long_name, short_name) "
        " VALUES "
        " (?1, ?2, ?3, ?4, ?5, ?6) ";

static const char *COMMAND_OPT_WRITE_SQL =
        " INSERT INTO command_opt "
        " (uuid, cmd_arg_uuid, name) "
        " VALUES "
        " (?1, ?2, ?3) ";

// DB schema should perform cascade deletes
static const char *COMMAND_DELETE_SQL =
        " DELETE FROM command "
        " WHERE name = ?1 "
        " AND parent_cmd IS NULL ";

bce_error_t db_query_root_command_names(struct sqlite3 *conn, linked_list_t *cmd_names) {
    bce_error_t err = ERR_NONE;

    if (!conn) {
        return ERR_NO_DATABASE_CONNECTION;
    }
    if (!cmd_names) {
        return ERR_INVALID_CMD_NAME;
    }

    // try to find the command by name
    sqlite3_stmt *stmt;
    // try to find the command by name
    int rc = sqlite3_prepare_v3(conn, ROOT_COMMAND_NAMES_SQL, -1, 0, &stmt, NULL);
    if (rc == SQLITE_OK) {
        int step = sqlite3_step(stmt);
        while (step == SQLITE_ROW) {
            char *cmd_name = calloc(NAME_FIELD_SIZE + 1, sizeof(char));
            strncat(cmd_name, (const char *) sqlite3_column_text(stmt, 0), NAME_FIELD_SIZE);
            ll_append_item(cmd_names, cmd_name);
            step = sqlite3_step(stmt);
        }
    }

    if (rc == SQLITE_OK) {
        err = ERR_NONE;
    } else {
        err = ERR_SQLITE_ERROR;
    }
    return err;
}

bce_error_t db_query_command(struct sqlite3 *conn, bce_command_t *cmd, const char *command_name) {
    int rc;
    bce_error_t err = ERR_NONE;
    unsigned int prep_flags = SQLITE_PREPARE_PERSISTENT;

    memset(cmd->uuid, 0, UUID_FIELD_SIZE + 1);
    memset(cmd->name, 0, NAME_FIELD_SIZE + 1);
    memset(cmd->parent_cmd_uuid, 0, UUID_FIELD_SIZE + 1);
    cmd->aliases = NULL;
    cmd->sub_commands = NULL;
    cmd->args = NULL;

    // prepare SQL statement
    sqlite3_stmt *stmt;
    rc = sqlite3_prepare_v3(conn, COMMAND_READ_SQL, -1, prep_flags, &stmt, NULL);
    if (rc != SQLITE_OK) {
        goto done;
    }

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
        ll_free_node_func free_command = (ll_free_node_func) &bce_command_free;
        ll_free_node_func free_alias = (ll_free_node_func) &bce_command_alias_free;
        ll_free_node_func free_arg = (ll_free_node_func) &bce_command_arg_free;

        cmd->aliases = ll_create(free_alias);
        cmd->sub_commands = ll_create(free_command);
        cmd->args = ll_create(free_arg);

        // populate child aliases
        err = db_query_command_aliases(conn, cmd);
        if (err != ERR_NONE) {
            goto done;
        }

        // populate child args
        err = db_query_command_args(conn, cmd);
        if (err != ERR_NONE) {
            goto done;
        }

        // populate child sub-cmds
        err = db_query_sub_commands(conn, cmd);
        if (err != ERR_NONE) {
            goto done;
        }
    }

    done:
    if (stmt) {
        sqlite3_finalize(stmt);
    }
    if (err == ERR_NONE) {
        if (rc != SQLITE_OK) {
            err = ERR_SQLITE_ERROR;
        }
    }
    return err;
}

bce_error_t db_query_command_aliases(struct sqlite3 *conn, bce_command_t *parent_cmd) {
    int rc;
    bce_error_t err = ERR_NONE;
    unsigned int prep_flags = SQLITE_PREPARE_PERSISTENT;

    // prepare SQL statement
    sqlite3_stmt *stmt;
    rc = sqlite3_prepare_v3(conn, COMMAND_ALIAS_READ_SQL, -1, prep_flags, &stmt, NULL);
    if (rc != SQLITE_OK) {
        goto done;
    }

    sqlite3_bind_text(stmt, 1, parent_cmd->uuid, -1, NULL);
    int step = sqlite3_step(stmt);
    while (step == SQLITE_ROW) {
        bce_command_alias_t *alias = bce_command_alias_new();
        strncat(alias->uuid, (const char *) sqlite3_column_text(stmt, 0), UUID_FIELD_SIZE);
        strncat(alias->cmd_uuid, (const char *) sqlite3_column_text(stmt, 1), UUID_FIELD_SIZE);
        strncat(alias->name, (const char *) sqlite3_column_text(stmt, 2), NAME_FIELD_SIZE);

        // add this alias to the parent
        ll_append_item(parent_cmd->aliases, alias);

        step = sqlite3_step(stmt);
    }

    done:
    if (stmt) {
        sqlite3_finalize(stmt);
    }
    if (err == ERR_NONE) {
        if ((rc == SQLITE_OK) && (step == SQLITE_DONE)) {
            err = ERR_NONE;
        } else {
            err = ERR_SQLITE_ERROR;
        }
    }
    return err;
}

bce_error_t db_query_sub_commands(struct sqlite3 *conn, bce_command_t *parent_cmd) {
    int rc;
    bce_error_t err = ERR_NONE;
    unsigned int prep_flags = SQLITE_PREPARE_PERSISTENT;

    // prepare SQL statement
    sqlite3_stmt *stmt;
    rc = sqlite3_prepare_v3(conn, SUB_COMMAND_READ_SQL, -1, prep_flags, &stmt, NULL);
    if (rc != SQLITE_OK) {
        goto done;
    }

    sqlite3_bind_text(stmt, 1, parent_cmd->uuid, -1, NULL);
    int step = sqlite3_step(stmt);
    while (step == SQLITE_ROW) {
        // create bce_command_t
        bce_command_t *sub_cmd = bce_command_new();
        strncat(sub_cmd->uuid, (const char *) sqlite3_column_text(stmt, 0), UUID_FIELD_SIZE);
        strncat(sub_cmd->name, (const char *) sqlite3_column_text(stmt, 1), NAME_FIELD_SIZE);
        if (sqlite3_column_type(stmt, 2) == SQLITE_TEXT) {
            strncat(sub_cmd->parent_cmd_uuid, (const char *) sqlite3_column_text(stmt, 2), UUID_FIELD_SIZE);
        } else {
            memset(sub_cmd->parent_cmd_uuid, 0, UUID_FIELD_SIZE + 1);
        }

        // populate child aliases
        err = db_query_command_aliases(conn, sub_cmd);
        if (err != ERR_NONE) {
            goto done;
        }

        // populate child args
        err = db_query_command_args(conn, sub_cmd);
        if (err != ERR_NONE) {
            goto done;
        }

        // populate child sub-cmds
        err = db_query_sub_commands(conn, sub_cmd);
        if (err != ERR_NONE) {
            goto done;
        }

        // add this sub_cmd to the parent
        ll_append_item(parent_cmd->sub_commands, sub_cmd);

        step = sqlite3_step(stmt);
    }

    done:
    if (stmt) {
        sqlite3_finalize(stmt);
    }
    if (err == ERR_NONE) {
        if ((rc == SQLITE_OK) && (step == SQLITE_DONE)) {
            err = ERR_NONE;
        } else {
            err = ERR_SQLITE_ERROR;
        }
    }
    return err;
}

bce_error_t db_query_command_args(sqlite3 *conn, bce_command_t *parent_cmd) {
    if (!conn) {
        return ERR_NO_DATABASE_CONNECTION;
    }
    if (!parent_cmd) {
        return ERR_INVALID_CMD;
    }

    bce_error_t err = ERR_NONE;

    // ensure cmd->args is fresh
    parent_cmd->args = ll_destroy(parent_cmd->args);
    ll_free_node_func free_arg = (ll_free_node_func) &bce_command_arg_free;
    parent_cmd->args = ll_create(free_arg);

    char *command_uuid = parent_cmd->uuid;

    // pull statement from cache
    unsigned int prep_flags = SQLITE_PREPARE_PERSISTENT;
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v3(conn, COMMAND_ARG_READ_SQL, -1, prep_flags, &stmt, NULL);
    if (rc != SQLITE_OK) {
        goto done;
    }

    sqlite3_bind_text(stmt, 1, command_uuid, -1, NULL);
    int step = sqlite3_step(stmt);
    while (step == SQLITE_ROW) {
        bce_command_arg_t *arg = bce_command_arg_new();
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
        rc = db_query_command_opts(conn, arg);

        ll_append_item(parent_cmd->args, arg);

        step = sqlite3_step(stmt);
    }

    done:
    if (stmt) {
        sqlite3_finalize(stmt);
    }
    if (err == ERR_NONE) {
        if ((rc == SQLITE_OK) && (step == SQLITE_DONE)) {
            err = ERR_NONE;
        } else {
            err = ERR_SQLITE_ERROR;
        }
    }
    return err;
}

bce_error_t db_query_command_opts(struct sqlite3 *conn, bce_command_arg_t *parent_arg) {
    if (!conn) {
        return ERR_NO_DATABASE_CONNECTION;
    }
    if (!parent_arg) {
        return ERR_INVALID_ARG;
    }

    bce_error_t err = ERR_NONE;

    // ensure arg->opts is fresh
    parent_arg->opts = ll_destroy(parent_arg->opts);
    ll_free_node_func free_opt = (ll_free_node_func) &bce_command_opt_free;
    parent_arg->opts = ll_create(free_opt);

    // pull statement from cache
    unsigned int prep_flags = SQLITE_PREPARE_PERSISTENT;
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v3(conn, COMMAND_OPT_READ_SQL, -1, prep_flags, &stmt, NULL);
    if (rc != SQLITE_OK) {
        goto done;
    }

    char *arg_uuid = parent_arg->uuid;
    sqlite3_bind_text(stmt, 1, arg_uuid, -1, NULL);
    int step = sqlite3_step(stmt);
    while (step == SQLITE_ROW) {
        bce_command_opt_t *opt = bce_command_opt_new();
        // co.uuid, co.cmd_arg_uuid, co.name
        strncat(opt->uuid, (const char *) sqlite3_column_text(stmt, 0), UUID_FIELD_SIZE);
        strncat(opt->cmd_arg_uuid, (const char *) sqlite3_column_text(stmt, 1), UUID_FIELD_SIZE);
        strncat(opt->name, (const char *) sqlite3_column_text(stmt, 2), NAME_FIELD_SIZE);
        ll_append_item(parent_arg->opts, opt);

        step = sqlite3_step(stmt);
    }

    done:
    if (stmt) {
        sqlite3_finalize(stmt);
    }
    if (err == ERR_NONE) {
        if ((rc == SQLITE_OK) && (step == SQLITE_DONE)) {
            err = ERR_NONE;
        } else {
            err = ERR_SQLITE_ERROR;
        }
    }
    return err;
}

bce_command_t *bce_command_new(void) {
    bce_command_t *cmd = malloc(sizeof(bce_command_t));
    if (cmd) {
        memset(cmd->uuid, 0, UUID_FIELD_SIZE + 1);
        memset(cmd->parent_cmd_uuid, 0, UUID_FIELD_SIZE + 1);
        memset(cmd->name, 0, NAME_FIELD_SIZE + 1);

        ll_free_node_func free_command = (ll_free_node_func) &bce_command_free;
        ll_free_node_func free_alias = (ll_free_node_func) &bce_command_alias_free;
        ll_free_node_func free_arg = (ll_free_node_func) &bce_command_arg_free;

        cmd->aliases = ll_create(free_alias);
        cmd->sub_commands = ll_create(free_command);
        cmd->args = ll_create(free_arg);
        cmd->is_present_on_cmdline = false;
    }
    return cmd;
}

bce_command_alias_t *bce_command_alias_new(void) {
    bce_command_alias_t *alias = malloc(sizeof(bce_command_alias_t));
    if (alias) {
        memset(alias->uuid, 0, UUID_FIELD_SIZE + 1);
        memset(alias->cmd_uuid, 0, UUID_FIELD_SIZE + 1);
        memset(alias->name, 0, NAME_FIELD_SIZE + 1);
    }
    return alias;
}

bce_command_arg_t *bce_command_arg_new(void) {
    bce_command_arg_t *arg = malloc(sizeof(bce_command_arg_t));
    if (arg) {
        memset(arg->uuid, 0, UUID_FIELD_SIZE + 1);
        memset(arg->cmd_uuid, 0, UUID_FIELD_SIZE + 1);
        memset(arg->arg_type, 0, CMD_TYPE_FIELD_SIZE + 1);
        memset(arg->description, 0, DESCRIPTION_FIELD_SIZE + 1);
        memset(arg->long_name, 0, NAME_FIELD_SIZE + 1);
        memset(arg->short_name, 0, SHORTNAME_FIELD_SIZE + 1);
        arg->is_present_on_cmdline = false;

        ll_free_node_func free_opt = (ll_free_node_func) bce_command_opt_free;
        arg->opts = ll_create(free_opt);
    }
    return arg;
}

bce_command_opt_t *bce_command_opt_new(void) {
    bce_command_opt_t *opt = malloc(sizeof(bce_command_opt_t));
    if (opt) {
        memset(opt->uuid, 0, UUID_FIELD_SIZE + 1);
        memset(opt->name, 0, NAME_FIELD_SIZE + 1);
        memset(opt->cmd_arg_uuid, 0, UUID_FIELD_SIZE + 1);
    }
    return opt;
}

bce_command_t *bce_command_free(bce_command_t *cmd) {
    if (!cmd) {
        return NULL;
    }

    // free dynamic internals
    cmd->aliases = ll_destroy(cmd->aliases);
    cmd->sub_commands = ll_destroy(cmd->sub_commands);
    cmd->args = ll_destroy(cmd->args);

    free(cmd);
    return NULL;
}

bce_command_alias_t *bce_command_alias_free(bce_command_alias_t *alias) {
    if (!alias) {
        return NULL;
    }

    free(alias);
    return NULL;
}

bce_command_arg_t *bce_command_arg_free(bce_command_arg_t *arg) {
    if (!arg) {
        return NULL;
    }

    arg->opts = ll_destroy(arg->opts);

    free(arg);
    return NULL;
}

bce_command_opt_t *bce_command_opt_free(bce_command_opt_t *opt) {
    if (!opt) {
        return NULL;
    }

    // no internal data to free here

    free(opt);
    return NULL;
}

bce_error_t db_store_command(struct sqlite3 *conn, const bce_command_t *completion_command) {
    if (!completion_command) {
        return ERR_INVALID_CMD;
    }

    sqlite3_stmt *stmt;
    int rc;

    // insert the command
    rc = sqlite3_prepare_v3(conn, COMMAND_WRITE_SQL, -1, 0, &stmt, NULL);
    if (rc != SQLITE_OK) {
        goto done;
    }
    sqlite3_bind_text(stmt, 1, completion_command->uuid, -1, NULL);
    sqlite3_bind_text(stmt, 2, completion_command->name, -1, NULL);
    if (strlen(completion_command->parent_cmd_uuid) > 0) {
        sqlite3_bind_text(stmt, 3, completion_command->parent_cmd_uuid, -1, NULL);
    } else {
        sqlite3_bind_null(stmt, 3);
    }
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        goto done;
    }

    // insert the aliases
    if (completion_command->aliases) {
        for (linked_list_node_t *node = completion_command->aliases->head; node != NULL; node = node->next) {
            bce_command_alias_t *alias = (bce_command_alias_t *) node->data;
            rc = db_store_command_alias(conn, alias);
            if (rc != SQLITE_OK) {
                goto done;
            }
        }
    }

    // insert each sub-command
    if (completion_command->sub_commands) {
        for (linked_list_node_t *node = completion_command->sub_commands->head; node != NULL; node = node->next) {
            bce_command_t *subcmd = (bce_command_t *) node->data;
            rc = db_store_command(conn, subcmd);
            if (rc != SQLITE_OK) {
                goto done;
            }
        }
    }

    // insert each of the command_args
    if (completion_command->args) {
        for (linked_list_node_t *node = completion_command->args->head; node != NULL; node = node->next) {
            bce_command_arg_t *arg = (bce_command_arg_t *) node->data;
            rc = db_store_command_arg(conn, arg);
            if (rc != SQLITE_OK) {
                goto done;
            }
        }
    }

    done:
    sqlite3_finalize(stmt);
    if ((rc == SQLITE_DONE) || (rc == SQLITE_OK)) {
        return ERR_NONE;
    } else {
        return ERR_SQLITE_ERROR;
    }
}

bce_error_t db_store_command_alias(struct sqlite3 *conn, const bce_command_alias_t *alias) {
    if (!alias) {
        return ERR_INVALID_ALIAS;
    }

    sqlite3_stmt *stmt;
    int rc;

    // insert the alias
    rc = sqlite3_prepare_v3(conn, COMMAND_ALIAS_WRITE_SQL, -1, 0, &stmt, NULL);
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
    if ((rc == SQLITE_DONE) || (rc == SQLITE_OK)) {
        return ERR_NONE;
    } else {
        return ERR_SQLITE_ERROR;
    }
}

bce_error_t db_store_command_arg(struct sqlite3 *conn, const bce_command_arg_t *arg) {
    if (!arg) {
        return ERR_INVALID_ARG;
    }

    sqlite3_stmt *stmt;
    int rc;

    // insert the arg
    rc = sqlite3_prepare_v3(conn, COMMAND_ARG_WRITE_SQL, -1, 0, &stmt, NULL);
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
        for (linked_list_node_t *node = arg->opts->head; node != NULL; node = node->next) {
            bce_command_opt_t *opt = (bce_command_opt_t *) node->data;
            rc = db_store_command_opt(conn, opt);
            if (rc != SQLITE_OK) {
                goto done;
            }
        }
    }

    done:
    sqlite3_finalize(stmt);
    if ((rc == SQLITE_DONE) || (rc = SQLITE_OK)) {
        return ERR_NONE;
    } else {
        return ERR_SQLITE_ERROR;
    }
}

bce_error_t db_store_command_opt(struct sqlite3 *conn, const bce_command_opt_t *opt) {
    if (!opt) {
        return ERR_INVALID_OPT;
    }

    sqlite3_stmt *stmt;
    int rc;

    // insert the opt
    rc = sqlite3_prepare_v3(conn, COMMAND_OPT_WRITE_SQL, -1, 0, &stmt, NULL);
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
    if ((rc == SQLITE_DONE) || (rc = SQLITE_OK)) {
        return ERR_NONE;
    } else {
        return ERR_SQLITE_ERROR;
    }
}

bce_error_t db_delete_command(struct sqlite3 *conn, const char *command_name) {
    if (!conn || !command_name) {
        return ERR_NO_DATABASE_CONNECTION;
    }

    sqlite3_stmt *stmt;
    int rc;

    // delete the command (CASCADE should happen for all FKs)
    rc = sqlite3_prepare_v3(conn, COMMAND_DELETE_SQL, -1, 0, &stmt, NULL);
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
    if ((rc == SQLITE_DONE) || (rc = SQLITE_OK)) {
        return ERR_NONE;
    } else {
        return ERR_SQLITE_ERROR;
    }
}