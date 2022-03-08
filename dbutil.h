#ifndef BCE_DBUTIL_H
#define BCE_DBUTIL_H

#include <sqlite3.h>
#include <stdbool.h>
#include "error.h"

static const int SCHEMA_VERSION = 1;

sqlite3 *open_database(const char *filename, int *result);

int get_schema_version(struct sqlite3 *conn);

bce_error_t create_schema(struct sqlite3 *conn);

bool read_file_into_buffer(const char *filename, char **ppbuffer);

bce_error_t exec_sql_script(struct sqlite3 *conn, const char *filename);

#endif // BCE_DBUTIL_H
