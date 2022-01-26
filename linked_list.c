#include "linked_list.h"
#include <stdlib.h>
#include <string.h>

linked_list_t* ll_create() {
    linked_list_t* list = malloc(sizeof(linked_list_t));
    if (list != NULL) {
        list->size = 0;
        list->head = NULL;
    }
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
        if (node != NULL) {
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
    }
    return false;
}

void* ll_get_nth_element(const linked_list_t* list, int elem) {
    if (list == NULL) {
        return NULL;
    }
    if (elem < 0) {
        return NULL;
    }
    if (elem > (list->size - 1)) {
        return NULL;
    }
    linked_list_node_t *node = list->head;
    for (int i = 0; i < elem; i++) {
        node = node->next;
    }
    return node->data;
}

linked_list_t *string_to_list(const char *str, const char *delim, size_t max_len) {
    linked_list_t *list = ll_create();

    char search_str[max_len + 1];
    memset(search_str, 0, max_len + 1);
    strncpy(search_str, str, max_len);
    char *tok = strtok(search_str, delim);
    while (tok != NULL) {
        char *data = calloc(strlen(tok) + 1, sizeof(char));
        strncpy(data, tok, strlen(tok));
        ll_append(list, data);
        tok = strtok(NULL, delim);
    }

    return list;
}

