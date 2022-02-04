#include "prune.h"
#include "completion_model.h"
#include "completion_input.h"
#include "linked_list.h"

void prune_sub_commands(completion_command_t* cmd, const linked_list_t *word_list);
void prune_arguments(completion_command_t* cmd, const linked_list_t *word_list);

/*
 * Find the sub-commands and arguments related to the given command.
 * Prune the results based on the current command_line
 */
void prune_command(completion_command_t* cmd) {
    // build a list of words from the command line
    linked_list_t *word_list = ll_string_to_list(completion_input.line, " ", MAX_LINE_SIZE);

    prune_arguments(cmd, word_list);
    prune_sub_commands(cmd, word_list);

    ll_destroy(&word_list);
    return;
}

/*
 * Iterate over the sub-commands and prune any sibling sub-commands.
 */
void prune_sub_commands(completion_command_t* cmd, const linked_list_t *word_list) {
    if (!cmd || !cmd->sub_commands) {
        return;
    }

    // prune sibling sub-commands
    linked_list_t *sub_cmds = cmd->sub_commands;
    linked_list_node_t *check_node = sub_cmds->head;
    while (check_node) {
        completion_command_t *sub_cmd = (completion_command_t *)check_node->data;
        // check if cmd_name is in word_list
        sub_cmd->is_present_on_cmdline = ll_is_string_in_list(word_list, sub_cmd->name);
        if (!sub_cmd->is_present_on_cmdline) {
            // try harder - examine the aliases
            sub_cmd->is_present_on_cmdline = ll_is_any_in_list(word_list, sub_cmd->aliases);
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
        check_node = check_node->next;
    }

    // recurse over the remaining sub-cmds
    linked_list_node_t *sub_node = sub_cmds->head;
    while (sub_node) {
        completion_command_t *sub_cmd = (completion_command_t *)sub_node->data;
        prune_arguments(sub_cmd, word_list);
        prune_sub_commands(sub_cmd, word_list);
        sub_node = sub_node->next;
    }

    sub_node = sub_cmds->head;
    while (sub_node) {
        bool node_deleted = false;
        completion_command_t *sub_cmd = (completion_command_t *)sub_node->data;
        // if a sub-command is present and has no children, it has been used and should be pruned
        if ((sub_cmd->is_present_on_cmdline)
            && (sub_cmd->sub_commands) && (sub_cmd->sub_commands->size == 0)
            && (sub_cmd->args) && (sub_cmd->args->size == 0))
        {
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
void prune_arguments(completion_command_t* cmd, const linked_list_t *word_list) {
    if (!cmd) {
        return;
    }

    linked_list_t *args = cmd->args;
    if (args) {
        linked_list_node_t *arg_node = args->head;
        while (arg_node) {
            bool arg_removed = false;
            completion_command_arg_t *arg = (completion_command_arg_t *)arg_node->data;
            // check if arg_name is in word_list
            if (ll_is_string_in_list(word_list, arg->short_name) || ll_is_string_in_list(word_list, arg->long_name)) {
                arg->is_present_on_cmdline = true;
                // check if the arg has options
                if (arg->opts) {
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
                            completion_command_opt_t *opt = (completion_command_opt_t *) opt_node->data;
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
