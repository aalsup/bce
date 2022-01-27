#include "linked_list.h"
#include <stdlib.h>
#include <string.h>

static int node_id_seq = 1;

linked_list_t* ll_create() {
    linked_list_t* list = malloc(sizeof(linked_list_t));
    if (list != NULL) {
        list->size = 0;
        list->head = NULL;
    }
    return list;
}

bool ll_destroy(linked_list_t **pplist) {
    if (pplist != NULL) {
        linked_list_t *list = *pplist;
        linked_list_node_t* node = list->head;
        while (node != NULL) {
            free(node->data);
            node->data = NULL;
            node = node->next;
        }
        list->head = NULL;
        list->size = 0;
        free(list);
        *pplist = NULL;
    }
    return true;
}

bool ll_append_item(linked_list_t* list, void* data) {
    if (list != NULL) {
        // create a new node
        linked_list_node_t *node = malloc(sizeof(linked_list_node_t));
        if (node != NULL) {
            node->id = node_id_seq++;
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

void* ll_get_nth_item(const linked_list_t* list, size_t elem) {
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

linked_list_t *ll_string_to_list(const char *str, const char *delim, size_t max_len) {
    linked_list_t *list = ll_create();

    char search_str[max_len + 1];
    memset(search_str, 0, max_len + 1);
    strncat(search_str, str, max_len);
    char *tok = strtok(search_str, delim);
    while (tok != NULL) {
        char *data = calloc(strlen(tok) + 1, sizeof(char));
        strncat(data, tok, strlen(tok));
        ll_append_item(list, data);
        tok = strtok(NULL, delim);
    }

    return list;
}

bool ll_is_string_in_list(const linked_list_t* list, const char *str) {
    if ((list != NULL) && (str != NULL)) {
        size_t len = strlen(str);
        linked_list_node_t *node = list->head;
        while (node != NULL) {
            const char *data = (char *)node->data;
            if (strncmp(str, data, len) == 0) {
                return true;
            }
            node = node->next;
        }
    }
    return false;
}

bool ll_remove_item(linked_list_t *list, linked_list_node_t *node_to_remove) {
    bool result = false;
    if (list != NULL) {
        linked_list_node_t *prev_node = NULL;
        linked_list_node_t *node = list->head;
        while (node != NULL) {
            if (node->id == node_to_remove->id) {
                // free the node
                free(node->data);
                node->data = NULL;
                // point prev -> next
                if (prev_node != NULL) {
                    // remove node from middle
                    prev_node->next = node->next;
                } else {
                    // remove node from head
                    list->head = node->next;
                }
                list->size--;
                result = true;
                break;
            }
            prev_node = node;
            node = node->next;
        }
    }
    return result;
}

