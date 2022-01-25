#include "completion.h"
#include "linked_list.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sqlite3.h>

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
        " JOIN command_alias a ON a.cmd_uuid = c.uuid "
        " WHERE c.name = ?1 OR a.name = ?2 ";

const char* COMPLETION_SUB_COMMAND_SQL =
        " SELECT c.uuid, c.name, c.parent_cmd "
        " FROM command c "
        " WHERE c.parent_cmd = ?1 ";

const char* COMPLETION_SUB_COMMAND_COUNT_SQL =
        " SELECT count(*) "
        " FROM command c "
        " WHERE c.parent_cmd = ?1 ";

sqlite3* open_database(const char *filename, int *result) {
    char *err_msg = 0;
    sqlite3 *conn;

    int rc = sqlite3_open("completion.db", &conn);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(conn));
        sqlite3_close(conn);

        *result = ERR_OPEN_DATABASE;
        return NULL;
    }

    rc = sqlite3_exec(conn, "PRAGMA journal_mode = 'WAL'", 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Unable to set journal_mode pragma\n");
        sqlite3_close(conn);

        *result = ERR_DATABASE_PRAGMA;
        return NULL;
    }

    rc = sqlite3_exec(conn, "PRAGMA foreign_keys = 1", 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Unable to set foreign_keys pragma\n");
        sqlite3_close(conn);

        *result = ERR_DATABASE_PRAGMA;
        return NULL;
    }

    *result = SQLITE_OK;
    return conn;
}

bool ensure_schema(sqlite3 *conn) {
    char *err_msg = 0;
    int rc = sqlite3_exec(conn, ENSURE_SCHEMA_SQL, 0, 0, &err_msg);
    return (rc == SQLITE_OK);
}

void print_command_tree(sqlite3 *conn, completion_command_t *cmd, int level) {
    // indent
    for (int i = 0; i < level; i++) {
        printf("  ");
    }
    printf("command: %s (%s)\n", cmd->name, cmd->uuid);

    if (cmd->sub_commands != NULL) {
        linked_list_node_t *node = cmd->sub_commands->head;
        while (node != NULL) {
            completion_command_t *sub_cmd = (completion_command_t *) node->data;
            print_command_tree(conn, sub_cmd, level + 1);
            node = node->next;
        }
    }
}

int get_db_command(completion_command_t *dest, sqlite3 *conn, char* command_name) {
    memset(dest->uuid, 0, UUID_FIELD_SIZE);
    memset(dest->name, 0, NAME_FIELD_SIZE);
    memset(dest->parent_cmd_uuid, 0, UUID_FIELD_SIZE);

    sqlite3_stmt *stmt;
    // try to find the command by name
    int rc = sqlite3_prepare(conn, COMPLETION_COMMAND_SQL, -1, &stmt, 0);
    if (rc == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, command_name, -1, NULL);
        sqlite3_bind_text(stmt, 2, command_name, -1, NULL);
        int step = sqlite3_step(stmt);
        if (step == SQLITE_ROW) {
            strncpy(dest->uuid, (const char *) sqlite3_column_text(stmt, 0), UUID_FIELD_SIZE);
            strncpy(dest->name, (const char *) sqlite3_column_text(stmt, 1), NAME_FIELD_SIZE);
            if (sqlite3_column_type(stmt, 2) == SQLITE_TEXT) {
                strncpy(dest->parent_cmd_uuid, (const char *) sqlite3_column_text(stmt, 2), UUID_FIELD_SIZE);
            }
            dest->sub_commands = ll_init();
            dest->command_args = ll_init();
            rc = get_db_sub_commands(conn, dest);
        }
    }
    sqlite3_finalize(stmt);

    return rc;
}

int get_db_sub_commands(sqlite3 *conn, completion_command_t *parent_cmd) {
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare(conn, COMPLETION_SUB_COMMAND_SQL, -1, &stmt, 0);
    if (rc == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, parent_cmd->uuid, -1, NULL);
        int step = sqlite3_step(stmt);
        while (step == SQLITE_ROW) {
            // create completion_command_t
            completion_command_t *sub_cmd = malloc(sizeof(completion_command_t));
            strncpy(sub_cmd->uuid, (const char *)sqlite3_column_text(stmt, 0), UUID_FIELD_SIZE);
            strncpy(sub_cmd->name, (const char *)sqlite3_column_text(stmt, 1), NAME_FIELD_SIZE);
            if (sqlite3_column_type(stmt, 2) == SQLITE_TEXT) {
                strncpy(sub_cmd->parent_cmd_uuid, (const char *)sqlite3_column_text(stmt, 2), UUID_FIELD_SIZE);
            }
            sub_cmd->sub_commands = ll_init();
            sub_cmd->command_args = ll_init();

            // recurse for sub-commands
            rc = get_db_sub_commands(conn, sub_cmd);
            ll_append(parent_cmd->sub_commands, sub_cmd);

            step = sqlite3_step(stmt);
        }
    }
    sqlite3_finalize(stmt);

    return rc;
}

void free_completion_command(completion_command_t *cmd) {
    if (cmd == NULL) {
        return;
    }

    // free sub-commands
    linked_list_t *sub_cmd_list = cmd->sub_commands;
    if (sub_cmd_list != NULL) {
        linked_list_node_t *node = (linked_list_node_t *)sub_cmd_list->head;
        while (node != NULL) {
            completion_command_t *sub_cmd = (completion_command_t *)node->data;
            free_completion_command(sub_cmd);
            linked_list_node_t *next_node = node->next;
            node->data = NULL;
            node->next = NULL;
            node = next_node;
        }
    }
    ll_destroy(sub_cmd_list);

    // free command-args
    linked_list_t *arg_list = cmd->command_args;
    if (arg_list != NULL) {
        linked_list_node_t *node = (linked_list_node_t *)arg_list->head;
        while (node != NULL) {
            completion_command_arg_t *arg = (completion_command_arg_t *)node->data;
            free_completion_command_arg(arg);
            linked_list_node_t *next_node = node->next;
            node->data = NULL;
            node->next = NULL;
            node = next_node;
        }
    }
    ll_destroy(arg_list);
}

void free_completion_command_arg(completion_command_arg_t *arg) {
    if (arg == NULL) {
        return;
    }

    // TODO: implement free logic
}



