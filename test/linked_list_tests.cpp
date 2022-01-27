#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include <string.h>

extern "C" {
#include "../linked_list.h"
};

TEST_CASE("LinkedList create") {
    linked_list_t *list = ll_create();
    CHECK(list != NULL);
    CHECK(list->size == 0);
    CHECK(list->head == NULL);
}

TEST_CASE("LinkedList ops") {
    linked_list_t *list = ll_create();

    SECTION("append string") {
        bool retval;
        char *data = "This is a test";
        retval = ll_append_item(list, data);
        CHECK(retval == true);
        CHECK(list->size == 1);
        CHECK(strncmp((char *) list->head->data, data, strlen(data)) == 0);
    }

    SECTION("append strings") {
        bool retval;
        char *str1 = "some data";
        retval = ll_append_item(list, str1);
        CHECK(retval == true);
        char *str2 = "more data";
        retval = ll_append_item(list, str2);
        CHECK(retval == true);
        char *str3 = "a little bit more";
        retval = ll_append_item(list, str3);
        CHECK(retval == true);
        CHECK(list->size == 3);
        CHECK(strncmp((char *) list->head->data, str1, strlen(str1)) == 0);
    }
}

TEST_CASE("LinkedList destroy") {
    bool retval;
    linked_list_t *list = ll_create();
    retval = ll_destroy(&list);
    CHECK(retval == true);
    CHECK(list == NULL);
}

