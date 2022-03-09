#include <stdio.h>
#include <string.h>
#include <sqlite3.h>
#include "linked_list.h"
#include "dbutil.h"
#include "data_model.h"
#include "input.h"
#include "error.h"
#include "prune.h"
#include "cli.h"

#define DEBUG

bce_error_t process_completion(void);

void collect_primary_recommendations(linked_list_t *recommendation_list, const bce_command_t *cmd, const char *current_word, const char *previous_word);

void collect_secondary_recommendations(linked_list_t *recommendation_list, const bce_command_t *cmd, const char *current_word, const char *previous_word);

bce_command_arg_t *get_current_arg(const bce_command_t *cmd, const char *current_word);

void print_recommendations(const linked_list_t *recommendation_list);

int main(int argc, char **argv) {
    if (argc <= 1) {
        // called from BASH (for completion help)
        return process_completion();
    }

    return process_cli(argc, (const char **) argv);
}

bce_error_t process_completion(void) {
    bce_error_t err = ERR_NONE;    // custom error values
    int rc = 0;     // SQLite return values
    char command_name[MAX_LINE_SIZE + 1];
    char current_word[MAX_LINE_SIZE + 1];
    char previous_word[MAX_LINE_SIZE + 1];

#ifdef DEBUG
    printf("SQLite version %s\n", sqlite3_libversion());
#endif

    sqlite3 *conn = open_database(BCE_DB__FILENAME, &rc);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Error %d opening database", rc);
        err = ERR_OPEN_DATABASE;
        goto done;
    }

    int schema_version = get_schema_version(conn);
    if (schema_version == 0) {
        // create the schema
        err = create_schema(conn);
        if (err != ERR_NONE) {
            fprintf(stderr, "Unable to create database schema\n");
            goto done;
        }
        schema_version = get_schema_version(conn);
    }
    if (schema_version != SCHEMA_VERSION) {
        fprintf(stderr, "Schema version %d does not match expected version %d\n", schema_version, SCHEMA_VERSION);
        err = ERR_DATABASE_SCHEMA_VERSION_MISMATCH;
        goto done;
    }

    completion_input_t *input = create_completion_input(&err);
    if (err != ERR_NONE) {
        switch (err) {
            case ERR_MISSING_ENV_COMP_LINE:
                fprintf(stderr, "No %s env var\n", BASH_LINE_VAR);
            case ERR_MISSING_ENV_COMP_POINT:
                fprintf(stderr, "No %s env var\n", BASH_CURSOR_VAR);
            default:
                fprintf(stderr, "Unknown error: %d\n", err);
        }
        goto done;
    }

    // load the data provided by environment
    if (!get_command_from_input(input, command_name, MAX_LINE_SIZE)) {
        fprintf(stderr, "Unable to determine command\n");
        err = ERR_INVALID_CMD_NAME;
        goto done;
    }
    get_current_word(input, current_word, MAX_LINE_SIZE);
    get_previous_word(input, previous_word, MAX_LINE_SIZE);

#ifdef DEBUG
    printf("input: %s\n", input->line);
    printf("command: %s\n", command_name);
    printf("current_word: %s\n", current_word);
    printf("previous_word: %s\n", previous_word);
#endif

    // explicitly start a transaction, since this will be done automatically (per statement) otherwise
    rc = sqlite3_exec(conn, "BEGIN TRANSACTION;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "begin transaction returned %d\n", rc);
        err = ERR_SQLITE_ERROR;
        goto done;
    }

    // load the statement cache (prevent re-parsing statements)
    rc = prepare_statement_cache(conn);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Unable to load statement cache. err=%d\n", rc);
        err = ERR_SQLITE_ERROR;
        goto done;
    }

    // search for the command directly (load all descendents)
    bce_command_t *completion_command = create_bce_command();
    rc = load_db_command(conn, completion_command, command_name);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "load_db_command() returned %d\n", rc);
        err = ERR_SQLITE_ERROR;
        goto done;
    }

    rc = sqlite3_exec(conn, "COMMIT;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "commit transaction returned %d\n", rc);
        err = ERR_SQLITE_ERROR;
        goto done;
    }

#ifdef DEBUG
    printf("\nCommand Tree (Database)\n");
    print_command_tree(completion_command, 0);
#endif

    // remove non-relevant command data
    prune_command(completion_command, input);

#ifdef DEBUG
    printf("\nCommand Tree (Pruned)\n");
    print_command_tree(completion_command, 0);
#endif

    // build the command recommendations
    linked_list_t *recommendation_list = ll_create_unique(NULL);
    collect_primary_recommendations(recommendation_list, completion_command, current_word, previous_word);
    collect_secondary_recommendations(recommendation_list, completion_command, current_word, previous_word);

#ifdef DEBUG
    printf("\nRecommendations (Prioritized)\n");
#endif
    print_recommendations(recommendation_list);
    recommendation_list = ll_destroy(recommendation_list);

    done:
    input = free_completion_input(input);
    completion_command = free_bce_command(completion_command);
    rc = free_statement_cache(conn);
    sqlite3_close(conn);

    return err;
}

void collect_primary_recommendations(linked_list_t *recommendation_list, const bce_command_t *cmd, const char *current_word, const char *previous_word) {
    if (!recommendation_list || !cmd) {
        return;
    }

    bce_command_arg_t *arg = get_current_arg(cmd, current_word);

    if (arg->opts) {
        linked_list_node_t *opt_node = arg->opts->head;
        while (opt_node) {
            char *data = calloc(NAME_FIELD_SIZE, sizeof(char));
            bce_command_opt_t *opt = (bce_command_opt_t *) opt_node->data;
            strncat(data, opt->name, NAME_FIELD_SIZE);
            ll_append_item(recommendation_list, data);
            opt_node = opt_node->next;
        }
    } 
}

void collect_secondary_recommendations(linked_list_t *recommendation_list, const bce_command_t *cmd, const char *current_word, const char *previous_word) {
    if (!cmd) {
        return;
    }

    // collect all the sub-commands
    if (cmd->sub_commands) {
        linked_list_node_t *sub_cmd_node = cmd->sub_commands->head;
        while (sub_cmd_node) {
            bce_command_t *sub_cmd = (bce_command_t *) sub_cmd_node->data;
            if (sub_cmd && !sub_cmd->is_present_on_cmdline) {
                char *data = calloc(NAME_FIELD_SIZE, sizeof(char));
                strncat(data, sub_cmd->name, NAME_FIELD_SIZE);
                if (sub_cmd->aliases) {
                    char *shortest = NULL;
                    linked_list_node_t *alias_node = sub_cmd->aliases->head;
                    while (alias_node) {
                        if (!shortest) {
                            shortest = alias_node->data;
                        } else {
                            if (strlen(alias_node->data) < strlen(shortest)) {
                                shortest = alias_node->data;
                            }
                        }
                        alias_node = alias_node->next;
                    }
                    // show the shortest alias
                    if (shortest) {
                        strcat(data, " (");
                        strncat(data, shortest, NAME_FIELD_SIZE);
                        strcat(data, ")");
                    }
                }
                ll_append_item(recommendation_list, data);
            }
            collect_secondary_recommendations(recommendation_list, sub_cmd, current_word, previous_word);
            sub_cmd_node = sub_cmd_node->next;
        }
    }

    // collect all the args
    if (cmd->args) {
        linked_list_node_t *arg_node = cmd->args->head;
        while (arg_node) {
            bce_command_arg_t *arg = (bce_command_arg_t *) arg_node->data;
            if (arg) {
                if (!arg->is_present_on_cmdline) {
                    char *arg_str = calloc(MAX_LINE_SIZE, sizeof(char));
                    if (strlen(arg->long_name) > 0) {
                        strncat(arg_str, arg->long_name, NAME_FIELD_SIZE);
                        if (strlen(arg->short_name) > 0) {
                            strcat(arg_str, " (");
                            strncat(arg_str, arg->short_name, SHORTNAME_FIELD_SIZE);
                            strcat(arg_str, ")");
                        }
                    } else {
                        strncat(arg_str, arg->short_name, SHORTNAME_FIELD_SIZE);
                    }
                    ll_append_item(recommendation_list, arg_str);
                } else {
                    // collect all the options
                    if (arg->opts) {
                        linked_list_node_t *opt_node = arg->opts->head;
                        while (opt_node) {
                            char *data = calloc(NAME_FIELD_SIZE, sizeof(char));
                            bce_command_opt_t *opt = (bce_command_opt_t *) opt_node->data;
                            strncat(data, opt->name, NAME_FIELD_SIZE);
                            ll_append_item(recommendation_list, data);
                            opt_node = opt_node->next;
                        }
                    }
                }
            }
            arg_node = arg_node->next;
        }
    }
}

bce_command_arg_t *get_current_arg(const bce_command_t *cmd, const char *current_word) {
    if (!cmd || !current_word) {
        return NULL;
    }

    bce_command_arg_t *found_arg = NULL;

    if (cmd->args) {
        linked_list_node_t *arg_node = cmd->args->head;
        while (arg_node) {
            bce_command_arg_t *arg = (bce_command_arg_t *) arg_node->data;
            if (arg) {
                if (arg->is_present_on_cmdline) {
                    if ((strncmp(arg->long_name, current_word, NAME_FIELD_SIZE) == 0) ||
                            (strncmp(arg->short_name, current_word, SHORTNAME_FIELD_SIZE) == 0)) {
                        found_arg = arg;
                        break;
                    }
                }
            }
            arg_node = arg_node->next;
        }
    }

    if (found_arg) {
        return found_arg;
    }

    // recurse for sub-commands
    if (cmd->sub_commands) {
        linked_list_node_t *sub_node = cmd->sub_commands->head;
        while (sub_node) {
            bce_command_t *sub = (bce_command_t *) sub_node->data;
            found_arg = get_current_arg(sub, current_word);
            if (found_arg) {
                break;
            }
            sub_node = sub_node->next;
        }
    }

    return found_arg;
}

void print_recommendations(const linked_list_t *recommendation_list) {
    if (!recommendation_list) {
        return;
    }

    linked_list_node_t *node = recommendation_list->head;
    while (node) {
        printf("%s\n", (char *) node->data);
        node = node->next;
    }
}
