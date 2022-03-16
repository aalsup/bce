#ifndef BCE_DBUTIL_H
#define BCE_DBUTIL_H

#include <sqlite3.h>
#include <stdbool.h>
#include "error.h"

sqlite3 *db_open(const char *filename, int *result);

sqlite3 *db_open_with_xa(const char *filename, int *result);

int db_get_schema_version(struct sqlite3 *conn);

bce_error_t db_create_schema(struct sqlite3 *conn);

bool read_file_into_buffer(const char *filename, char **ppbuffer);

bce_error_t db_exec_sql_script(struct sqlite3 *conn, const char *filename);

#endif // BCE_DBUTIL_H
