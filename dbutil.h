#ifndef DBUTIL_H
#define DBUTIL_H

#include <stdbool.h>
#include <sqlite3.h>

static const int SCHEMA_VERSION = 1;

sqlite3* open_database(const char *filename, int *result);
int get_schema_version(struct sqlite3 *conn);
bool create_schema(struct sqlite3 *conn, int *result);
bool read_file_into_buffer(const char *filename, char **ppbuffer);
bool exec_sql_script(struct sqlite3 *conn, const char *filename);

#endif // DBUTIL_H
