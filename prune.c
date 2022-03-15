#include <string.h>
#include "prune.h"
#include "data_model.h"
#include "input.h"
#include "linked_list.h"

static void prune_sub_commands(bce_command_t *cmd, const linked_list_t *word_list);

static void prune_arguments(bce_command_t *cmd, const linked_list_t *word_list);

/*
 * Find the sub-commands and arguments related to the given command.
 * Prune the results based on the current command_line
 */
void prune_command(bce_command_t *cmd, const completion_input_t *input) {
    // build a list of words from the command line
    linked_list_t *word_list = ll_string_to_list(input->line, " ", MAX_LINE_SIZE);

    prune_arguments(cmd, word_list);
    prune_sub_commands(cmd, word_list);

    word_list = ll_destroy(word_list);
    return;
}

/*
 * Iterate over the sub-commands and prune any sibling sub-commands.
 */
static void prune_sub_commands(bce_command_t *cmd, const linked_list_t *word_list) {
    if (!cmd || !cmd->sub_commands) {
        return;
    }

    // prune sibling sub-commands
    linked_list_t *sub_cmds = cmd->sub_commands;
    linked_list_node_t *check_node = sub_cmds->head;
    while (check_node) {
        bce_command_t *sub_cmd = (bce_command_t *) check_node->data;
        if (sub_cmd) {
            // check if cmd_name is in word_list
            sub_cmd->is_present_on_cmdline = ll_is_string_in_list(word_list, sub_cmd->name);
            if (!sub_cmd->is_present_on_cmdline) {
                // try harder - examine the aliases
                if (sub_cmd->aliases && (sub_cmd->aliases->size > 0)) {
                    linked_list_node_t *check_alias_node = sub_cmd->aliases->head;
                    while (check_alias_node) {
                        bce_command_alias_t *alias = (bce_command_alias_t *)check_alias_node->data;
                        bool found_alias = ll_is_string_in_list(word_list, alias->name);
                        if (found_alias) {
                            sub_cmd->is_present_on_cmdline = true;
                            break;
                        }
                        check_alias_node = check_alias_node->next;
                    }
                    sub_cmd->is_present_on_cmdline = ll_is_any_in_list(word_list, sub_cmd->aliases);

                }
            }
            if (sub_cmd->is_present_on_cmdline) {
                // remove the sub_command's siblings
                linked_list_node_t *candidate_node = sub_cmds->head;
                while (candidate_node) {
                    if (candidate_node->id == check_node->id) {
                        // skip
                        candidate_node = candidate_node->next;
                    } else {
                        // remove candidate_node
                        linked_list_node_t *next_node = candidate_node->next;
                        ll_remove_item(sub_cmds, candidate_node);
                        candidate_node = next_node;
                    }
                }
            }
        }
        check_node = check_node->next;
    }

    // recurse over the remaining sub-cmds
    linked_list_node_t *sub_node = sub_cmds->head;
    while (sub_node) {
        bce_command_t *sub_cmd = (bce_command_t *) sub_node->data;
        prune_arguments(sub_cmd, word_list);
        prune_sub_commands(sub_cmd, word_list);
        sub_node = sub_node->next;
    }

    sub_node = sub_cmds->head;
    while (sub_node) {
        bool node_deleted = false;
        bce_command_t *sub_cmd = (bce_command_t *) sub_node->data;
        // if a sub-command is present and has no children, it has been used and should be pruned
        if ((sub_cmd->is_present_on_cmdline)
            && (sub_cmd->sub_commands) && (sub_cmd->sub_commands->size == 0)
            && (sub_cmd->args) && (sub_cmd->args->size == 0)) {
            linked_list_node_t *next_node = sub_node->next;
            ll_remove_item(sub_cmds, sub_node);
            sub_node = next_node;
            node_deleted = true;
        }
        if (!node_deleted) {
            sub_node = sub_node->next;
        }
    }
}

/*
 * Find the arguments related to the current command.
 * Remove any arguments (without options) that have already been used.
 */
static void prune_arguments(bce_command_t *cmd, const linked_list_t *word_list) {
    if (!cmd) {
        return;
    }

    linked_list_t *args = cmd->args;
    if (args && (args->size > 0)) {
        linked_list_node_t *arg_node = args->head;
        while (arg_node) {
            bool arg_removed = false;
            bce_command_arg_t *arg = (bce_command_arg_t *) arg_node->data;
            // check if arg_name is in word_list
            if (ll_is_string_in_list(word_list, arg->short_name) || ll_is_string_in_list(word_list, arg->long_name)) {
                arg->is_present_on_cmdline = true;
                // check if the arg has options
                if (arg->opts && (arg->opts->size > 0)) {
                    linked_list_t *opts = arg->opts;
                    bool should_remove_arg = false;
                    if (opts->size == 0) {
                        should_remove_arg = true;
                    } else {
                        // possibly remove the arg, if an option has already been supplied
                        should_remove_arg = true;
                        linked_list_node_t *opt_node = opts->head;
                        while (opt_node) {
                            should_remove_arg = false;
                            bce_command_opt_t *opt = (bce_command_opt_t *) opt_node->data;
                            if (ll_is_string_in_list(word_list, opt->name)) {
                                should_remove_arg = true;
                                break;
                            }
                            opt_node = opt_node->next;
                        }
                    }
                    if (should_remove_arg) {
                        linked_list_node_t *next_node = arg_node->next;
                        ll_remove_item(args, arg_node);
                        arg_node = next_node;
                        arg_removed = true;
                    }
                }
            }
            // if we removed the arg, we've already moved forward
            if (!arg_removed) {
                arg_node = arg_node->next;
            }
        }
    }
}

bool collect_required_recommendations(linked_list_t *recommendation_list, const bce_command_t *cmd,
                                      const char *current_word, const char *previous_word) {
    if (!recommendation_list || !cmd) {
        return false;
    }

    bool result = false;

    // if a current argument is selected, its options should be displayed 1st
    bce_command_arg_t *arg = get_current_arg(cmd, current_word);
    if (!arg) {
        return result;
    }

    if (arg->opts && (arg->opts->size > 0)) {
        // if the arg_type is NONE, don't expect options
        if (strncmp(arg->arg_type, "NONE", CMD_TYPE_FIELD_SIZE) != 0) {
            result = true;
            linked_list_node_t *opt_node = arg->opts->head;
            while (opt_node) {
                char *data = calloc(NAME_FIELD_SIZE + 1, sizeof(char));
                bce_command_opt_t *opt = (bce_command_opt_t *) opt_node->data;
                strncat(data, opt->name, NAME_FIELD_SIZE);
                ll_append_item(recommendation_list, data);
                opt_node = opt_node->next;
            }
        }
    }

    return result;
}

bool collect_optional_recommendations(linked_list_t *recommendation_list, const bce_command_t *cmd, const char *current_word,
                                 const char *previous_word) {
    if (!recommendation_list || !cmd) {
        return false;
    }

    // collect all the sub-commands
    if (cmd->sub_commands) {
        linked_list_node_t *sub_cmd_node = cmd->sub_commands->head;
        while (sub_cmd_node) {
            bce_command_t *sub_cmd = (bce_command_t *) sub_cmd_node->data;
            if (sub_cmd && !sub_cmd->is_present_on_cmdline) {
                char *data = calloc(NAME_FIELD_SIZE + 1, sizeof(char));
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
            collect_optional_recommendations(recommendation_list, sub_cmd, current_word, previous_word);
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
                    char *arg_str = calloc(MAX_LINE_SIZE + 1, sizeof(char));
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
                    if (arg->opts && (arg->opts->size > 0)) {
                        linked_list_node_t *opt_node = arg->opts->head;
                        while (opt_node) {
                            char *data = calloc(NAME_FIELD_SIZE + 1, sizeof(char));
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

    return true;
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
