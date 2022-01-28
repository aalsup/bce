#ifndef SCHEMA_H
#define SCHEMA_H

#include <stdbool.h>
#include <sqlite3.h>

bool ensure_schema(struct sqlite3 *conn);
bool create_schema(struct sqlite3 *conn, int *result);
// TODO: migrate_schema
bool read_file_into_buffer(const char *filename, char **ppbuffer);
bool exec_sql_script(struct sqlite3 *conn, const char *filename);


#endif // SCHEMA_H
