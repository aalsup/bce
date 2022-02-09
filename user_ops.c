#include "user_ops.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "error.h"

static const int CMD_NAME_SIZE = 50;

int parse_args(int argc, char **argv) {
    if (argc <= 1) {
        // called from BASH (for completion help)
        return false;
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
        default:
            // OP_NONE or OP_HELP
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
    return 0;
}

