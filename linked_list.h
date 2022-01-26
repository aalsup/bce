#include <stdlib.h>
#include <stdbool.h>

#ifndef LINKED_LIST_H
#define LINKED_LIST_H

typedef struct linked_list_node_t {
    void *data;
    struct linked_list_node_t *next;
} linked_list_node_t;

typedef struct linked_list_t {
    size_t size;
    linked_list_node_t *head;
} linked_list_t;

linked_list_t* ll_create();
bool ll_destroy(linked_list_t **list);
bool ll_append(linked_list_t *list, void *data);
void* ll_get_nth_element(const linked_list_t* list, size_t elem);
linked_list_t *string_to_list(const char *str, const char *delim, size_t max_len);

#endif // LINKED_LIST_H