#include "linked_list.h"
#include <stdlib.h>

linked_list_t* ll_init() {
    linked_list_t* list = malloc(sizeof(linked_list_t));
    list->size = 0;
    list->head = NULL;
    return list;
}

bool ll_destroy(linked_list_t* list) {
    if (list != NULL) {
        linked_list_node_t* node = list->head;
        while (node != NULL) {
            free(node->data);
            node->data = NULL;
            node = node->next;
        }
        list->head = NULL;
        list->size = 0;
    }
    return true;
}

bool ll_append(linked_list_t* list, void* data) {
    if (list != NULL) {
        // create a new node
        linked_list_node_t *node = malloc(sizeof(linked_list_node_t));
        node->data = data;
        node->next = NULL;
        // find the end of the list
        linked_list_node_t *last = list->head;
        if (last == NULL) {
            list->head = node;
        } else {
            while (last->next != NULL) {
                last = last->next;
            }
            last->next = node;
        }
        list->size++;
        return true;
    }
    return false;
}

