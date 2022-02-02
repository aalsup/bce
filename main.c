#undef DEBUG

#include <stdio.h>
#include <string.h>
#include <sqlite3.h>
#include <wordexp.h>
#include "linked_list.h"
#include "dbutil.h"
#include "completion_model.h"
#include "completion_input.h"
#include "error.h"
#include "prune.h"

void collect_recommendations(linked_list_t *recommendation_list, completion_command_t *cmd);
void print_recommendations(linked_list_t *recommendation_list);

int main() {
    int err = 0;    // custom error values
    int rc = 0;     // SQLite return values
    completion_command_t *completion_command = NULL;
    char command_name[MAX_LINE_SIZE + 1];
    char current_word[MAX_LINE_SIZE + 1];
    char previous_word[MAX_LINE_SIZE + 1];

#ifdef DEBUG
    printf("SQLite version %s\n", sqlite3_libversion());
#endif

    sqlite3 *conn = open_database("completion.db", &rc);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Error %d opening database", rc);
        err = ERR_OPEN_DATABASE;
        goto done;
    }

    if (!ensure_schema(conn)) {
        if (!create_schema(conn, &rc)) {
            fprintf(stderr, "Unable to create database schema\n");
            err = ERR_DATABASE_SCHEMA;
            goto done;
        }
    }

    err = load_completion_input();
    if (err != 0) {
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
    if (!get_command_input(command_name, MAX_LINE_SIZE)) {
        fprintf(stderr, "Unable to determine command\n");
        goto done;
    }
    get_current_word(current_word, MAX_LINE_SIZE);
    get_previous_word(previous_word, MAX_LINE_SIZE);

#ifdef DEBUG
    printf("input: %s\n", completion_input.line);
    printf("command: %s\n", command_name);
    printf("current_word: %s\n", current_word);
    printf("previous_word: %s\n", previous_word);
#endif

    // search for the command directly (load all descendents)
    completion_command = create_completion_command();
    rc = get_db_command(completion_command, conn, command_name);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "get_db_command() returned %d\n", rc);
        goto done;
    }

#ifdef DEBUG
    printf("\nCommand Tree (Database)\n");
    print_command_tree(conn, completion_command, 0);
#endif

    prune_command(completion_command);

#ifdef DEBUG
    printf("\nCommand Tree (Pruned)\n");
    print_command_tree(conn, completion_command, 0);
#endif

    linked_list_t *recommendation_list = ll_create();
    collect_recommendations(recommendation_list, completion_command);
    print_recommendations(recommendation_list);
    ll_destroy(&recommendation_list, NULL);

    done:
    free_completion_command(&completion_command);
    sqlite3_close(conn);

    return err;
}

// TODO: prioritize recommendations, bubble up most relevant to earlier in the list.
// For example: `kubectl get pods <tab>` should show args/opts for `pods`, then `get`, and finally `kubectl`
void collect_recommendations(linked_list_t *recommendation_list, completion_command_t *cmd) {
    if (cmd == NULL) {
        return;
    }

    // collect all the sub-commands
    linked_list_t *sub_cmd_list = cmd->sub_commands;
    if (sub_cmd_list != NULL) {
        linked_list_node_t *sub_cmd_node = sub_cmd_list->head;
        while (sub_cmd_node != NULL) {
            completion_command_t *sub_cmd = (completion_command_t *) sub_cmd_node->data;
            if ((sub_cmd != NULL) && (!sub_cmd->is_present_on_cmdline)) {
                char *data = calloc(NAME_FIELD_SIZE, sizeof(char));
                strncat(data, sub_cmd->name, NAME_FIELD_SIZE);
                if (sub_cmd->aliases != NULL) {
                    char *shortest = NULL;
                    linked_list_node_t *alias_node = sub_cmd->aliases->head;
                    while (alias_node != NULL) {
                        if (shortest == NULL) {
                            shortest = alias_node->data;
                        } else {
                            if (strlen(alias_node->data) < strlen(shortest)) {
                                shortest = alias_node->data;
                            }
                        }
                        alias_node = alias_node->next;
                    }
                    // show the shortest alias
                    if (shortest != NULL) {
                        strcat(data, " (");
                        strncat(data, shortest, NAME_FIELD_SIZE);
                        strcat(data, ")");
                    }
                }
                ll_append_item(recommendation_list, data);
            }
            collect_recommendations(recommendation_list, sub_cmd);
            sub_cmd_node = sub_cmd_node->next;
        }
    }

    // collect all the args
    linked_list_t *arg_list = cmd->command_args;
    if (arg_list != NULL) {
        linked_list_node_t *arg_node = arg_list->head;
        while (arg_node != NULL) {
            completion_command_arg_t *arg = (completion_command_arg_t *) arg_node->data;
            if (arg != NULL) {
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
                    linked_list_t *opt_list = arg->opts;
                    if (opt_list != NULL) {
                        linked_list_node_t *opt_node = opt_list->head;
                        while (opt_node != NULL) {
                            completion_command_opt_t *opt = (completion_command_opt_t *) opt_node->data;
                            if (opt != NULL) {
                                char *data = calloc(NAME_FIELD_SIZE, sizeof(char));
                                strncat(data, opt->name, NAME_FIELD_SIZE);
                                ll_append_item(recommendation_list, data);
                            }
                            opt_node = opt_node->next;
                        }
                    }
                }
            }
            arg_node = arg_node->next;
        }
    }
}

void print_recommendations(linked_list_t *recommendation_list) {
    if (recommendation_list == NULL) {
        return;
    }

    linked_list_node_t* node = recommendation_list->head;
    while (node != NULL) {
        printf("%s\n", (char *)node->data);
        node = node->next;
    }

}