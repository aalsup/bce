#include "cli.h"
#include <stdio.h>
#include <string.h>
#include <sqlite3.h>
#include "error.h"
#include "dbutil.h"
#include "data_model.h"

static const int CMD_NAME_SIZE = 50;

sqlite3* open_db_with_xa(const char *filename, int *rc);

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
    int rc = SQLITE_OK;
    int err = 0;

    // open the source database
    sqlite3 *src_db = open_db_with_xa(filename, &rc);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Unable to open database. error: %d, database: %s\n", rc, filename);
        err = ERR_OPEN_DATABASE;
        goto done;
    }

    // open dest database
    sqlite3 *dest_db = open_db_with_xa("completion.db", &rc);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Unable to open database. error: %d, database: %s\n", rc, "completion.db");
        err = ERR_OPEN_DATABASE;
        goto done;
    }

    // get a list of the top-level commands in source database
    linked_list_t *cmd_names = ll_create(NULL);
    rc = get_db_command_names(src_db, cmd_names);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Unable to query commands. error: %d, database: %s\n", rc, filename);
        err = ERR_SQLITE_ERROR;
        goto done;
    }

    // iterate over the commands
    linked_list_node_t *node = cmd_names->head;
    while (node) {
        char *cmd_name = (char *)node->data;
        completion_command_t *cmd = create_completion_command();

        // read cmd from src database
        rc = get_db_command(src_db, cmd, cmd_name);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "Unable to query command: %s. error: %d\n", cmd_name, rc);
            err = ERR_SQLITE_ERROR;
            goto done;
        }

        // delete cmd (recurse) from dest database
        rc = delete_db_command(dest_db, cmd_name);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "Unable to delete the command before importing. command %s, error: %d\n", cmd_name, rc);
            err = ERR_SQLITE_ERROR;
            goto done;
        }

        // write cmd to dest database
        rc = write_db_command(dest_db, cmd);
        if (rc != SQLITE_OK) {
            err = ERR_SQLITE_ERROR;
            goto done;
        }

        // cleanup
        free_completion_command(&cmd);

        node = node->next;
    }

    // commit transaction
    rc = sqlite3_exec(dest_db, "COMMIT;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Unable to commit transaction, error: %d, database: %s\n", rc, "completion.db");
        err = ERR_SQLITE_ERROR;
        goto done;
    }

done:
    sqlite3_close(src_db);
    sqlite3_close(dest_db);
    return err;
}

int process_export(const char *command_name, const char *filename) {
    int rc = SQLITE_OK;
    int err = 0;

    // open the source database
    sqlite3 *src_db = open_db_with_xa("completion.db", &rc);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Unable to open database. error: %d, database: %s\n", rc, "completion.db");
        err = ERR_OPEN_DATABASE;
        goto done;
    }

    // open the destination database
    remove(filename);
    sqlite3 *dest_db = open_db_with_xa(filename, &rc);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Unable to open database. error: %d, database: %s\n", rc, filename);
        err = ERR_OPEN_DATABASE;
        goto done;
    }

    completion_command_t *completion_command = create_completion_command();
    rc = get_db_command(src_db, completion_command, command_name);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "get_db_command() returned %d\n", rc);
        goto done;
    }

    rc = write_db_command(dest_db, completion_command);
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
    if (err) {
        fprintf(stderr, "Export did not complete successfully. error: %d\n", err);
    }
    if (completion_command) {
        free_completion_command(&completion_command);
    }
    sqlite3_close(src_db);
    sqlite3_close(dest_db);
    return err;
}

sqlite3* open_db_with_xa(const char *filename, int *rc) {
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

