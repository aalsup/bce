#include "dbutil.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <sqlite3.h>
#include "error.h"

static const char* SCHEMA_VERSION_SQL =
        " PRAGMA user_version ";

static const char* ENSURE_SCHEMA_SQL =
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

static const char* CREATE_COMPLETION_COMMAND_SQL =
        " CREATE TABLE IF NOT EXISTS command ( "
        "    uuid TEXT PRIMARY KEY, "
        "    name TEXT NOT NULL, "
        "    parent_cmd TEXT "
        " ); "
        " \n "
        " CREATE UNIQUE INDEX command_name_idx "
        "    ON command (name); "
        " \n "
        " CREATE INDEX command_parent_idx "
        "    ON command (parent_cmd); ";

static const char* CREATE_COMPLETION_COMMAND_ALIAS_SQL =
        " CREATE TABLE IF NOT EXISTS command_alias ( "
        "    uuid TEXT PRIMARY KEY, "
        "    cmd_uuid TEXT NOT NULL, "
        "    name TEXT NOT NULL, "
        "    FOREIGN KEY(cmd_uuid) REFERENCES command(uuid) "
        " ); "
        " \n "
        " CREATE UNIQUE INDEX command_alias_name_idx "
        "    ON command_alias (name); "
        " \n "
        " CREATE INDEX command_alias_cmd_uuid_idx "
        "    ON command_alias (cmd_uuid); ";

static const char* CREATE_COMPLETION_COMMAND_ARG_SQL =
        " CREATE TABLE IF NOT EXISTS command_arg ( "
        "    uuid TEXT PRIMARY KEY, "
        "    cmd_uuid TEXT NOT NULL, "
        "    arg_type TEXT NOT NULL, "
        "    long_name TEXT, "
        "    short_name TEXT, "
        "    FOREIGN KEY(cmd_uuid) REFERENCES command(uuid) "
        " ); "
        " \n "
        " CREATE INDEX command_arg_cmd_uuid_idx "
        "    ON command_arg (cmd_uuid); ";

static const char* CREATE_COMPLETION_COMMAND_OPT_SQL =
        " CREATE TABLE IF NOT EXISTS command_opt ( "
        "    uuid TEXT PRIMARY KEY, "
        "    cmd_arg_uuid TEXT NOT NULL, "
        "    name TEXT NOT NULL,\n"
        "    FOREIGN KEY(cmd_arg_uuid) REFERENCES command_arg(uuid) "
        " );"
        " \n "
        " CREATE INDEX command_opt_cmd_arg_idx "
        "    ON command_opt (cmd_arg_uuid); ";

// TODO: migrate old to new schema.
bool ensure_schema(struct sqlite3 *conn) {
    // get the schema version from the DB `user_version` pragma
    int schema_version = get_schema_version(conn);
    if (schema_version != SCHEMA_VERSION) {
        // TODO: should we upgrade the schema?
        return false;
    }

    // query the database to ensure the expected tables exist
    bool result = false;
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare(conn, ENSURE_SCHEMA_SQL, -1, &stmt, 0);
    if (rc == SQLITE_OK) {
        int step = sqlite3_step(stmt);
        if (step == SQLITE_ROW) {
            int success = sqlite3_column_int(stmt, 0);
            result = (success == 1);
        }
    }
    sqlite3_finalize(stmt);
    return result;
}

int get_schema_version(struct sqlite3 *conn) {
    int version = 0;
    sqlite3_stmt *stmt;
    // try to find the command by name
    int rc = sqlite3_prepare(conn, SCHEMA_VERSION_SQL, -1, &stmt, 0);
    if (rc == SQLITE_OK) {
        int step = sqlite3_step(stmt);
        if (step == SQLITE_ROW) {
            version = sqlite3_column_int(stmt, 0);
        }
    }
    sqlite3_finalize(stmt);
    return version;
}

bool create_schema(struct sqlite3 *conn, int *result) {
    char *err_msg = 0;
    int rc;

    rc = sqlite3_exec(conn, CREATE_COMPLETION_COMMAND_SQL, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        *result = ERR_DATABASE_CREATE_TABLE;
        return false;
    }

    rc = sqlite3_exec(conn, CREATE_COMPLETION_COMMAND_ALIAS_SQL, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        *result = ERR_DATABASE_CREATE_TABLE;
        return false;
    }

    rc = sqlite3_exec(conn, CREATE_COMPLETION_COMMAND_ARG_SQL, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        *result = ERR_DATABASE_CREATE_TABLE;
        return false;
    }

    rc = sqlite3_exec(conn, CREATE_COMPLETION_COMMAND_OPT_SQL, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        *result = ERR_DATABASE_CREATE_TABLE;
        return false;
    }

    *result = SQLITE_OK;
    return true;
}

bool read_file_into_buffer(const char *filename, char **ppbuffer) {
    char *buffer = *ppbuffer;
    size_t length;
    FILE *f = fopen (filename, "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        length = ftell(f);
        fseek(f, 0, SEEK_SET);
        buffer = (char *)malloc(length);
        if (buffer) {
            fread(buffer, 1, length, f);
        }
        fclose (f);
    }

    if (buffer) {
        return true;
    }
    return false;
}

bool exec_sql_script(struct sqlite3 *conn, const char *filename) {
    int result = true;
    char **sql_data = (char **)malloc(sizeof(char));
    if (read_file_into_buffer(filename, sql_data)) {
        char *err_msg = 0;
        int rc;

        rc = sqlite3_exec(conn, *sql_data, 0, 0, &err_msg);
        if (rc != SQLITE_OK) {
            result = false;
        }
    }
    free((void *) *sql_data);
    free((void *) sql_data);
    return result;
}

