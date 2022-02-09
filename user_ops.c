#include "user_ops.h"
#include <stdio.h>
#include <string.h>
#include <sqlite3.h>
#include "error.h"
#include "dbutil.h"
#include "completion_model.h"

static const int CMD_NAME_SIZE = 50;

sqlite3* open_and_prepare_db(const char *filename, int *rc);

int parse_args(int argc, char **argv) {
    if (argc <= 1) {
        // called from BASH (for completion help)
        return 0;
    }

    // collect the command-line arguments
    operation_t op = OP_NONE;
    char filename[FILENAME_MAX] = "";
    char command_name[CMD_NAME_SIZE + 1] = "";
    for (int i = 1; i < argc; i++) {
        if ((strncmp(HELP_ARG_LONGNAME, argv[i], strlen(HELP_ARG_LONGNAME)) == 0)
            || (strncmp(HELP_ARG_SHORTNAME, argv[i], strlen(HELP_ARG_SHORTNAME)) == 0))
        {
            op = OP_HELP;
            break;
        }
        else if ((strncmp(EXPORT_ARG_LONGNAME, argv[i], strlen(EXPORT_ARG_LONGNAME)) == 0)
                 || (strncmp(EXPORT_ARG_SHORTNAME, argv[i], strlen(EXPORT_ARG_SHORTNAME)) == 0))
        {
            op = OP_EXPORT;
            // next parameter should be the command name
            if ((i+1) < argc) {
                strncpy(command_name, argv[++i], CMD_NAME_SIZE);
            }
        }
        else if ((strncmp(IMPORT_ARG_LONGNAME, argv[i], strlen(IMPORT_ARG_LONGNAME)) == 0)
                 || (strncmp(IMPORT_ARG_SHORTNAME, argv[i], strlen(IMPORT_ARG_SHORTNAME)) == 0))
        {
            op = OP_IMPORT;
        }
        else if ((strncmp(FILE_ARG_LONGNAME, argv[i], strlen(FILE_ARG_LONGNAME)) == 0)
                 || (strncmp(FILE_ARG_SHORTNAME, argv[i], strlen(FILE_ARG_SHORTNAME)) == 0))
        {
            // next parameter should be the filename
            if ((i+1) < argc) {
                strncpy(filename, argv[++i], FILENAME_MAX);
            }
        }
    }

    // determine what operation to perform
    int result = 0;
    switch (op) {
        case OP_EXPORT:
            result = process_export(command_name, filename);
            break;
        case OP_IMPORT:
            result = process_import(filename);
            break;
        case OP_NONE:
            fprintf(stderr, "Invalid arguments\n");
            result = ERR_INVALID_ARGUMENT;
        default: // OP_NONE or OP_HELP
            show_usage();
    }
    return result;
}

void show_usage(void) {
    printf("\nbce (bash_complete_extension)\n");
    printf("usage:\n");
    printf("  bce --export <command> --file <filename>\n");
    printf("  bce --import --file <filename>\n");
    printf("\narguments:\n");
    printf("  %s (%s) : export command data to file\n", EXPORT_ARG_LONGNAME, EXPORT_ARG_SHORTNAME);
    printf("  %s (%s) : import command data from file\n", IMPORT_ARG_LONGNAME, IMPORT_ARG_SHORTNAME);
    printf("  %s (%s) : filename to import/export\n", FILE_ARG_LONGNAME, FILE_ARG_SHORTNAME);
    printf("\n");
}

int process_import(const char *filename) {
    printf("process_import() called: filename=%s\n", filename);
    return 0;
}

int process_export(const char *command_name, const char *filename) {
    printf("process_export() called: command_name=%s, filename=%s\n", command_name, filename);
    int rc = SQLITE_OK;
    int err = 0;

    // open the source database
    sqlite3 *src_db = open_and_prepare_db("completion.db", &rc);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Unable to open database. error: %d, database: %s\n", rc, filename);
        err = ERR_OPEN_DATABASE;
        goto done;
    }

    // open the destination database
    remove(filename);
    sqlite3 *dest_db = open_and_prepare_db(filename, &rc);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Unable to open database. error: %d, database: %s\n", rc, filename);
        err = ERR_OPEN_DATABASE;
        goto done;
    }

    completion_command_t *completion_command = create_completion_command();
    rc = get_db_command(completion_command, src_db, command_name);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "get_db_command() returned %d\n", rc);
        goto done;
    }

    rc = write_db_command(completion_command, dest_db);
    if (rc != SQLITE_OK) {
        err = ERR_SQLITE_ERROR;
        goto done;
    }

    // commit transaction
    rc = sqlite3_exec(dest_db, "COMMIT;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Unable to commit transaction, error: %d, database: %s\n", rc, filename);
        err = ERR_SQLITE_ERROR;
        goto done;
    }

done:
    sqlite3_close(src_db);
    sqlite3_close(dest_db);
    return err;
}

sqlite3* open_and_prepare_db(const char *filename, int *rc) {
    // open the completion database
    sqlite3 *conn = open_database(filename, rc);
    if (*rc != SQLITE_OK) {
        fprintf(stderr, "Unable to open database. error: %d, database: %s\n", *rc, filename);
        return NULL;
    }

    // check schema version
    int schema_version = get_schema_version(conn);
    if (schema_version == 0) {
        // create the schema
        if (!create_schema(conn, rc)) {
            fprintf(stderr, "Unable to create database schema. database: %s\n", filename);
            return NULL;
        }
        schema_version = get_schema_version(conn);
    }
    if (schema_version != SCHEMA_VERSION) {
        fprintf(stderr, "Schema version mismatch. database: %s, expected: %d, found: %d\n", filename, SCHEMA_VERSION, schema_version);
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

