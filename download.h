#ifndef BCE_DOWNLOAD_H
#define BCE_DOWNLOAD_H

#include <stdbool.h>

bool file_exists(const char *filename);

bool download_file(const char *url, const char *filename);

#endif // BCE_DOWNLOAD_H
