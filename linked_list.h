#include <stdlib.h>
#include <stdbool.h>

#ifndef LINKED_LIST_H
#define LINKED_LIST_H

typedef struct linked_list_node_t {
    int id;
    void *data;
    struct linked_list_node_t *next;
} linked_list_node_t;

typedef struct linked_list_t {
    size_t size;
    linked_list_node_t *head;
} linked_list_t;

linked_list_t* ll_create();
bool ll_destroy(linked_list_t **list);
bool ll_append_item(linked_list_t *list, void *data);
bool ll_remove_item(linked_list_t *list, linked_list_node_t *node);
void* ll_get_nth_item(const linked_list_t* list, size_t elem);
linked_list_t *ll_string_to_list(const char *str, const char *delim, size_t max_len);
bool ll_is_string_in_list(const linked_list_t* list, const char *str);

#endif // LINKED_LIST_H