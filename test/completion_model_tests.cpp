#include "catch.hpp"
#include <string.h>

extern "C" {
#include <stdio.h>
#include <sqlite3.h>
#include "../schema.h"
#include "../completion_model.h"
};

TEST_CASE("create schema") {
    int rc;
    int err = 0;    // custom error values
    const char *database_file = "test/test1.db";

    // ensure database file does not exist
    remove(database_file);

    sqlite3 *conn = open_database(database_file, &rc);
    CHECK(rc == SQLITE_OK);

    bool schema_exists = ensure_schema(conn);
    CHECK(schema_exists == false);

    bool schema_created = create_schema(conn, &rc);
    CHECK(schema_created == true);

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
        bool result = exec_sql_script(conn, "test/kubectl_data.sql");
        CHECK(result == true);
    }
}
