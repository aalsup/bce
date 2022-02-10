#include "dbutil.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <sqlite3.h>
#include "error.h"

static const char* SCHEMA_VERSION_SQL =
        " PRAGMA user_version ";

static const char* CREATE_COMPLETION_COMMAND_SQL =
        " CREATE TABLE IF NOT EXISTS command ( "
        "    uuid TEXT PRIMARY KEY, "
        "    name TEXT NOT NULL, "
        "    parent_cmd TEXT, "
        "    FOREIGN KEY(parent_cmd) REFERENCES command(uuid) ON DELETE CASCADE "
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
        "    FOREIGN KEY(cmd_uuid) REFERENCES command(uuid) ON DELETE CASCADE "
        " ); "
        " \n "
        " CREATE INDEX command_alias_name_idx "
        "    ON command_alias (name); "
        " \n "
        " CREATE INDEX command_alias_cmd_uuid_idx "
        "    ON command_alias (cmd_uuid); "
        " \n "
        " CREATE UNIQUE INDEX command_alias_cmd_name_idx "
        "    ON command_alias (cmd_uuid, name); ";

static const char* CREATE_COMPLETION_COMMAND_ARG_SQL =
        " CREATE TABLE IF NOT EXISTS command_arg ( "
        "    uuid TEXT PRIMARY KEY, "
        "    cmd_uuid TEXT NOT NULL, "
        "    arg_type TEXT NOT NULL "
        "        CHECK (arg_type IN ('NONE', 'OPTION', 'FILE', 'TEXT')), "
        "    description TEXT NOT NULL, "
        "    long_name TEXT, "
        "    short_name TEXT, "
        "    FOREIGN KEY(cmd_uuid) REFERENCES command(uuid) ON DELETE CASCADE, "
        "    CHECK ( (long_name IS NOT NULL) OR (short_name IS NOT NULL) ) "
        " ); "
        " \n "
        " CREATE INDEX command_arg_cmd_uuid_idx "
        "    ON command_arg (cmd_uuid); "
        " \n "
        " CREATE UNIQUE INDEX command_arg_longname_idx "
        "    ON command_arg (cmd_uuid, long_name); ";

static const char* CREATE_COMPLETION_COMMAND_OPT_SQL =
        " CREATE TABLE IF NOT EXISTS command_opt ( "
        "    uuid TEXT PRIMARY KEY, "
        "    cmd_arg_uuid TEXT NOT NULL, "
        "    name TEXT NOT NULL, "
        "    FOREIGN KEY(cmd_arg_uuid) REFERENCES command_arg(uuid) ON DELETE CASCADE "
        " );"
        " \n "
        " CREATE INDEX command_opt_cmd_arg_idx "
        "    ON command_opt (cmd_arg_uuid); "
        " \n "
        " CREATE UNIQUE INDEX command_opt_arg_name_idx "
        "    ON command_opt (cmd_arg_uuid, name); ";

sqlite3* open_database(const char *filename, int *result) {
    char *err_msg = 0;
    sqlite3 *conn;

    int rc = sqlite3_open(filename, &conn);
    if (rc != SQLITE_OK) {
        sqlite3_close(conn);

        *result = ERR_OPEN_DATABASE;
        return NULL;
    }

    rc = sqlite3_exec(conn, "PRAGMA journal_mode = WAL", 0, 0, &err_msg);
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

int get_schema_version(struct sqlite3 *conn) {
    int version = 0;
    sqlite3_stmt *stmt;
    // try to find the command by name
    int rc = sqlite3_prepare(conn, SCHEMA_VERSION_SQL, -1, &stmt, NULL);
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

    rc = sqlite3_exec(conn, "PRAGMA user_version = 1;", 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        *result = ERR_DATABASE_PRAGMA;
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

