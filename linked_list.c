#include "linked_list.h"
#include <stdlib.h>
#include <string.h>

static unsigned long node_id_seq = 1;

/*
 * Create a new linked list.
 * Allocates dynamic memory for the struct. Caller should use `ll_destroy()` when done.
 * If `free_node_func` is not NULL, this function will be called to free items from the list.
 */
linked_list_t *ll_create(void *(*free_func)(void *)) {
    linked_list_t *list = malloc(sizeof(linked_list_t));
    if (list) {
        list->size = 0;
        list->head = NULL;
        list->unique = false;
        list->free_node_func = free_func;
    }
    return list;
}

linked_list_t *ll_create_unique(void *(*free_func)(void *)) {
    linked_list_t *list = ll_create(free_func);
    if (list) {
        list->unique = true;
    }
    return list;
}

/*
 * Frees the memory used by the linked list and sets the list pointer = NULL.
 * free_node_func is a function pointer to perform any custom logic to free each node's data.
 * If free_node_func is NULL, the nodes' data will be reclaimed using `free(node->data)`.
 */
linked_list_t *ll_destroy(linked_list_t *list) {
    if (!list) {
        return NULL;
    }

    linked_list_node_t *node = list->head;
    while (node) {
        // free tbe node's data
        if (list->free_node_func) {
            node->data = list->free_node_func(node->data);
        } else {
            free(node->data);
        }
        node->data = NULL;
        // store the next node
        linked_list_node_t *next_node = node->next;
        // free the node
        free(node);
        // point to next
        node = next_node;
    }
    list->head = NULL;
    list->size = 0;
    free(list);
    return NULL;
}

/*
 * Appends a new node to the end of the list.
 */
bool ll_append_item(linked_list_t *list, const void *data) {
    if (list) {
        bool should_append_item = true;
        // if unique, check for item
        if (list->unique) {
            should_append_item = !ll_is_string_in_list(list, (const char *) data);
        }

        // create a new node
        if (should_append_item) {
            linked_list_node_t *node = malloc(sizeof(linked_list_node_t));
            if (node) {
                node->id = node_id_seq++;
                node->data = (void *) data;
                node->next = NULL;
                // find the end of the list
                linked_list_node_t *last = list->head;
                if (!last) {
                    list->head = node;
                } else {
                    while (last->next) {
                        last = last->next;
                    }
                    last->next = node;
                }
                list->size++;
                return true;
            }
        }
    }
    return false;
}

/*
 * Retrieves a particular data item from the list.
 */
void *ll_get_nth_item(const linked_list_t *list, size_t elem) {
    if (!list) {
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

/*
 * Creates a new linked list from the provided string.
 */
linked_list_t *ll_string_to_list(const char *str, const char *delim, size_t max_len) {
    linked_list_t *list = ll_create(NULL);

    char search_str[max_len + 1];
    memset(search_str, 0, max_len + 1);
    strncat(search_str, str, max_len);
    char *tok = strtok(search_str, delim);
    while (tok) {
        char *data = calloc(strlen(tok) + 1, sizeof(char));
        strncat(data, tok, strlen(tok));
        ll_append_item(list, data);
        tok = strtok(NULL, delim);
    }

    return list;
}

/*
 * Iterate over the items in a list, searching for a particular string.
 */
bool ll_is_string_in_list(const linked_list_t *list, const char *str) {
    if (!list || !str) {
        return false;
    }

    size_t len = strlen(str);
    for (linked_list_node_t *node = list->head; node != NULL; node = node->next) {
        const char *data = (char *) node->data;
        if (strncmp(str, data, len) == 0) {
            return true;
        }
    }
    return false;
}

/*
 * Iterate over a list searching for any matching string from the 2nd list
 */
bool ll_is_any_in_list(const linked_list_t *search_list, const linked_list_t *str_list) {
    if (!search_list || !str_list) {
        return false;
    }

    for (linked_list_node_t *node = str_list->head; node != NULL; node = node->next) {
        const char *str = (char *) node->data;
        if (ll_is_string_in_list(search_list, str)) {
            return true;
        }
    }
    return false;
}

/*
 * Removes a particular node from the list.
 */
bool ll_remove_item(linked_list_t *list, const linked_list_node_t *node_to_remove) {
    if (!list || !node_to_remove) {
        return false;
    }
    bool result = false;
    linked_list_node_t *prev_node = NULL;
    linked_list_node_t *node = list->head;
    while (node) {
        if (node->id == node_to_remove->id) {
            // free the node's data
            if (list->free_node_func) {
                list->free_node_func(node->data);
            } else {
                free(node->data);
            }
            node->data = NULL;
            // point prev -> next
            if (prev_node) {
                // remove node from middle
                prev_node->next = node->next;
            } else {
                // remove node from head
                list->head = node->next;
            }
            // free the node
            free(node);
            node = NULL;
            list->size--;
            result = true;
            break;
        }
        prev_node = node;
        node = node->next;
    }
    return result;
}