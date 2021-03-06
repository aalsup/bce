#define CATCH_CONFIG_MAIN

#include "catch.hpp"

extern "C" {
#include "../linked_list.h"
};

TEST_CASE("LinkedList create") {
    linked_list_t *list = ll_create(NULL);
    CHECK(list != NULL);
    CHECK(list->size == 0);
    CHECK(list->head == NULL);
}

TEST_CASE("LinkedList ops") {
    linked_list_t *list = ll_create(NULL);

    SECTION("append string") {
        bool retval;
        const char *data = "This is a test";
        retval = ll_append_item(list, (char *) data);
        CHECK(retval == true);
        CHECK(list->size == 1);
        CHECK(strncmp((char *) list->head->data, data, strlen(data)) == 0);
    }

    SECTION("append strings") {
        bool retval;
        const char *str1 = (char *) "some data";
        retval = ll_append_item(list, str1);
        CHECK(retval == true);
        const char *str2 = (char *) "more data";
        retval = ll_append_item(list, str2);
        CHECK(retval == true);
        char *str3 = (char *) "a little bit more";
        retval = ll_append_item(list, str3);
        CHECK(retval == true);
        CHECK(list->size == 3);
        CHECK(strncmp((char *) list->head->data, str1, strlen(str1)) == 0);
    }
}

TEST_CASE("LinkedList destroy") {
    bool retval;
    linked_list_t *list = ll_create(NULL);
    // allocate a dynamic variable
    char *data = (char *) calloc(10, sizeof(char));
    strcat(data, "yada yada");
    ll_append_item(list, data);
    CHECK(list->size == 1);
    list = ll_destroy(list);
    CHECK(list == NULL);
}

