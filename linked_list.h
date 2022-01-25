#include <stdbool.h>

typedef struct linked_list_node_t {
    void *data;
    struct linked_list_node_t *next;
} linked_list_node_t;

typedef struct linked_list_t {
    int size;
    linked_list_node_t *head;
} linked_list_t;

linked_list_t* ll_init();
bool ll_destroy(linked_list_t* list);
bool ll_append(linked_list_t* list, void* data);
