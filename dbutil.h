#ifndef DBUTIL_H
#define DBUTIL_H

#include <stdbool.h>
#include <sqlite3.h>

// TODO: migrate old to new schema, using `PRAGMA user_version`

bool ensure_schema(struct sqlite3 *conn);
bool create_schema(struct sqlite3 *conn, int *result);
bool read_file_into_buffer(const char *filename, char **ppbuffer);
bool exec_sql_script(struct sqlite3 *conn, const char *filename);

#endif // DBUTIL_H
