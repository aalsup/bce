#include "cli.h"
#include <stdio.h>
#include <string.h>
#include <sqlite3.h>
#include <json-c/json.h>
#include "error.h"
#include "dbutil.h"
#include "data_model.h"
#include "uuid4.h"
#include "download.h"

static const size_t CMD_NAME_SIZE = 50;
static const size_t URL_SIZE = 1024;

static int process_import_sqlite(const char *filename);

static int process_export_sqlite(const char *command_name, const char *filename);

static int process_import_json(const char *filename, const char *url);

static int process_export_json(const char *command_name, const char *filename);

static sqlite3 *open_db_with_xa(const char *filename, int *rc);

static bce_command_t *bce_command_from_json(const char *parent_cmd_uuid, struct json_object *j_command);

static bce_command_alias_t *bce_command_alias_from_json(const char *cmd_uuid, struct json_object *j_alias);

static bce_command_arg_t *bce_command_arg_from_json(const char *cmd_uuid, struct json_object *j_arg);

static bce_command_opt_t *bce_command_opt_from_json(const char *arg_uuid, struct json_object *j_opt);

static json_object *bce_command_to_json(bce_command_t *cmd);

static json_object *bce_command_alias_to_json(bce_command_alias_t *alias);

static json_object *bce_command_arg_to_json(bce_command_arg_t *arg);

static json_object *bce_command_opt_to_json(bce_command_opt_t *opt);

int process_cli(int argc, char **argv) {
    if (argc <= 1) {
        // called from BASH (for completion help)
        return 0;
    }

    // collect the command-line arguments
    operation_t op = OP_NONE;
    char filename[FILENAME_MAX + 1];
    char command_name[CMD_NAME_SIZE + 1];
    char url[URL_SIZE + 1];
    format_t format = FORMAT_SQLITE;
    for (int i = 1; i < argc; i++) {
        // help
        if ((strncmp(HELP_ARG_LONGNAME, argv[i], strlen(HELP_ARG_LONGNAME)) == 0)
            || (strncmp(HELP_ARG_SHORTNAME, argv[i], strlen(HELP_ARG_SHORTNAME)) == 0)) {
            op = OP_HELP;
            break;
        }
        // export
        else if ((strncmp(EXPORT_ARG_LONGNAME, argv[i], strlen(EXPORT_ARG_LONGNAME)) == 0)
                 || (strncmp(EXPORT_ARG_SHORTNAME, argv[i], strlen(EXPORT_ARG_SHORTNAME)) == 0)) {
            op = OP_EXPORT;
            // next parameter should be the command name
            if ((i + 1) < argc) {
                strncat(command_name, argv[++i], CMD_NAME_SIZE);
            } else {
                op = OP_NONE;
                break;
            }
        }
        // import
        else if ((strncmp(IMPORT_ARG_LONGNAME, argv[i], strlen(IMPORT_ARG_LONGNAME)) == 0)
                 || (strncmp(IMPORT_ARG_SHORTNAME, argv[i], strlen(IMPORT_ARG_SHORTNAME)) == 0)) {
            op = OP_IMPORT;
        }
        // filename
        else if ((strncmp(FILE_ARG_LONGNAME, argv[i], strlen(FILE_ARG_LONGNAME)) == 0)
                 || (strncmp(FILE_ARG_SHORTNAME, argv[i], strlen(FILE_ARG_SHORTNAME)) == 0)) {
            // next parameter should be the filename
            if ((i + 1) < argc) {
                strncat(filename, argv[++i], FILENAME_MAX);
            } else {
                op = OP_NONE;
                break;
            }
        }
        // format
        else if ((strncmp(FORMAT_ARG_LONGNAME, argv[i], strlen(FORMAT_ARG_LONGNAME)) == 0)
                 || (strncmp(FORMAT_ARG_SHORTNAME, argv[i], strlen(FORMAT_ARG_SHORTNAME)) == 0)) {
            // next parameter should be the format
            if ((i + 1) < argc) {
                i++;
                if (strncmp("json", argv[i], CMD_NAME_SIZE) == 0) {
                    format = FORMAT_JSON;
                } else if (strncmp("sqlite", argv[i], CMD_NAME_SIZE) == 0) {
                    format = FORMAT_SQLITE;
                } else {
                    op = OP_NONE;
                    break;
                }
            } else {
                op = OP_NONE;
                break;
            }
        }
        // url
        else if ((strncmp(URL_ARG_LONGNAME, argv[i], strlen(URL_ARG_LONGNAME)) == 0)
                 || (strncmp(URL_ARG_SHORTNAME, argv[i], strlen(URL_ARG_SHORTNAME)) == 0)) {
            // next parameter should be the URL
            if ((i + 1) < argc) {
                strncat(url, argv[++i], URL_SIZE);
            } else {
                op = OP_NONE;
                break;
            }
        }
    }

    // check values
    if (op == OP_EXPORT) {
        if (strlen(filename) == 0) {
            op = OP_NONE;
        } else if (strlen(command_name) == 0) {
            op = OP_NONE;
        }
    } else if (op == OP_IMPORT) {
        if ((strlen(filename) == 0) && (strlen(url) == 0)) {
            op = OP_NONE;
        }
    }

    // determine what operation to perform
    int result = 0;
    switch (op) {
        case OP_EXPORT:
            if (format == FORMAT_JSON) {
                result = process_export_json(command_name, filename);
            } else {
                result = process_export_sqlite(command_name, filename);
            }
            break;
        case OP_IMPORT:
            if (format == FORMAT_JSON) {
                result = process_import_json(filename, url);
            } else {
                result = process_import_sqlite(filename);
            }
            break;
        case OP_NONE:
            fprintf(stderr, "Invalid arguments\n");
            result = ERR_INVALID_CLI_ARGUMENT;
        default: // OP_NONE or OP_HELP
            show_usage();
    }
    return result;
}

void show_usage(void) {
    printf("\nbce (bash_complete_extension)\n");
    printf("usage:\n");
    printf("  bce --export <command> --format <sqlite|json> --file <filename>\n");
    printf("  bce --import --format <sqlite|json> --file <filename>\n");
    printf("  bce --import --format json --url <url-of-json-file>\n");
    printf("\narguments:\n");
    printf("  %s (%s) : export command data to file\n",
           EXPORT_ARG_LONGNAME, EXPORT_ARG_SHORTNAME);
    printf("  %s (%s) : import command data from file\n",
           IMPORT_ARG_LONGNAME, IMPORT_ARG_SHORTNAME);
    printf("  %s (%s) : format to read/write data [sqlite|json] (default=sqlite)\n",
           FORMAT_ARG_LONGNAME, FORMAT_ARG_SHORTNAME);
    printf("  %s (%s) : filename to import/export\n",
           FILE_ARG_LONGNAME, FILE_ARG_SHORTNAME);
    printf("  %s (%s) : url of json file to import\n",
           URL_ARG_LONGNAME, URL_ARG_SHORTNAME);
    printf("\n");
}

static int process_import_sqlite(const char *filename) {
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
        char *cmd_name = (char *) node->data;
        bce_command_t *cmd = create_bce_command();

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

        // vacuum the database (reclaim space)
        rc = sqlite3_exec(dest_db, "VACUUM", NULL, NULL, NULL);

        // write cmd to dest database
        rc = write_db_command(dest_db, cmd);
        if (rc != SQLITE_OK) {
            err = ERR_SQLITE_ERROR;
            goto done;
        }

        // cleanup
        cmd = free_bce_command(cmd);

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

static int process_export_sqlite(const char *command_name, const char *filename) {
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

    // load the command hierarchy
    bce_command_t *completion_command = create_bce_command();
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
        completion_command = free_bce_command(completion_command);
    }
    sqlite3_close(src_db);
    sqlite3_close(dest_db);
    return err;
}

static int process_import_json(const char *json_filename, const char *url) {
    int err = 0;
    int rc = SQLITE_OK;
    const char *db_filename = "completion.db";
    if (uuid4_init() != UUID4_ESUCCESS) {
        fprintf(stderr, "UUID init failure\n");
        goto done;
    }

    // do we need to download the file
    if ((strlen(url) > 0) && (strlen(json_filename) == 0)) {
        bool result = download_file(url, json_filename);
        if (!result) {
            fprintf(stderr, "Unable to download file: %s\n", url);
            goto done;
        }
    }

    // parse the json
    struct json_object *parsed_json = json_object_from_file(json_filename);
    struct json_object *j_command = json_object_object_get(parsed_json, "command");
    bce_command_t *command = bce_command_from_json(NULL, j_command);

    // open the database
    sqlite3 *dest_db = open_db_with_xa(db_filename, &rc);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Unable to open database. error: %d, database: %s\n", rc, db_filename);
        err = ERR_OPEN_DATABASE;
        goto done;
    }

    // delete cmd (recurse) from dest database
    rc = delete_db_command(dest_db, command->name);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Unable to delete the command before importing. command %s, error: %d\n", command->name, rc);
        err = ERR_SQLITE_ERROR;
        goto done;
    }

    // insert cmd data
    rc = write_db_command(dest_db, command);
    if (rc != SQLITE_OK) {
        err = ERR_SQLITE_ERROR;
        goto done;
    }

    // commit transaction
    rc = sqlite3_exec(dest_db, "COMMIT;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Unable to commit transaction, error: %d, database: %s\n", rc, db_filename);
        err = ERR_SQLITE_ERROR;
        goto done;
    }

    done:
    sqlite3_close(dest_db);
    return err;
}

static int process_export_json(const char *command_name, const char *filename) {
    int rc = SQLITE_OK;
    int err = 0;

    // open the source database
    sqlite3 *src_db = open_db_with_xa("completion.db", &rc);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Unable to open database. error: %d, database: %s\n", rc, "completion.db");
        err = ERR_OPEN_DATABASE;
        goto done;
    }

    // load the command hierarchy
    prepare_statement_cache(src_db);
    bce_command_t *completion_command = create_bce_command();
    rc = get_db_command(src_db, completion_command, command_name);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "get_db_command() returned %d\n", rc);
        goto done;
    }
    free_statement_cache(src_db);

    // convert model object to json
    json_object *j_command = json_object_new_object();
    json_object *j_command_body = bce_command_to_json(completion_command);
    json_object_object_add(j_command, "command", j_command_body);
    int json_flags = JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED | JSON_C_TO_STRING_NOSLASHESCAPE;
    json_object_to_file_ext(filename, j_command, json_flags);
    json_object_put(j_command);

    done:
    if (err) {
        fprintf(stderr, "Export did not complete successfully. error: %d\n", err);
    }
    if (completion_command) {
        completion_command = free_bce_command(completion_command);
    }
    sqlite3_close(src_db);
    return err;
}

/*
"command": {
  "uuid": "str", <optional>
  "name": "str",
  "aliases": [],
  "args": [],
  "sub_commands": []
}
 */
static bce_command_t *bce_command_from_json(const char *parent_cmd_uuid, struct json_object *j_command) {
    bce_command_t *bce_command = create_bce_command();
    json_object *j_obj = NULL;

    if (parent_cmd_uuid) {
        strncat(bce_command->parent_cmd_uuid, parent_cmd_uuid, UUID_FIELD_SIZE);
    }

    j_obj = json_object_object_get(j_command, "uuid");
    if (j_obj) {
        const char *uuid = json_object_get_string(j_obj);
        strncat(bce_command->uuid, uuid, UUID_FIELD_SIZE);
    } else {
        uuid4_generate(bce_command->uuid);
    }
    j_obj = json_object_object_get(j_command, "name");
    if (j_obj) {
        const char *name = json_object_get_string(j_obj);
        strncat(bce_command->name, name, NAME_FIELD_SIZE);
    }
    j_obj = json_object_object_get(j_command, "aliases");
    if (j_obj) {
        if (json_object_is_type(j_obj, json_type_array)) {
            size_t len = json_object_array_length(j_obj);
            for (size_t i = 0; i < len; i++) {
                struct json_object *j_alias = json_object_array_get_idx(j_obj, i);
                bce_command_alias_t *alias = bce_command_alias_from_json(bce_command->uuid, j_alias);
                ll_append_item(bce_command->aliases, alias);
            }
        }
    }
    j_obj = json_object_object_get(j_command, "args");
    if (j_obj) {
        if (json_object_is_type(j_obj, json_type_array)) {
            size_t len = json_object_array_length(j_obj);
            for (size_t i = 0; i < len; i++) {
                struct json_object *j_arg = json_object_array_get_idx(j_obj, i);
                bce_command_arg_t *arg = bce_command_arg_from_json(bce_command->uuid, j_arg);
                ll_append_item(bce_command->args, arg);
            }
        }
    }
    j_obj = json_object_object_get(j_command, "sub_commands");
    if (j_obj) {
        if (json_object_is_type(j_obj, json_type_array)) {
            size_t len = json_object_array_length(j_obj);
            for (size_t i = 0; i < len; i++) {
                struct json_object *j_sub = json_object_array_get_idx(j_obj, i);
                bce_command_t *sub = bce_command_from_json(bce_command->uuid, j_sub);
                ll_append_item(bce_command->sub_commands, sub);
            }
        }
    }

    return bce_command;
}

/*
{
  "uuid": "str", <optional>
  "name": "str"
}
 */
static bce_command_alias_t *bce_command_alias_from_json(const char *cmd_uuid, struct json_object *j_alias) {
    bce_command_alias_t *bce_alias = create_bce_command_alias();
    json_object *j_obj = NULL;

    strncat(bce_alias->cmd_uuid, cmd_uuid, UUID_FIELD_SIZE);

    j_obj = json_object_object_get(j_alias, "uuid");
    if (j_obj) {
        const char *uuid = json_object_get_string(j_obj);
        strncat(bce_alias->uuid, uuid, UUID_FIELD_SIZE);
    } else {
        uuid4_generate(bce_alias->uuid);
    }
    j_obj = json_object_object_get(j_alias, "name");
    if (j_obj) {
        const char *name = json_object_get_string(j_obj);
        strncat(bce_alias->name, name, NAME_FIELD_SIZE);
    }

    return bce_alias;
}

/*
{
  "uuid": "str",
  "arg_type": "NONE|OPTION|FILE|TEXT",
  "description": "str",
  "long_name": "str",
  "short_name": "str",
  "opts": []
}
 */
static bce_command_arg_t *bce_command_arg_from_json(const char *cmd_uuid, struct json_object *j_arg) {
    bce_command_arg_t *bce_arg = create_bce_command_arg();
    json_object *j_obj = NULL;

    strncat(bce_arg->cmd_uuid, cmd_uuid, UUID_FIELD_SIZE);

    j_obj = json_object_object_get(j_arg, "uuid");
    if (j_obj) {
        const char *uuid = json_object_get_string(j_obj);
        strncat(bce_arg->uuid, uuid, UUID_FIELD_SIZE);
    } else {
        uuid4_generate(bce_arg->uuid);
    }
    j_obj = json_object_object_get(j_arg, "arg_type");
    if (j_obj) {
        const char *arg_type = json_object_get_string(j_obj);
        strncat(bce_arg->arg_type, arg_type, CMD_TYPE_FIELD_SIZE);
    }
    j_obj = json_object_object_get(j_arg, "description");
    if (j_obj) {
        const char *description = json_object_get_string(j_obj);
        strncat(bce_arg->description, description, DESCRIPTION_FIELD_SIZE);
    }
    j_obj = json_object_object_get(j_arg, "long_name");
    if (j_obj) {
        const char *long_name = json_object_get_string(j_obj);
        strncat(bce_arg->long_name, long_name, NAME_FIELD_SIZE);
    }
    j_obj = json_object_object_get(j_arg, "short_name");
    if (j_obj) {
        const char *short_name = json_object_get_string(j_obj);
        strncat(bce_arg->short_name, short_name, SHORTNAME_FIELD_SIZE);
    }

    j_obj = json_object_object_get(j_arg, "opts");
    if (j_obj) {
        if (json_object_is_type(j_obj, json_type_array)) {
            size_t len = json_object_array_length(j_obj);
            for (size_t i = 0; i < len; i++) {
                struct json_object *j_opt = json_object_array_get_idx(j_obj, i);
                bce_command_opt_t *opt = bce_command_opt_from_json(bce_arg->uuid, j_opt);
                ll_append_item(bce_arg->opts, opt);
            }
        }
    }

    return bce_arg;
}

/*
{
  "uuid": "str",
  "name": "str"
}
 */
static bce_command_opt_t *bce_command_opt_from_json(const char *arg_uuid, struct json_object *j_opt) {
    bce_command_opt_t *bce_opt = create_bce_command_opt();
    json_object *j_obj = NULL;

    j_obj = json_object_object_get(j_opt, "uuid");
    if (j_obj) {
        const char *uuid = json_object_get_string(j_obj);
        strncat(bce_opt->uuid, uuid, UUID_FIELD_SIZE);
    } else {
        uuid4_generate(bce_opt->uuid);
    }
    j_obj = json_object_object_get(j_opt, "name");
    if (j_obj) {
        const char *name = json_object_get_string(j_obj);
        strncat(bce_opt->name, name, NAME_FIELD_SIZE);
    }

    strncat(bce_opt->cmd_arg_uuid, arg_uuid, UUID_FIELD_SIZE);
    return bce_opt;
}

static json_object *bce_command_to_json(bce_command_t *cmd) {
    if (!cmd) {
        return NULL;
    }

    // create command attributes
    struct json_object *j_command = json_object_new_object();
    json_object_object_add(j_command, "uuid", json_object_new_string(cmd->uuid));
    json_object_object_add(j_command, "name", json_object_new_string(cmd->name));
    // don't encode parent_cmd (json is already hierarchical)

    // array of aliases
    struct json_object *j_aliases = json_object_new_array();
    if (cmd->aliases) {
        linked_list_node_t *alias_node = cmd->aliases->head;
        while (alias_node) {
            bce_command_alias_t *alias = (bce_command_alias_t *) alias_node->data;
            if (alias) {
                json_object *j_alias = bce_command_alias_to_json(alias);
                json_object_array_add(j_aliases, j_alias);
            }
            alias_node = alias_node->next;
        }
    }
    json_object_object_add(j_command, "aliases", j_aliases);

    // array of args
    struct json_object *j_args = json_object_new_array();
    if (cmd->args) {
        linked_list_node_t *arg_node = cmd->args->head;
        while (arg_node) {
            bce_command_arg_t *arg = (bce_command_arg_t *) arg_node->data;
            if (arg) {
                json_object *j_arg = bce_command_arg_to_json(arg);
                json_object_array_add(j_args, j_arg);
            }
            arg_node = arg_node->next;
        }
    }
    json_object_object_add(j_command, "args", j_args);

    // array of sub-commands (recurse)
    struct json_object *j_subs = json_object_new_array();
    if (cmd->sub_commands) {
        linked_list_node_t *sub_node = cmd->sub_commands->head;
        while (sub_node) {
            bce_command_t *sub_cmd = (bce_command_t *) sub_node->data;
            if (sub_cmd) {
                json_object *j_sub = bce_command_to_json(sub_cmd);
                json_object_array_add(j_subs, j_sub);
            }
            sub_node = sub_node->next;
        }
    }
    json_object_object_add(j_command, "sub_commands", j_subs);

    return j_command;
}

static json_object *bce_command_alias_to_json(bce_command_alias_t *alias) {
    struct json_object *j_alias = json_object_new_object();
    json_object_object_add(j_alias, "uuid", json_object_new_string(alias->uuid));
    json_object_object_add(j_alias, "name", json_object_new_string(alias->name));
    return j_alias;
}

static json_object *bce_command_arg_to_json(bce_command_arg_t *arg) {
    struct json_object *j_arg = json_object_new_object();
    json_object_object_add(j_arg, "uuid", json_object_new_string(arg->uuid));
    json_object_object_add(j_arg, "arg_type", json_object_new_string(arg->arg_type));
    json_object_object_add(j_arg, "description", json_object_new_string(arg->description));
    json_object_object_add(j_arg, "long_name", json_object_new_string(arg->long_name));
    json_object_object_add(j_arg, "short_name", json_object_new_string(arg->short_name));
    struct json_object *j_opts = json_object_new_array();
    if (arg->opts) {
        linked_list_node_t *opt_node = arg->opts->head;
        while (opt_node) {
            bce_command_opt_t *opt = (bce_command_opt_t *) opt_node->data;
            if (opt) {
                // convert the option to json
                json_object *j_opt = bce_command_opt_to_json(opt);
                // append to the json array
                json_object_array_add(j_opts, j_opt);
            }
            opt_node = opt_node->next;
        }
    }
    json_object_object_add(j_arg, "opts", j_opts);
    return j_arg;
}

static json_object *bce_command_opt_to_json(bce_command_opt_t *opt) {
    struct json_object *j_opt = json_object_new_object();
    json_object_object_add(j_opt, "uuid", json_object_new_string(opt->uuid));
    json_object_object_add(j_opt, "name", json_object_new_string(opt->name));
    return j_opt;
}

static sqlite3 *open_db_with_xa(const char *filename, int *rc) {
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
        if (!create_schema(conn)) {
            fprintf(stderr, "Unable to create database schema. database: %s\n", filename);
            return NULL;
        }
        schema_version = get_schema_version(conn);
    }
    if (schema_version != SCHEMA_VERSION) {
        fprintf(stderr, "Schema version mismatch. database: %s, expected: %d, found: %d\n", filename, SCHEMA_VERSION,
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

