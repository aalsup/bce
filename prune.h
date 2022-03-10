#ifndef BCE_PRUNE_H
#define BCE_PRUNE_H

#include "input.h"
#include "data_model.h"

void prune_command(bce_command_t *cmd, const completion_input_t *input);

/* Collect recommendations that should appear first in the list */
bool collect_required_recommendations(linked_list_t *recommendation_list, const bce_command_t *cmd, const char *current_word, const char *previous_word);

/* Collect remaining recommendations */
bool collect_secondary_recommendations(linked_list_t *recommendation_list, const bce_command_t *cmd, const char *current_word, const char *previous_word);

/* Determine if the user's cursor is positioned at a `command_arg` */
bce_command_arg_t *get_current_arg(const bce_command_t *cmd, const char *current_word);

#endif // BCE_PRUNE_H
