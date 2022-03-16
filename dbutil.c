#include "dbutil.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <sqlite3.h>
#include "error.h"
#include "data_model.h"

static const char *SCHEMA_VERSION_SQL =
        " PRAGMA user_version ";

static const char *CREATE_COMPLETION_COMMAND_SQL =
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

static const char *CREATE_COMPLETION_COMMAND_ALIAS_SQL =
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

static const char *CREATE_COMPLETION_COMMAND_ARG_SQL =
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

static const char *CREATE_COMPLETION_COMMAND_OPT_SQL =
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

sqlite3 *db_open(const char *filename, int *result) {
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

sqlite3 *db_open_with_xa(const char *filename, int *rc) {
    // open the completion database
    sqlite3 *conn = db_open(filename, rc);
    if (*rc != SQLITE_OK) {
        fprintf(stderr, "Unable to open database. error: %d, database: %s\n", *rc, filename);
        return NULL;
    }

    // check schema version
    int schema_version = db_get_schema_version(conn);
    if (schema_version == 0) {
        // create the schema
        if (!db_create_schema(conn)) {
            fprintf(stderr, "Unable to create database schema. database: %s\n", filename);
            return NULL;
        }
        schema_version = db_get_schema_version(conn);
    }
    if (schema_version != DB_SCHEMA_VERSION) {
        fprintf(stderr, "Schema version mismatch. database: %s, expected: %d, found: %d\n", filename, DB_SCHEMA_VERSION,
                schema_version);
        return NULL;
    }

    // explicitly start a transaction, since this will be done automatically (per statement) otherwise
    *rc = sqlite3_exec(conn, "BEGIN TRANSACTION;", NULL, NULL, NULL);
    if (*rc != SQLITE_OK) {
        fprintf(stderr, "Unable to begin transaction, error: %d, database: %s\n", *rc, filename);
        return NULL;
    }

    return conn;
}

int db_get_schema_version(struct sqlite3 *conn) {
    int version = 0;
    sqlite3_stmt *stmt;
    // try to find the command by name
    int rc = sqlite3_prepare_v3(conn, SCHEMA_VERSION_SQL, -1, 0, &stmt, NULL);
    if (rc == SQLITE_OK) {
        int step = sqlite3_step(stmt);
        if (step == SQLITE_ROW) {
            version = sqlite3_column_int(stmt, 0);
        }
    }
    sqlite3_finalize(stmt);
    return version;
}

bce_error_t db_create_schema(struct sqlite3 *conn) {
    int rc;

    rc = sqlite3_exec(conn, CREATE_COMPLETION_COMMAND_SQL, 0, 0, NULL);
    if (rc != SQLITE_OK) {
        return ERR_DATABASE_CREATE_TABLE;
    }

    rc = sqlite3_exec(conn, CREATE_COMPLETION_COMMAND_ALIAS_SQL, 0, 0, NULL);
    if (rc != SQLITE_OK) {
        return ERR_DATABASE_CREATE_TABLE;
    }

    rc = sqlite3_exec(conn, CREATE_COMPLETION_COMMAND_ARG_SQL, 0, 0, NULL);
    if (rc != SQLITE_OK) {
        return ERR_DATABASE_CREATE_TABLE;
    }

    rc = sqlite3_exec(conn, CREATE_COMPLETION_COMMAND_OPT_SQL, 0, 0, NULL);
    if (rc != SQLITE_OK) {
        return ERR_DATABASE_CREATE_TABLE;
    }

    rc = sqlite3_exec(conn, "PRAGMA user_version = 1;", 0, 0, NULL);
    if (rc != SQLITE_OK) {
        return ERR_DATABASE_PRAGMA;
    }

    return ERR_NONE;
}

bool read_file_into_buffer(const char *filename, char **ppbuffer) {
    char *buffer = *ppbuffer;
    size_t length;
    FILE *f = fopen(filename, "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        length = ftell(f);
        fseek(f, 0, SEEK_SET);
        buffer = malloc(length);
        if (buffer) {
            fread(buffer, 1, length, f);
        }
        fclose(f);
    }

    if (buffer) {
        return true;
    }
    return false;
}

bce_error_t db_exec_sql_script(struct sqlite3 *conn, const char *filename) {
    bce_error_t result = ERR_NONE;
    char **sql_data = calloc(1, sizeof(char *));
    if (read_file_into_buffer(filename, sql_data)) {
        int rc;

        rc = sqlite3_exec(conn, *sql_data, 0, 0, NULL);
        if (rc != SQLITE_OK) {
            result = ERR_SQLITE_ERROR;
        }
    } else {
        fprintf(stderr, "Error reading file: %s\n", filename);
        result = ERR_READ_FILE;
    }
    // free the data in the buffer
    free(*sql_data);
    // free the buffer ptr
    free(sql_data);
    return result;
}

