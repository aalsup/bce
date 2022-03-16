#ifndef BCE_CLI_H
#define BCE_CLI_H

#include <stdio.h>
#include "error.h"

typedef enum operation_t {
    OP_NONE,
    OP_HELP,
    OP_EXPORT,
    OP_IMPORT
} operation_t;

typedef enum format_t {
    FORMAT_SQLITE,
    FORMAT_JSON
} format_t;

static const char *HELP_ARG_LONGNAME = "--help";
static const char *HELP_ARG_SHORTNAME = "-h";
static const char *EXPORT_ARG_LONGNAME = "--export";
static const char *EXPORT_ARG_SHORTNAME = "-e";
static const char *IMPORT_ARG_LONGNAME = "--import";
static const char *IMPORT_ARG_SHORTNAME = "-i";
static const char *FORMAT_ARG_LONGNAME = "--format";
static const char *FORMAT_ARG_SHORTNAME = "-o";
static const char *FILE_ARG_LONGNAME = "--file";
static const char *FILE_ARG_SHORTNAME = "-f";
static const char *URL_ARG_LONGNAME = "--url";
static const char *URL_ARG_SHORTNAME = "-u";

void show_usage(void);

/* Perform the CLI operations specified on the command line */
bce_error_t process_cli_impl(int argc, const char **argv);

#endif // BCE_CLI_H
