#ifndef USER_OPS_H
#define USER_OPS_H

#include <stdio.h>

typedef enum operation_t {
    OP_NONE,
    OP_HELP,
    OP_EXPORT,
    OP_IMPORT
} operation_t;

static const char* HELP_ARG_LONGNAME = "--help";
static const char* HELP_ARG_SHORTNAME = "-h";
static const char* EXPORT_ARG_LONGNAME = "--export";
static const char* EXPORT_ARG_SHORTNAME = "-e";
static const char* IMPORT_ARG_LONGNAME = "--import";
static const char* IMPORT_ARG_SHORTNAME = "-i";
static const char* FILE_ARG_LONGNAME = "--file";
static const char* FILE_ARG_SHORTNAME = "-f";

void show_usage(void);
int parse_args(int argc, char **argv);
int process_import(const char *filename);
int process_export(const char *command_name, const char *filename);

#endif // USER_OPS_H
