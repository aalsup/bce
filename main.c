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

/* Normal BASH completion processing */
bce_error_t process_completion(void);

/* CLI options, such as 'import', 'export' */
bce_error_t process_cli(int argc, const char **argv);

/* Display the recommendations to stdout */
void print_recommendations(const linked_list_t *recommendation_list);

void print_command_tree(const bce_command_t *cmd, int level);

int main(int argc, char **argv) {
    int result = 0;

    if (argc <= 1) {
        // called from BASH (for completion help)
        result = process_completion();
    } else {
        // called with CLI args
        result = process_cli(argc, (const char **) argv);
    }

    return result;
}

bce_error_t process_cli(int argc, const char **argv) {
    return process_cli_impl(argc, argv);
}

/* Program called from BASH shell, for completion assistance to user */
bce_error_t process_completion(void) {
    bce_error_t err = ERR_NONE;    // custom error values
    int rc = 0;     // SQLite return values
    char command_name[MAX_LINE_SIZE + 1];
    char current_word[MAX_LINE_SIZE + 1];
    char previous_word[MAX_LINE_SIZE + 1];

#ifdef DEBUG
    printf("SQLite version %s\n", sqlite3_libversion());
#endif

    sqlite3 *conn = db_open(BCE_DB__FILENAME, &rc);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Error %d opening database", rc);
        err = ERR_OPEN_DATABASE;
        goto done;
    }

    int schema_version = db_get_schema_version(conn);
    if (schema_version == 0) {
        // create the schema
        err = db_create_schema(conn);
        if (err != ERR_NONE) {
            fprintf(stderr, "Unable to create database schema\n");
            goto done;
        }
        schema_version = db_get_schema_version(conn);
    }
    if (schema_version != DB_SCHEMA_VERSION) {
        fprintf(stderr, "Schema version %d does not match expected version %d\n", schema_version, DB_SCHEMA_VERSION);
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
    rc = db_prepare_stmt_cache(conn);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Unable to load statement cache. err=%d\n", rc);
        err = ERR_SQLITE_ERROR;
        goto done;
    }

    // search for the command directly (load all descendents)
    bce_command_t *completion_command = bce_command_new();
    rc = db_query_command(conn, completion_command, command_name);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "db_query_command() returned %d\n", rc);
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
    bool has_required = collect_required_recommendations(recommendation_list, completion_command, current_word, previous_word);
    if (!has_required) {
        collect_optional_recommendations(recommendation_list, completion_command, current_word, previous_word);
    }

#ifdef DEBUG
    if (has_required) {
        printf("\nRecommendations (Required)\n");
    } else {
        printf("\nRecommendations (Optional)\n");
    }
#endif

    // display the list of recommended completions
    print_recommendations(recommendation_list);

    done:
    // dispose of everything
    input = free_completion_input(input);
    recommendation_list = ll_destroy(recommendation_list);
    completion_command = bce_command_free(completion_command);
    rc = db_free_stmt_cache(conn);
    sqlite3_close(conn);

    return err;
}

void print_command_tree(const bce_command_t *cmd, const int level) {
    // indent
    for (int i = 0; i < level; i++) {
        printf("  ");
    }
    printf("command: %s\n", cmd->name);

    if (cmd->aliases && (cmd->aliases->size > 0)) {
        for (int i = 0; i < level; i++) {
            printf("  ");
        }
        printf("  aliases: ");
        for (linked_list_node_t *alias_node = cmd->aliases->head; alias_node != NULL; alias_node = alias_node->next) {
            bce_command_alias_t *alias = (bce_command_alias_t *) alias_node->data;
            printf("%s ", alias->name);
        }
        printf("\n");
    }

    if (cmd->args) {
        for (linked_list_node_t *arg_node = cmd->args->head; arg_node != NULL; arg_node = arg_node->next) {
            bce_command_arg_t *arg = (bce_command_arg_t *) arg_node->data;
            if (arg) {
                for (int i = 0; i < level; i++) {
                    printf("  ");
                }
                printf("  arg: %s (%s): %s\n", arg->long_name, arg->short_name, arg->arg_type);

                // print opts
                if (arg->opts) {
                    for (linked_list_node_t *opt_node = arg->opts->head; opt_node != NULL; opt_node = opt_node->next) {
                        bce_command_opt_t *opt = (bce_command_opt_t *) opt_node->data;
                        if (opt) {
                            for (int i = 0; i < level; i++) {
                                printf("  ");
                            }
                            printf("    opt: %s\n", opt->name);
                        }
                    }
                }
            }
        }
    }

    if (cmd->sub_commands) {
        for (linked_list_node_t *node = cmd->sub_commands->head; node != NULL; node = node->next) {
            bce_command_t *sub_cmd = (bce_command_t *) node->data;
            print_command_tree(sub_cmd, level + 1);
        }
    }
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
