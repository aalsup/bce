#include "catch.hpp"

extern "C" {
#include <stdio.h>
#include <sqlite3.h>
#include "../dbutil.h"
#include "../data_model.h"
#include "../error.h"
};

TEST_CASE("create schema") {
    int rc;
    bce_error_t err = ERR_NONE;
    const char *database_file = "test/test1.db";

    // ensure database file does not exist
    remove(database_file);

    sqlite3 *conn = open_database(database_file, &rc);
    CHECK(rc == SQLITE_OK);

    int schema_version = get_schema_version(conn);
    CHECK(schema_version == 0);

    err = create_schema(conn);
    CHECK(err == ERR_NONE);

    remove(database_file);
}

TEST_CASE("load data") {
    int rc;
    const char *database_file = "test/test2.db";

    // ensure database file does not exist
    remove(database_file);

    sqlite3 *conn = open_database(database_file, &rc);
    CHECK(rc == SQLITE_OK);
    if (conn != NULL) {
        bce_error_t result = exec_sql_script(conn, "test/kubectl_data.sql");
        CHECK(result == ERR_NONE);
    }
}
